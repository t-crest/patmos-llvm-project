
#include "PreRegallocReduce.h"
#include "Patmos.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineFunction.h"

using namespace llvm;

#define DEBUG_TYPE "patmos-singlepath"

char PreRegallocReduce::ID = 0;

FunctionPass *llvm::createPreRegallocReduce(const PatmosTargetMachine &tm) {
  return new PreRegallocReduce(tm);
}

bool PreRegallocReduce::runOnMachineFunction(MachineFunction &MF) {
	bool changed = false;
	// only convert function if marked
	if ( MF.getInfo<PatmosMachineFunctionInfo>()->isSinglePath()) {
		LLVM_DEBUG(
			dbgs() << "[Single-Path] Reducing " << MF.getFunction().getName() << "\n" ;
			MF.dump();
		);

		doReduceFunction(MF);
		MF.getProperties().reset(MachineFunctionProperties::Property::IsSSA);
		MF.getProperties().reset(MachineFunctionProperties::Property::NoVRegs);
		changed |= true;

		LLVM_DEBUG(
			dbgs() << "[Single-Path] Reducing after " << MF.getFunction().getName() << "\n" ;
			MF.dump();
		);
	}
	return changed;
}

static Register getVregForPred(
		MachineRegisterInfo &RI, unsigned pred,
		std::map<unsigned, Register>& vreg_map)
{
	if (vreg_map.count(pred)) {
		return vreg_map[pred];
	} else {
		auto new_vreg = createVirtualRegisterWithHint(RI);
		vreg_map[pred] = new_vreg;
		return new_vreg;
	}
}

void PreRegallocReduce::doReduceFunction(MachineFunction &MF){
	// Assign each predicate to a virtual predicate register
	std::map<unsigned, Register> vreg_map;

	applyPredicates(&MF, vreg_map);

	insertPredDefinitions(&MF, vreg_map);
}

void PreRegallocReduce::applyPredicates(MachineFunction *MF, std::map<unsigned, Register>& vreg_map) {

	auto &EQ = getAnalysis<EquivalenceClasses>();
	auto &RI = MF->getRegInfo();

	std::for_each(MF->begin(), MF->end(), [&](auto &mbb){
		auto block_pred = EQ.getClassFor(&mbb).number;
		auto predVreg = getVregForPred(RI, block_pred, vreg_map);

		LLVM_DEBUG(
			dbgs() << "Applying predicates in bb." << mbb.getNumber() << "." << mbb.getName() << ": "
					<< block_pred << " -> " << printReg(predVreg, TRI, 0, &mbb.getParent()->getRegInfo()) <<"\n";
		);

		// apply predicate to all instructions in block
		for( auto MI = mbb.instr_begin(); MI != mbb.getFirstInstrTerminator(); ++MI)
		{
			if (MI->isReturn()) {
				LLVM_DEBUG( dbgs() << "    skip return: " << *MI );
				continue;
			}
			if (TII->isStackControl(&*MI)) {
				LLVM_DEBUG( dbgs() << "    skip stack control: " << *MI );
				continue;
			}
			if (MI->getFlag(MachineInstr::FrameSetup)) {
				continue;
				LLVM_DEBUG(dbgs() << "    skip frame setup: " << *MI);
			}

			if (MI->isCall() && !PatmosSinglePathInfo::isPseudoRoot(*getCallTargetMF(&*MI))) {
				LLVM_DEBUG( dbgs() << "    call: " << *MI );
				assert(!TII->isPredicated(*MI) && "call predicated");
				DebugLoc DL = MI->getDebugLoc();
				// copy actual preg to temporary preg
				// (this is the predicate call argument which enables/disabled the whole function)
				BuildMI(mbb, MI, DL,
					TII->get(Patmos::COPY), Patmos::P7)
				.addReg(predVreg);
				continue;
			}

			if (MI->isPredicable(MachineInstr::QueryType::IgnoreBundle) ) {
				int pred_op_idx = MI->findFirstPredOperandIdx();

				if(pred_op_idx!=-1 && MI->getOperand(pred_op_idx).getReg() == Patmos::NoRegister) {
					assert(pred_op_idx != -1);

					MachineOperand &PO1 = MI->getOperand(pred_op_idx);
					MachineOperand &PO2 = MI->getOperand(pred_op_idx+1);
					assert(PO1.isReg() && PO2.isImm() &&
									 "Unexpected Patmos predicate operand");
					PO1.setReg(predVreg);
				} else if(pred_op_idx!=-1 && MI->getOperand(pred_op_idx).getReg().isVirtual() ){
					// Instruction already predicated but not from single-path.
					// This can come from the "select" llvm instruction.
					// We must ensure the predicate depends on whether the block is
					// enabled in addition to the original condition.

					auto new_vreg = createVirtualRegisterWithHint(RI);
					BuildMI(mbb, MI, DebugLoc(),
						TII->get(Patmos::PAND), new_vreg)
							.addReg(Patmos::P0).addImm(0) // Don't predicate
							.addReg(predVreg).addImm(0)
							.addReg(MI->getOperand(pred_op_idx).getReg()).addImm(0);
					MI->getOperand(pred_op_idx).setReg(new_vreg);
				}
			}
		}
	});
}

