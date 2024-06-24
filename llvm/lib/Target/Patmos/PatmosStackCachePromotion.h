//===-- PatmosStackCachePromotion.h - Promote values ti the stack-cache. -=====//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#ifndef PATMOSSTACKCACHEPROMOTION
#define PATMOSSTACKCACHEPROMOTION

#include "Patmos.h"
#include "PatmosTargetMachine.h"
#include "PatmosRegisterInfo.h"


#include <llvm/IR/Module.h>
#include <llvm/CodeGen/MachineFunctionPass.h>

#define DEBUG_TYPE "patmos-stack-cache-promotion"

namespace llvm {

class PatmosStackCachePromotion : public MachineFunctionPass {

private:

  const PatmosTargetMachine &TM;
  const PatmosSubtarget &STC;
  const PatmosInstrInfo *TII;
  const PatmosRegisterInfo *TRI;


public:
  static char ID;
  PatmosStackCachePromotion(const PatmosTargetMachine &tm):
                                                              MachineFunctionPass(ID), TM(tm),
                                                              STC(*tm.getSubtargetImpl()),
                                                              TII(static_cast<const PatmosInstrInfo*>(tm.getInstrInfo())),
                                                              TRI(static_cast<const PatmosRegisterInfo*>(tm.getRegisterInfo()))
  {}

  /// getPassName - Return the pass' name.
  StringRef getPassName() const override {
    return "Patmos StackCache-Promotion pass (machine code)";
  }

  bool runOnMachineFunction(MachineFunction &MF) override ;
};

} // End llvm namespace

#endif
