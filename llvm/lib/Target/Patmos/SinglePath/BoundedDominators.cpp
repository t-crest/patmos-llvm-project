//===-- .cpp - Remove unused function declarations ------------===//
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

#include "Patmos.h"
#include "SinglePath/PatmosSinglePathInfo.h"
#include "BoundedDominatorAnalysis.h"
#include "BoundedDominators.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

using namespace llvm;


char BoundedDominators::ID = 0;

/// createDataCacheAccessEliminationPass - Returns a new DataCacheAccessElimination
/// \see DataCacheAccessElimination
FunctionPass *llvm::createPatmosBoundedDominatorsPass() {
  return new BoundedDominators();
}

static bool constantBounds(const MachineBasicBlock *mbb) {
  if(auto bounds = getLoopBounds(mbb)) {
    return bounds->first == bounds->second;
  }
  return false;
}

void BoundedDominators::calculate(MachineFunction &MF, MachineLoopInfo &LI) {
  if(PatmosSinglePathInfo::isEnabled(MF)) {
    dominators = boundedDominatorsAnalysis(MF.getBlockNumbered(0), &LI, constantBounds);
  }
}

bool BoundedDominators::runOnMachineFunction(MachineFunction &MF) {
  calculate(MF, getAnalysis<MachineLoopInfo>());
  return false;
}

void BoundedDominators::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<MachineLoopInfo>();
  AU.setPreservesAll();
  MachineFunctionPass::getAnalysisUsage(AU);
}


