//===-- PatmosStackCachePromotion.cpp - Remove unused function declarations
//------------===//
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
#include "llvm/CodeGen/MachineFrameInfo.h"

using namespace llvm;

STATISTIC(LoadsConverted,
          "Number of data-cache loads converted to stack cache loads");
STATISTIC(SavesConverted,
          "Number of data-cache saves converted to stack cache saves");

static cl::opt<bool> EnableStackCachePromotion(
    "mpatmos-enable-stack-cache-promotion", cl::init(false),
    cl::desc("Enable the compiler to promot data to the stack cache"));

char PatmosStackCachePromotion::ID = 0;

/// createDataCacheAccessEliminationPass - Returns a new
/// DataCacheAccessElimination \see DataCacheAccessElimination
FunctionPass *
llvm::createPatmosStackCachePromotionPass(const PatmosTargetMachine &tm) {
  return new PatmosStackCachePromotion(tm);
}

bool PatmosStackCachePromotion::runOnMachineFunction(MachineFunction &MF) {
  if (EnableStackCachePromotion) {
    LLVM_DEBUG(dbgs() << "Enabled Stack Cache promotion for: "
                      << MF.getFunction().getName() << "\n");

    // Calculate the amount of bytes to store on SC
    MachineFrameInfo &MFI = MF.getFrameInfo();
    unsigned stackSize = 0;
    for(unsigned FI = 0, FIe = MFI.getObjectIndexEnd(); FI != FIe; FI++) {
      stackSize += MFI.getObjectSize(FI);
    }

    LLVM_DEBUG(dbgs() << "Storing " << MFI.getObjectIndexEnd() << "MFIs resulting in " << stackSize << " bytes to SC\n");

    // Insert Reserve before function start
    auto startofblock = MF.begin();
    auto startInstr = startofblock->begin();

    auto *ensureInstr = MF.CreateMachineInstr(TII->get(Patmos::SRESi),
                                              startInstr->getDebugLoc());
    MachineInstrBuilder ensureInstrBuilder(MF, ensureInstr);
    AddDefaultPred(ensureInstrBuilder);
    ensureInstrBuilder.addImm(stackSize);

    startofblock->insert(startInstr, ensureInstr);

    // TODO Convert access to SC access

    // Insert Free after function End
    auto endofBlock = MF.rbegin();
    auto endInstr = endofBlock->rbegin();

    auto *freeInstr = MF.CreateMachineInstr(TII->get(Patmos::SFREEi),
                                            endInstr->getDebugLoc());
    MachineInstrBuilder freeInstrBuilder(MF, freeInstr);
    AddDefaultPred(freeInstrBuilder);
    freeInstrBuilder.addImm(stackSize);

    endofBlock->insert(endInstr->getIterator(), freeInstr);
  }
  return true;
}
