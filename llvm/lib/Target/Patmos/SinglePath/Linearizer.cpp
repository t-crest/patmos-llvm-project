#include "Linearizer.h"
#include "Patmos.h"
#include "SinglePath/PatmosSPReduce.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/Statistic.h"

using namespace llvm;

#define DEBUG_TYPE "patmos-singlepath"

char Linearizer::ID = 0;

FunctionPass *llvm::createSinglePathLinearizer(const PatmosTargetMachine &tm) {
  return new Linearizer(tm);
}

/// Returns the preheader and unilatch of the loop inserted by LoopCountInsert
///
/// The preheader is the single predecessors of the loop.
/// The unilatch is the block all latches have an edge to instead of the header directly
static std::pair<MachineBasicBlock *, MachineBasicBlock *> getPreHeaderUnilatch(MachineLoop *loop)
{
	auto header = loop->getHeader();
	assert(header->pred_size() == 2
			&& "Loops headers should only have a preheader and unilatch as predecessors");
	if(loop->contains(*header->pred_begin())) {
		return std::make_pair(*std::next(header->pred_begin()), *header->pred_begin());
	} else {
		return std::make_pair(*header->pred_begin(), *std::next(header->pred_begin()));
	}
}

bool Linearizer::runOnMachineFunction(MachineFunction &MF) {
	bool changed = false;
	// only convert function if marked
	if ( MF.getInfo<PatmosMachineFunctionInfo>()->isSinglePath()) {
		auto RootScope = getAnalysis<PatmosSinglePathInfo>().getRootScope();
		LLVM_DEBUG(
			dbgs() << "[Single-Path] Linearizing " << MF.getFunction().getName() << "\n";
		);

		auto &LI = getAnalysis<MachineLoopInfo>();
		// Update unilatch to use the counter as condition (instead of unconditionally branching
		std::for_each(MF.begin(), MF.end(), [&](auto &header_mbb){
			if(LI.isLoopHeader(&header_mbb)) {
				DebugLoc DL;
				auto loop = LI.getLoopFor(&header_mbb);

				auto unilatch = getPreHeaderUnilatch(loop).second;

				// Insert counter check in unilatch
				auto is_dec = [](MachineInstr &instr){
					return instr.getOpcode() == Patmos::SUBi;
				};
				assert(std::count_if(unilatch->begin(), unilatch->end(), is_dec) == 1
						&& "Couldn't find unilatch unique counter decrementer");
				auto counter_decrementer = std::find_if(unilatch->begin(), unilatch->end(), is_dec);
				auto decremented_count_reg = counter_decrementer->getOperand(3).getReg();
				assert(Patmos::RRegsRegClass.contains(decremented_count_reg)
						&& "Loop counter not using general-purpose register");

				auto counter_check_vreg = MF.getRegInfo().createVirtualRegister(&Patmos::PRegsRegClass);
				MF.getRegInfo().setSimpleHint(counter_check_vreg, Patmos::P1);
				BuildMI(*unilatch, unilatch->getFirstInstrTerminator(), DL,
					TII->get(Patmos::CMPLT), counter_check_vreg)
					.addReg(Patmos::P0).addImm(0)
					.addReg(Patmos::R0).addReg(decremented_count_reg);

				// Replace branch with back edge
				unilatch->remove(&*unilatch->getFirstInstrTerminator());
				BuildMI(*unilatch, unilatch->getFirstInstrTerminator(), DL,
					TII->get(Patmos::BR))
					.addReg(counter_check_vreg).addImm(0)
					.addMBB(&header_mbb);
			}
		});

		linearizeScope(getAnalysis<PatmosSinglePathInfo>().getRootScope());
		mergeMBBs(MF);

		// Clear all flags from general purpose register operands since they might not be correct any more.
		// E.g. if a register is killed on one path, it might not be so on the other paths. Since we are now
		// in single-path code, the kill would interfere with a live register from other paths.
		// The only flag we keep is the "implicit", since we need it for calls etc.
		// We also set the "undef" for all registers to ensure we don't get "using undefined register" errors.
		std::for_each(MF.begin(), MF.end(), [&](auto &mbb){
			std::for_each(mbb.begin(), mbb.end(), [&](MachineInstr &instr){
				for(auto &op: instr.operands()) {
					if(op.isReg() && op.isUse() && Patmos::RRegsRegClass.contains(op.getReg())) {
						op.ChangeToRegister(op.getReg(), false, op.isImplicit(), false, false, true, false);
					}
				}
			});
		});

		assert(MF.verify());

		LLVM_DEBUG(
			dbgs() << "[Single-Path] Linearizing end " << MF.getFunction().getName() << "\n";
		);
		changed |= true;
	}
	return changed;
}


