#ifndef TARGET_PATMOS_SINGLEPATH_INSTRUCTIONLEVELCET_H_
#define TARGET_PATMOS_SINGLEPATH_INSTRUCTIONLEVELCET_H_

#include "Patmos.h"
#include "PatmosMachineFunctionInfo.h"
#include "PatmosSinglePathInfo.h"
#include "llvm/IR/Metadata.h"

#define DEBUG_TYPE "patmos-singlepath"

namespace llvm {

class InstructionLevelCET : public MachineFunctionPass {

private:

  const PatmosTargetMachine &TM;
  const PatmosSubtarget &STC;
  const PatmosInstrInfo *TII;
  const PatmosRegisterInfo *TRI;

  PatmosSinglePathInfo *PSPI;

public:
  static char ID;
  InstructionLevelCET(const PatmosTargetMachine &tm):
       MachineFunctionPass(ID), TM(tm),
       STC(*tm.getSubtargetImpl()),
       TII(static_cast<const PatmosInstrInfo*>(tm.getInstrInfo())),
       TRI(static_cast<const PatmosRegisterInfo*>(tm.getRegisterInfo()))
  {}

  /// getPassName - Return the pass' name.
  StringRef getPassName() const override {
    return "Patmos Single-Path Instruction-Level Constant Execution-Time (machine code)";
  }

  /// getAnalysisUsage - Specify which passes this pass depends on
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<PatmosSinglePathInfo>();
    AU.addPreserved<PatmosSinglePathInfo>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool doInitialization(Module &M) override {
    return false;
  }

  bool doFinalization(Module &M) override {
    return false;
  }

  bool runOnMachineFunction(MachineFunction &MF) override ;
  void compensate(MachineFunction &MF);
};

}

#endif /* TARGET_PATMOS_SINGLEPATH_INSTRUCTIONLEVELCET_H_ */
