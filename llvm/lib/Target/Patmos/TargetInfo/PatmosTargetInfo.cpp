//===-- PatmosTargetInfo.cpp - Patmos Target Implementation ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Patmos.h"
#include "PatmosTargetInfo.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Compiler.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/ADT/Optional.h"

using namespace llvm;


void llvm::getMBBIRName(const MachineBasicBlock *MBB,
                         SmallString<128> &result) {
  const BasicBlock *BB = MBB->getBasicBlock();
  const Twine bbname = (BB && BB->hasName()) ? BB->getName()
                                             : "<anonymous>";
  const Twine bb_ir_label = "#" + MBB->getParent()->getFunction().getName() +
                            "#" + bbname +
                            "#" + Twine(MBB->getNumber());
  bb_ir_label.toVector(result);
}

Optional<std::pair<uint64_t, uint64_t>> llvm::getLoopBounds(const MachineBasicBlock * MBB) {
  if(MBB && MBB->getBasicBlock()) {
    auto bound_instr = std::find_if(MBB->begin(), MBB->end(), [&](auto &instr){
      return instr.getOpcode() == Patmos::PSEUDO_LOOPBOUND;
    });
    if(bound_instr != MBB->end()) {
      assert(bound_instr->getOperand(0).isImm());
      assert(bound_instr->getOperand(1).isImm());
      assert(bound_instr->getOperand(0).isImm() >= 0);
      assert(bound_instr->getOperand(1).isImm() >= 0);

      auto min = bound_instr->getOperand(0).getImm() + 1;
      auto max = min + bound_instr->getOperand(1).getImm();

      // Check for overflows
      if(min < 0 || max < min) {
        report_fatal_error("Invalid loop bounds");
      }

      return std::make_pair((uint64_t) min, (uint64_t) max);
    }
  }
  return None;
}

Target &llvm::getThePatmosTarget() {
  static Target ThePatmosTarget;
  return ThePatmosTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializePatmosTargetInfo() {
  RegisterTarget<Triple::patmos> X(getThePatmosTarget(), "patmos",
                                   "Patmos [experimental]", "Patmos");
}
