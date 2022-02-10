//===-- InstructionLevelCET.cpp - Remove unused function declarations ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass
// TODO: more description
//
//===----------------------------------------------------------------------===//

#include "InstructionLevelCET.h"
#include "llvm/ADT/Statistic.h"

using namespace llvm;

STATISTIC(CompLoads,     "Number of Constant Execution-Time compensatory loads added");
STATISTIC(CompSaves,     "Number of Constant Execution-Time compensatory Saves added");
STATISTIC(CompInstrs,     "Number of Constant Execution-Time compensatory Instructions added in total");

char InstructionLevelCET::ID = 0;

/// createInstructionLevelCETPass - Returns a new InstructionLevelCET
/// \see InstructionLevelCET
FunctionPass *llvm::createInstructionLevelCETPass(const PatmosTargetMachine &tm) {
  return new InstructionLevelCET(tm);
}

bool InstructionLevelCET::runOnMachineFunction(MachineFunction &MF) {
  PSPI = &getAnalysis<PatmosSinglePathInfo>();
    
  // only convert function if marked
  if (PSPI->isConverting(MF))
  {
    compensate(MF);
  }

  return true;
}

void InstructionLevelCET::compensate(MachineFunction &MF)
{
  for(auto BB_iter = MF.begin(); BB_iter != MF.end(); ++BB_iter){
    auto *block = PSPI->getRootScope()->findBlockOf(&*BB_iter);
    assert(block && "MBB is not associated with a PredicatedBlock");

    for(auto instr_iter = BB_iter->begin(); instr_iter != BB_iter->end(); ++instr_iter){
      if( instr_iter->mayLoadOrStore() && PatmosInstrInfo::getMemType(*instr_iter) == PatmosII::MEM_M) {
        auto *old_instr = &*instr_iter;
        auto old_pred = block->getInstructionPredicates()[old_instr];

        DebugLoc DL;
        auto new_instr_builder = BuildMI(*BB_iter, *instr_iter, DL, TII->get(Patmos::LWM), Patmos::R0);
        auto new_instr = new_instr_builder.getInstr();

        int i = old_instr->findFirstPredOperandIdx();
        new_instr_builder.add(old_instr->getOperand(i));    // Predicate
        new_instr_builder.add(old_instr->getOperand(i+1));  // Predicate negation
        new_instr_builder.addReg(Patmos::R0);

        block->setPredicateFor(new_instr, old_pred.first, !old_pred.second);

        CompLoads++;
        CompInstrs++;
      }
    }
  }
}