static bool is_exit_edge(std::pair<MachineBasicBlock*, MachineBasicBlock*> edge, MachineLoopInfo &LI) {
	auto source_loop = LI.getLoopFor(edge.first);
	SmallVector<std::pair<MachineBasicBlock*, MachineBasicBlock*>, 4> exit_edges;
	if(source_loop) {
		source_loop->getExitEdges(exit_edges);
	}
	return std::count(exit_edges.begin(), exit_edges.end(), edge) != 0;
}

/// Gets the header of the given block's loop.
/// If the block is not in a loop, the entry block is its header.
MachineBasicBlock* get_header_of(MachineBasicBlock*mbb, MachineLoopInfo&LI) {
	auto entry = &*mbb->getParent()->begin();
	auto loop = LI.getLoopFor(mbb);
	return loop? loop->getHeader() : entry;
}

void PreRegallocReduce::insertPredDefinitions(
		MachineFunction*MF, std::map<unsigned, Register>& vreg_map
) {
	auto &EQ = getAnalysis<EquivalenceClasses>();
	MachineBasicBlock *entry = &*MF->begin();
	auto &RI = MF->getRegInfo();
	auto &LI = getAnalysis<MachineLoopInfo>();

	for(auto eq_class: EQ.getAllClasses()) {
		assert(eq_class.members.size() >= 1);
		assert(eq_class.dependencies.size() >= 1);

		auto class_header = get_header_of(*eq_class.members.begin(), LI);

		bool has_definition_in_class_header = false;
		bool has_non_header_exit = false;
		bool is_header_class = std::any_of(eq_class.members.begin(), eq_class.members.end(), [&](auto member){ return LI.isLoopHeader(member) || member == entry;});
		bool has_multiple_real_dep_edges = eq_class.dependencies.size() > (eq_class.dependencies.count(None) + 1);
		bool non_destructive_def = has_multiple_real_dep_edges && !is_header_class;

		for(auto dep_edge: eq_class.dependencies) {
			if(!dep_edge) {
				auto initial_preg = PatmosSinglePathInfo::isRootLike(*MF)? Patmos::P0 : Patmos::P7;

				auto pred_vreg = getVregForPred(RI, eq_class.number, vreg_map);
				auto def = BuildMI(*entry, entry->begin(), DebugLoc(),
					TII->get(Patmos::COPY), pred_vreg)
						.addReg(initial_preg);
				LLVM_DEBUG(
					dbgs() << "Inserted entry definition of predicate " << eq_class.number << ":\n\t";
					def.getInstr()->dump();
				);
				has_definition_in_class_header = true;
			} else {
				auto source_vreg = getVregForPred(RI, EQ.getClassFor(dep_edge->first).number, vreg_map);

				auto target_vreg = getVregForPred(RI, eq_class.number, vreg_map);
				auto number_or_terminators = std::distance(dep_edge->first->getFirstTerminator(), dep_edge->first->end());

				if(!eq_class.members.count(dep_edge->second)) {
					// The edge doesn't go straight into the class.
					// Therefore, copy the target's class's predicate into this class'es predicate
					auto sink_vreg = getVregForPred(RI, EQ.getClassFor(dep_edge->second).number, vreg_map);

					MachineInstrBuilder def;
					if(non_destructive_def) {
						def = BuildMI(*dep_edge->second, dep_edge->second->getFirstTerminator(), DebugLoc(),
							TII->get(Patmos::PSET), target_vreg)
								.addReg(sink_vreg).addImm(0)
								.addUse(target_vreg, RegState::Implicit);
					} else {
						def = BuildMI(*dep_edge->second, dep_edge->second->getFirstTerminator(), DebugLoc(),
							TII->get(Patmos::COPY), target_vreg)
								.addReg(sink_vreg);
					}
					LLVM_DEBUG(
						dbgs() << "Inserted definition of predicate " << eq_class.number << " with non-member target "
							<< "in bb." << dep_edge->second->getNumber() << "." << dep_edge->second->getName() <<":\n\t";
						def.getInstr()->dump();
					);
				} else {
					assert(
						number_or_terminators >= 1 &&
						dep_edge->first->getFirstTerminator()->getOpcode() == Patmos::BR &&
						dep_edge->first->getFirstTerminator()->getNumExplicitOperands() == 3 &&
						"Control dependency edge not sourced in a conditional branch"
					);
					// First terminator is conditional branch, second is unconditional

					// Extract condition
					auto *cond_branch = &*dep_edge->first->getFirstTerminator();
					auto target = PatmosInstrInfo::getBranchTarget(cond_branch);
					auto cond_reg = cond_branch->getOperand(0).getReg();
					assert(cond_reg != Patmos::NoRegister && cond_reg != Patmos::P0);
					auto cond_neg = cond_branch->getOperand(1).getImm() != 0;

					// If the conditional branch doesn't target our target, negate the condition
					cond_neg = target != dep_edge->second? !cond_neg: cond_neg;

					auto target_header = get_header_of(dep_edge->second, LI);

					// We Need to initialize the predicate of an exit target to false in the header of the target
					// (unless the target and header are in the same class, since the header needs to initially be
					// enabled)
					assert((target_header == dep_edge->second? eq_class.members.count(target_header) : true)
						&& "If the target of a definition is a header, it must be in the class"
					);
					bool is_non_header_exit = is_exit_edge(*dep_edge, LI) && !eq_class.members.count(target_header);
					has_non_header_exit |= is_non_header_exit;

					bool is_def_in_class_header = class_header == dep_edge->first;
					has_definition_in_class_header |= is_def_in_class_header;

					bool needs_implicit_use = false;
					MachineInstrBuilder def;
					if(non_destructive_def) {
						assert(!is_non_header_exit);
						def = BuildMI(*dep_edge->first, dep_edge->first->getFirstTerminator(), DebugLoc(),
							TII->get(Patmos::PMOV), target_vreg)
								.addReg(source_vreg).addImm(0); // Only update predicate if current block is enabled
						needs_implicit_use = true;
					} else {
						def = BuildMI(*dep_edge->first, dep_edge->first->getFirstTerminator(), DebugLoc(),
							TII->get(Patmos::PAND), target_vreg)
								// If this edge exits the loop, only allow the predicate to be written
								// once (when the exit is taken). This ensures subsequent, disabled runs
								// of the loop don't overwrite the exit target's register
								// (to false, even though it should be true)
								// By using the target predicate as the negated guard, after the first
								// time the exit is taken, the predicate cannot be assigned to again
								// Remember: exit predicates are initialized to false unless their class includes the header
								.addReg(is_non_header_exit? target_vreg : Patmos::P0).addImm(is_non_header_exit?1:0)
								.addReg(source_vreg).addImm(0); // current guard as source
						needs_implicit_use = is_non_header_exit;
					}
					def.addReg(cond_reg).addImm(cond_neg? 1 : 0); // condition

					// We add implicit uses of the target vreg to ensure the register allocator knows that
					// the previous value is live, since it may not be changed. Without this LLVM would
					// think previous values are dead because it doesn't know that the instruction is predicated.
					// It will then delete previous definitions or reassign them to registers that are used by other
					// predicates.
					if(needs_implicit_use && !is_def_in_class_header) {
						def.addUse(target_vreg, RegState::Implicit);
					}
					LLVM_DEBUG(
						dbgs() << "Inserted definition of predicate " << eq_class.number
							<< " in bb." << dep_edge->first->getNumber() << "." << dep_edge->first->getName()
							<< " for bb." << dep_edge->second->getNumber() << "." << dep_edge->second->getName() << ":\n\t";
						def.getInstr()->dump();
					);
				}
			}
		}

		if((non_destructive_def || has_non_header_exit) && !has_definition_in_class_header) {
			auto loop = LI.getLoopFor(*eq_class.members.begin());
			assert(std::all_of(eq_class.members.begin(), eq_class.members.end(), [&](auto member){
				return LI.getLoopFor(member) == loop;
			}) && "All equivalence class members aren't in the same loop");
			auto class_header = loop? loop->getHeader(): entry;

			// Set predicate to false at the start of the scope (header)
			auto def = BuildMI(*class_header, class_header->getFirstTerminator(), DebugLoc(),
				TII->get(Patmos::PCLR), getVregForPred(RI, eq_class.number, vreg_map))
					.addReg(Patmos::P0).addImm(0); // Never predicated
			LLVM_DEBUG(
				dbgs() << "Inserted predicate " << eq_class.number << " initial clear "
					<< "in bb." << class_header->getNumber() << "." << class_header->getName() <<":\n\t";
				def.getInstr()->dump();
			);
		}


	}
}
