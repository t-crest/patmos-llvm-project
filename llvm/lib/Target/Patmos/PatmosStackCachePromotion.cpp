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

using namespace llvm;

STATISTIC(LoadsConverted,
          "Number of data-cache loads converted to stack cache loads");
STATISTIC(SavesConverted,
          "Number of data-cache saves converted to stack cache saves");

STATISTIC(BytesToStackCache,
          "Number of data-cache loads converted to stack cache loads");

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

    // TODO Promote some data to stack cache
    for (auto BB_iter = MF.begin(), BB_iter_end = MF.end();
         BB_iter != BB_iter_end; ++BB_iter) {
      for (auto instr_iter = BB_iter->begin(), instr_iter_end = BB_iter->end();
           instr_iter != instr_iter_end; ++instr_iter) {
        switch (instr_iter->getOpcode()) {
        case Patmos::LWC:
        case Patmos::LHC:
        case Patmos::LBC:
        case Patmos::LHUC:
        case Patmos::LBUC:
          LoadsConverted++;
          break;
        case Patmos::SWC:
          BytesToStackCache += 4;
          SavesConverted++;
          break;
        case Patmos::SHC:
          BytesToStackCache += 2;
          SavesConverted++;
          break;
        case Patmos::SBC:
          BytesToStackCache += 1;
          SavesConverted++;
          break;
        default:
          // Not a data cache accesses, ignore
          continue;
        }
      }
    }

    LLVM_DEBUG(dbgs() << "Converting: " << LoadsConverted << " Loads and "
                      << SavesConverted << " Saves resulting in " << BytesToStackCache << " Bytes to SC\n");

    auto startofblock = MF.begin();
    auto startInstr = startofblock->begin();

    auto *ensureInstr = MF.CreateMachineInstr(TII->get(Patmos::SRESi),
                                              startInstr->getDebugLoc());
    MachineInstrBuilder ensureInstrBuilder(MF, ensureInstr);
    AddDefaultPred(ensureInstrBuilder);
    ensureInstrBuilder.addImm(BytesToStackCache);

    startofblock->insert(startInstr, ensureInstr);

    for (auto BB_iter = MF.begin(), BB_iter_end = MF.end();
         BB_iter != BB_iter_end; ++BB_iter) {
      for (auto instr_iter = BB_iter->begin(), instr_iter_end = BB_iter->end();
           instr_iter != instr_iter_end; ++instr_iter) {
        unsigned convert_to;
        switch (instr_iter->getOpcode()) {
        case Patmos::LWC:
          LoadsConverted++;
          convert_to = Patmos::LWS;
          break;
        case Patmos::LHC:
          LoadsConverted++;
          convert_to = Patmos::LHS;
          break;
        case Patmos::LBC:
          LoadsConverted++;
          convert_to = Patmos::LBS;
          break;
        case Patmos::LHUC:
          LoadsConverted++;
          convert_to = Patmos::LHUS;
          break;
        case Patmos::LBUC:
          LoadsConverted++;
          convert_to = Patmos::LBUM;
          break;
        case Patmos::SWC:
          SavesConverted++;
          convert_to = Patmos::SWS;
          break;
        case Patmos::SHC:
          SavesConverted++;
          convert_to = Patmos::SHS;
          break;
        case Patmos::SBC:
          SavesConverted++;
          convert_to = Patmos::SBS;
          break;
        default:
          // Not a data cache accesses, ignore
          continue;
        }

        // Create new instruction accessing main memory instead
        /*auto *new_instr = MF.CreateMachineInstr(TII->get(convert_to),
                                                instr_iter->getDebugLoc());
        MachineInstrBuilder new_instr_builder(MF, new_instr);

        // Give it the same operands
        for (auto op : instr_iter->operands()) {
          new_instr_builder.add(op);
        }

        // Replace old instruction by new one in BB
        BB_iter->insertAfter(instr_iter, new_instr);
        instr_iter = BB_iter->erase(&*instr_iter);*/
      }
    }

    auto& endofBlock = MF.back();
    auto& endInstr = endofBlock.back();

    auto *freeInstr = MF.CreateMachineInstr(TII->get(Patmos::SFREEi),
                                            endInstr.getDebugLoc());
    MachineInstrBuilder freeInstrBuilder(MF, freeInstr);
    AddDefaultPred(freeInstrBuilder);
    freeInstrBuilder.addImm(BytesToStackCache);

    endofBlock.insert(endInstr, freeInstr);
  } else {
    LLVM_DEBUG(dbgs() << "Disabled Stack Cache promotion for: "
                      << MF.getFunction().getName() << "\n");
  }
  return true;
}
