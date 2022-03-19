#ifndef TARGET_PATMOS_SINGLEPATH_MEMORYACCESSANALYSIS_H_
#define TARGET_PATMOS_SINGLEPATH_MEMORYACCESSANALYSIS_H_

#include "Patmos.h"
#include "PatmosTargetMachine.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

#define DEBUG_TYPE "patmos-singlepath"

namespace llvm {

class MemoryAccessAnalysis : public MachineFunctionPass {

private:

  const PatmosTargetMachine &TM;
  const PatmosSubtarget &STC;
  const PatmosInstrInfo *TII;
  const PatmosRegisterInfo *TRI;

public:
  static char ID;
  MemoryAccessAnalysis(const PatmosTargetMachine &tm):
       MachineFunctionPass(ID), TM(tm),
       STC(*tm.getSubtargetImpl()),
       TII(static_cast<const PatmosInstrInfo*>(tm.getInstrInfo())),
       TRI(static_cast<const PatmosRegisterInfo*>(tm.getRegisterInfo()))
  {}

  /// getPassName - Return the pass' name.
  StringRef getPassName() const override {
    return "Patmos Single-Path Memory Access Analysis (machine code)";
  }

  /// getAnalysisUsage - Specify which passes this pass depends on
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfo>();
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

#endif /* TARGET_PATMOS_SINGLEPATH_MEMORYACCESSANALYSIS_H_ */
