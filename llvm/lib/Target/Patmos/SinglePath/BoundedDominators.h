//===- .h - Patmos machine function info -*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef _PATMOS_BOUNDEDDOMINATORS_H_
#define _PATMOS_BOUNDEDDOMINATORS_H_

#include "PatmosTargetMachine.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

#include <set>
#include <map>

namespace llvm {

///
class BoundedDominators : public MachineFunctionPass {
public:
  std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>> dominators;
  /// Pass ID
  static char ID;

  BoundedDominators() : MachineFunctionPass(ID){};

  explicit BoundedDominators(MachineFunction &MF, MachineLoopInfo &LI)
        : MachineFunctionPass(ID) {
      calculate(MF, LI);
    }

  BoundedDominators(const BoundedDominators &) = delete;
  BoundedDominators &operator=(const BoundedDominators &) = delete;

  void calculate(MachineFunction &MF, MachineLoopInfo &LI);

  /// getAnalysisUsage - Specify which passes this pass depends on
  void getAnalysisUsage(AnalysisUsage &AU) const override;

  /// runOnMachineFunction - Run the SP converter on the given function.
  bool runOnMachineFunction(MachineFunction &MF) override;

  /// getPassName - Return the pass' name.
  StringRef getPassName() const override{
    return "Patmos Bounded Dominators";
  }
};

} // End llvm namespace

#endif // _PATMOS_BOUNDEDDOMINATORS_H_
