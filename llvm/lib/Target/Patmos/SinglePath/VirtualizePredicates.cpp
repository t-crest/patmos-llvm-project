
#include "VirtualizePredicates.h"
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

char VirtualizePredicates::ID = 0;

FunctionPass *llvm::createVirtualizePredicates(const PatmosTargetMachine &tm) {
  return new VirtualizePredicates(tm);
}

bool VirtualizePredicates::runOnMachineFunction(MachineFunction &MF) {
	bool changed = false;
	// only convert function if marked
	if ( MF.getInfo<PatmosMachineFunctionInfo>()->isSinglePath()) {
		LLVM_DEBUG(
			dbgs() << "[Single-Path] Virtualize Predicates " << MF.getFunction().getName() << "\n" ;
			MF.dump();
		);

		std::map<Register, Register> phys_to_virt;
		phys_to_virt[Patmos::P1] = MF.getRegInfo().createVirtualRegister(&Patmos::PRegsRegClass, "sp.virtualized.p1");
		phys_to_virt[Patmos::P2] = MF.getRegInfo().createVirtualRegister(&Patmos::PRegsRegClass, "sp.virtualized.p2");
		phys_to_virt[Patmos::P3] = MF.getRegInfo().createVirtualRegister(&Patmos::PRegsRegClass, "sp.virtualized.p3");
		phys_to_virt[Patmos::P4] = MF.getRegInfo().createVirtualRegister(&Patmos::PRegsRegClass, "sp.virtualized.p4");
		phys_to_virt[Patmos::P5] = MF.getRegInfo().createVirtualRegister(&Patmos::PRegsRegClass, "sp.virtualized.p5");
		phys_to_virt[Patmos::P6] = MF.getRegInfo().createVirtualRegister(&Patmos::PRegsRegClass, "sp.virtualized.p6");
		phys_to_virt[Patmos::P7] = MF.getRegInfo().createVirtualRegister(&Patmos::PRegsRegClass, "sp.virtualized.p7");

		// There is a bug either in the LLVM register allocator or in the Patmos backend
		// that requires all predicate virtual register to have a hint. If not, the allocator
		// will fail an assertion looking for it.
		MF.getRegInfo().setSimpleHint(phys_to_virt[Patmos::P1], Patmos::P1);
		MF.getRegInfo().setSimpleHint(phys_to_virt[Patmos::P2], Patmos::P2);
		MF.getRegInfo().setSimpleHint(phys_to_virt[Patmos::P3], Patmos::P3);
		MF.getRegInfo().setSimpleHint(phys_to_virt[Patmos::P4], Patmos::P4);
		MF.getRegInfo().setSimpleHint(phys_to_virt[Patmos::P5], Patmos::P5);
		MF.getRegInfo().setSimpleHint(phys_to_virt[Patmos::P6], Patmos::P6);
		MF.getRegInfo().setSimpleHint(phys_to_virt[Patmos::P7], Patmos::P7);

		// Go through all instruction and replace uses of physical predicate register with their virtual
		// counterpart
		std::for_each(MF.begin(), MF.end(), [&](auto &mbb){
			std::for_each(mbb.begin(), mbb.end(), [&](MachineInstr &instr){
				for(auto &op: instr.operands()) {
					if(op.isReg() && phys_to_virt.count(op.getReg())) {
						if(instr.isCall() && !PatmosSinglePathInfo::isRootLike(*getCallTarget(&instr))
								&& op.getReg() == Patmos::P7) {
							// Don't rename the P7 register for calls (where it is used for enabling/disabling
							// the whole function).
							assert(op.isImplicit() && "Call instruction P7 argument isn't implicit");
						} else {
							op.setReg(phys_to_virt[op.getReg()]);
						}
					}
				}
			});
		});

		MF.getProperties().reset(MachineFunctionProperties::Property::IsSSA);
		MF.getProperties().reset(MachineFunctionProperties::Property::NoVRegs);
		changed |= true;

		LLVM_DEBUG(
			dbgs() << "[Single-Path] Virtualize Predicates End " << MF.getFunction().getName() << "\n" ;
			MF.dump();
		);
	}
	return changed;
}