MachineBasicBlock* Linearizer::linearizeScope(SPScope *S, MachineBasicBlock* last_block)
{
	auto blocks = S->getBlocksTopoOrd();

	for(auto block: blocks){
	    auto MBB = block->getMBB();
	    if (S->isSubheader(block)) {
			last_block = linearizeScope(S->findScopeOf(block), last_block);


	    } else {
			if(MBB->succ_size() == 1 && *MBB->succ_begin() == S->getHeader()->getMBB()) {
				// This is the unilatch block, don't mess with branches
			} else {
				// remove all successors
				for ( MachineBasicBlock::succ_iterator SI = MBB->succ_begin();
								SI != MBB->succ_end();
								SI = MBB->removeSuccessor(SI) )
								; // no body

				// remove the branch at the end of MBB
				TII->removeBranch(*MBB);
			}

			if (last_block) {
				// add to the last MBB as successor
				last_block->addSuccessor(MBB);
				// move in the code layout
				MBB->moveAfter(last_block);
			}
			// keep track of tail
			last_block = MBB;
	    }
	}
	return last_block;
}

void Linearizer::mergeMBBs(MachineFunction &MF) {
	LLVM_DEBUG( dbgs() << "Function before block merge:\n"; MF.dump() );
	// first, obtain the sequence of MBBs in DF order (as copy!)
	// NB: have to use the version below, as some version of libcxx will not
	// compile it (similar to
	//    http://lists.cs.uiuc.edu/pipermail/cfe-commits/Week-of-Mon-20130325/076850.html)
	//std::vector<MachineBasicBlock*> order(df_begin(&MF.front()),
	//                                      df_end(  &MF.front()));
	std::vector<MachineBasicBlock*> order;
	for (auto I = df_begin(&MF.front()), E = df_end(&MF.front()); I != E; ++I) {
		order.push_back(*I);
	}

	auto I = order.begin(), E = order.end();

	MachineBasicBlock *BaseMBB = *I;
	LLVM_DEBUG( dbgs() << "Base MBB#" << BaseMBB->getNumber() << "\n" );
	// iterate through order of MBBs
	while (++I != E) {
		// get MBB of iterator
		MachineBasicBlock *MBB = *I;

		if (BaseMBB->succ_size() > 1) {
			// we have encountered a backedge

			// Create an unconditional branch to the next block to exit the loop
			DebugLoc DL;
			BuildMI(*BaseMBB, BaseMBB->end(), DL,
				TII->get(Patmos::BRu))
				.addReg(Patmos::P0).addImm(0)
				.addMBB(MBB);

			BaseMBB = MBB;
			LLVM_DEBUG( dbgs() << "Base MBB#" << BaseMBB->getNumber() << "\n" );
		} else if (MBB->pred_size() == 1) {
			LLVM_DEBUG( dbgs() << "  Merge MBB#" << MBB->getNumber() << "\n" );
			// transfer the instructions
			BaseMBB->splice(BaseMBB->end(), MBB, MBB->begin(), MBB->end());
			// Any PHIs in MBB's successors are redirected to base
			std::for_each(MBB->succ_begin(), MBB->succ_end(), [&](auto succ){
				succ->replacePhiUsesWith(MBB, BaseMBB);
			});
			// remove the edge between BaseMBB and MBB
			BaseMBB->removeSuccessor(MBB);
			// BaseMBB gets the successors of MBB instead
			BaseMBB->transferSuccessors(MBB);

			// remove MBB from MachineFunction
			MF.erase(MBB);
		} else {
			BaseMBB = MBB;
			LLVM_DEBUG( dbgs() << "Base MBB#" << BaseMBB->getNumber() << "\n" );
		}
	}
	// invalidate order
	order.clear();
}

