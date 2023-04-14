
#include "LoopCountInsert.h"
#include "Patmos.h"
#include "SinglePath/PatmosSPReduce.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "patmos-singlepath"

char LoopCountInsert::ID = 0;

FunctionPass *llvm::createLoopCountInsert(const PatmosTargetMachine &tm) {
  return new LoopCountInsert(tm);
}

bool LoopCountInsert::runOnMachineFunction(MachineFunction &MF) {
	bool changed = false;
	// only convert function if marked
	if ( MF.getInfo<PatmosMachineFunctionInfo>()->isSinglePath()) {

		LLVM_DEBUG(
			dbgs() << "[Single-Path] Loop Counter Insertion " << MF.getFunction().getName() << "\n" ;
			MF.dump();
		);

		// Ensure all branches are explicit, makes it easier to work with.
		remove_fallthroughs(MF);
		LLVM_DEBUG(
			dbgs() << "[Single-Path] Loop Counter Insertion (explicit branches) " << MF.getFunction().getName() << "\n" ;
			MF.dump();
		);
		doFunction(MF);
		MF.RenumberBlocks();
		changed |= true;

		LLVM_DEBUG(
			dbgs() << "[Single-Path] Loop Counter Insertion End " << MF.getFunction().getName() << "\n" ;
			MF.dump();
		);
	}
	return changed;
}

void LoopCountInsert::remove_fallthroughs(MachineFunction &MF)
{
	std::for_each(MF.begin(), MF.end(), [&](MachineBasicBlock &mbb){
		if(TII->mayFallthrough(mbb)) {
			auto target = mbb.getFallThrough();

			BuildMI(mbb, mbb.end(), DebugLoc(),
				TII->get(Patmos::BRu))
					.addReg(Patmos::NoRegister).addImm(0)
					.addMBB(target);
		}
	});
}

