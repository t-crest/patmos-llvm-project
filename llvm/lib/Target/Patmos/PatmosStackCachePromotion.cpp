//===-- PatmosStackCachePromotion.cpp - Remove unused function declarations ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "PatmosStackCachePromotion.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

STATISTIC(LoadsConverted,     "Number of data-cache loads converted to stack cache loads");
STATISTIC(SavesConverted,     "Number of data-cache saves converted to stack cache saves");

static cl::opt<bool> EnableStackCachePromotion("mpatmos-enable-stack-cache-promotion",
                                       cl::init(false),
                                       cl::desc("Enable the compiler to promot data to the stack cache"));

char PatmosStackCachePromotion::ID = 0;

/// createDataCacheAccessEliminationPass - Returns a new DataCacheAccessElimination
/// \see DataCacheAccessElimination
FunctionPass *llvm::createPatmosStackCachePromotionPass(const PatmosTargetMachine &tm) {
  return new PatmosStackCachePromotion(tm);
}

bool PatmosStackCachePromotion::runOnMachineFunction(MachineFunction &MF) {
  if (EnableStackCachePromotion) {
    LLVM_DEBUG(dbgs() << "Enabled Stack Cache promotion for: " << MF.getFunction().getName() << "\n");

    // TODO Promote some data to stack cache
  } else {
    LLVM_DEBUG(dbgs() << "Disabled Stack Cache promotion for: " << MF.getFunction().getName() << "\n");
  }
  return true;
}


