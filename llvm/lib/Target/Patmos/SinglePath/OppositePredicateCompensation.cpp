//===-- InstructionLevelCET.cpp - Remove unused function declarations ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implements Memory access normalization throuh Opposite Predicate Compensation
// to ensure constant execution time.
//
// Looks for any function that has been flagged 'sp-cet-opposite' and inserts the
// compensation code into it.
// Ignores singlepath functions that aren't flagged, such that it is possible
// for earlier passes to decide which functions should use this technique
// for compensation instead of another.
//
//===----------------------------------------------------------------------===//

#include "OppositePredicateCompensation.h"
#include "llvm/ADT/Statistic.h"

#define DEBUG_TYPE "patmos-const-exec"

using namespace llvm;

STATISTIC(CompLoads,     "Number of non-phi instructions added by the 'opposite' constant execution time compensation");

char OppositePredicateCompensation::ID = 0;

const std::string OppositePredicateCompensation::ENABLE_OPC_ATTR="sp-cet-opposite";

FunctionPass *llvm::createOppositePredicateCompensationPass(const PatmosTargetMachine &tm) {
  return new OppositePredicateCompensation(tm);
}

bool OppositePredicateCompensation::runOnMachineFunction(MachineFunction &MF) {
  PSPI = &getAnalysis<PatmosSinglePathInfo>();
    
  if (PatmosSinglePathInfo::isConverting(MF) && MF.getFunction().hasFnAttribute(ENABLE_OPC_ATTR))
  {
    compensate(MF);
    return true;
  }

  return false;
}

void OppositePredicateCompensation::compensate(MachineFunction &MF)
{
  std::for_each(MF.begin(), MF.end(), [&](auto &BB){
    auto *block = PSPI->getRootScope()->findBlockOf(&BB);
    assert(block && "MBB is not associated with a PredicatedBlock");

    for(auto instr_iter = BB.begin(); instr_iter != BB.end(); ++instr_iter){
      if( instr_iter->mayLoadOrStore() && PatmosInstrInfo::getMemType(*instr_iter) == PatmosII::MEM_M) {
        auto old_pred = block->getInstructionPredicates()[&*instr_iter];

        DebugLoc DL;
        auto new_instr_builder = BuildMI(BB, *instr_iter, DL, TII->get(Patmos::LWM),
            // Always load into r0 such that you never overwrite a used register
            Patmos::R0)
            // Predicate
            .addReg(Patmos::NoRegister)
            // Predicate negation flag
            .addImm(0)
            // We create a load using address 0 (r0), which is always a valid address.
            .addReg(Patmos::R0);

        // Set the new instruction to use the negated predicate
        block->setPredicateFor(new_instr_builder.getInstr(), old_pred.first, !old_pred.second);

        CompLoads++;
      }
    }
  });
}