void LoopCountInsert::doFunction(MachineFunction &MF){
	auto &LI = getAnalysis<MachineLoopInfo>();

	std::for_each(MF.begin(), MF.end(), [&](auto &header_mbb){

		if(LI.isLoopHeader(&header_mbb)) {
			auto loop = LI.getLoopFor(&header_mbb);
			auto loop_bounds = getLoopBounds(&header_mbb);
			if(!loop_bounds) {
				report_fatal_error(
					  "Single-path code generation failed! "
					  "Loop has no bound. Loop bound expected in the following MBB but was not found: '" +
					  header_mbb.getName() + "'!");
			}
			auto upper_bound = loop_bounds->second;
			DebugLoc DL;

			// Start my making a preheader that will manage the initial counter
			MachineBasicBlock *preheader = MF.CreateMachineBasicBlock();
			auto header_mbb_position = std::find_if(MF.begin(), MF.end(), [&](auto &mbb){ return &mbb == &header_mbb;});
			// We put the preheader before the header and unilatch (added below)
			assert(header_mbb_position != MF.end());
			MF.insert(header_mbb_position, preheader);

			// Redirect loop predecessors
			for(auto pred_iter = header_mbb.pred_begin(); pred_iter != header_mbb.pred_end();)
			{
				auto pred = *pred_iter;
				if(loop->contains(pred)) {
					// Not loop predecessor
					pred_iter++;
				} else {
					(*pred_iter)->ReplaceUsesOfBlockWith(&header_mbb, preheader);

					pred_iter = header_mbb.pred_begin();
				}
			}

			// We now create a new block (unified latch) that will handle the loop iteration.
			// Each back edge is redirected to it.
			MachineBasicBlock *unilatch = MF.CreateMachineBasicBlock();
			header_mbb_position = std::find_if(MF.begin(), MF.end(), [&](auto &mbb){ return &mbb == &header_mbb;});
			// We put the unilatch before the header
			assert(header_mbb_position != MF.end());
			MF.insert(header_mbb_position, unilatch);

			// Update header PHIs
			// Any PHI inputs in the header that come from a loop predecessor must be moved to the preheader
			// Any PHI inputs from a latch must be moved to the unilatch
			for(auto instr_iter = header_mbb.begin(); instr_iter != header_mbb.end(); instr_iter++){
				auto &instr = *instr_iter;
				if(instr.isPHI()) {
					auto *reg_class = MF.getRegInfo().getRegClass(instr.getOperand(0).getReg());
					// Create PHI in preheader and unilatch to take over the operands
					Register preheader_replacement_vreg;
					if(reg_class == &Patmos::PRegsRegClass) {
						preheader_replacement_vreg = createVirtualRegisterWithHint(MF.getRegInfo());
					} else {
						preheader_replacement_vreg = MF.getRegInfo().createVirtualRegister(reg_class);
					}
					auto preheader_replacement_phi = BuildMI(*preheader, preheader->end(), DL,
						TII->get(Patmos::PHI), preheader_replacement_vreg);
					auto branch_replacement_vreg = MF.getRegInfo().createVirtualRegister(reg_class);
					auto branch_replacement_phi = BuildMI(*unilatch, unilatch->end(), DL,
						TII->get(Patmos::PHI), branch_replacement_vreg);

					while(instr.getNumOperands() > 1 ) {
						auto phi_pred = instr.getOperand(2).getMBB();
						auto phi_pred_op = instr.getOperand(1);
						if(loop->contains(phi_pred)) {
							assert(loop->isLoopLatch(phi_pred));
							branch_replacement_phi.add(phi_pred_op).addMBB(phi_pred);
						} else {
							preheader_replacement_phi.add(phi_pred_op).addMBB(phi_pred);
						}
						instr.RemoveOperand(2);
						instr.RemoveOperand(1);
					}
					// Add new phi vregs in old phi
					MachineInstrBuilder(MF, instr)
						.addReg(preheader_replacement_vreg).addMBB(preheader)
						.addReg(branch_replacement_vreg).addMBB(unilatch);
				} else {
					break;
				}
			}
			// Initialize counter
			auto preheader_counter_vreg = MF.getRegInfo().createVirtualRegister(&Patmos::RRegsRegClass);
			BuildMI(*preheader, preheader->end(), DL,
				TII->get((isUInt<12>(upper_bound)) ? Patmos::LIi : Patmos::LIl), preheader_counter_vreg)
				.addReg(Patmos::P0).addImm(0)
				.addImm(upper_bound);


			// Branch to header
			BuildMI(*preheader, preheader->end(), DL,
				TII->get(Patmos::BRu))
				.addReg(Patmos::P0).addImm(0)
				.addMBB(&header_mbb);
			preheader->addSuccessorWithoutProb(&header_mbb);

			// Add a PHI instruction for the loop counter in the header
			auto counter_vreg = MF.getRegInfo().createVirtualRegister(&Patmos::RRegsRegClass);
			auto counter_phi = BuildMI(header_mbb, header_mbb.begin(), DL,
				TII->get(Patmos::PHI), counter_vreg);

			// Add initial value to phi
			counter_phi.addReg(preheader_counter_vreg).addMBB(preheader);

			// redirect each latch
			SmallVector<MachineBasicBlock*> latches;
			loop->getLoopLatches(latches);
			for(auto latch: latches){
				latch->ReplaceUsesOfBlockWith(&header_mbb, unilatch);
			}

			// decrementing counter and branch to header
			auto counter_subtracted_vreg = MF.getRegInfo().createVirtualRegister(&Patmos::RRegsRegClass);
			BuildMI(*unilatch, unilatch->end(), DL,
				TII->get(Patmos::SUBi), counter_subtracted_vreg)
				.addReg(Patmos::P0).addImm(0)
				.addReg(counter_vreg).addImm(1);
			BuildMI(*unilatch, unilatch->end(), DL,
				TII->get(Patmos::BRu))
				.addReg(Patmos::NoRegister).addImm(0)
				.addMBB(&header_mbb);
			unilatch->addSuccessor(&header_mbb);
			counter_phi.addReg(counter_subtracted_vreg).addMBB(unilatch);
		}
	});
}









