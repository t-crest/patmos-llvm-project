//===-- MemoryAccessAnalysis.cpp - Remove unused function declarations ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass makes the single-pat code utilitize Patmos' dual issue pipeline.
// TODO: more description
//
//===----------------------------------------------------------------------===//

#include "MemoryAccessAnalysis.h"
#include "MemoryAccessAnalysisImpl.h"
#include "PatmosSinglePathInfo.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

#include "queue"
#include "deque"

#define DEBUG_TYPE "patmos-singlepath"

using namespace llvm;

//STATISTIC(LoadsConverted,     "Number of data-cache loads converted to direct main memory loads");
//STATISTIC(SavesConverted,     "Number of data-cache saves converted to direct main memory saves");

char MemoryAccessAnalysis::ID = 0;

/// createMemoryAccessAnalysisPass - Returns a new MemoryAccessAnalysis
/// \see MemoryAccessAnalysis
FunctionPass *llvm::createMemoryAccessAnalysisPass(const PatmosTargetMachine &tm) {
  return new MemoryAccessAnalysis(tm);
}

static unsigned countAccesses(const MachineBasicBlock *mbb){
  auto count = 0;
  for(auto instr_iter = mbb->instr_begin(); instr_iter != mbb->instr_end(); instr_iter++){
    switch( instr_iter->getOpcode() ){
      case Patmos::LWM:
      case Patmos::LHM:
      case Patmos::LBM:
      case Patmos::LHUM:
      case Patmos::LBUM:
      case Patmos::SWM:
      case Patmos::SHM:
      case Patmos::SBM:
        count++;
        break;
      default:
        break;
    }
  }
  return count;
}

static uint64_t getLoopBoundMax(const MachineBasicBlock *mbb){
  return getLoopBounds(mbb)->second;
}

bool MemoryAccessAnalysis::runOnMachineFunction(MachineFunction &MF) {

  // only convert function if marked
  if (PatmosSinglePathInfo::isConverting(MF))
  {
    compensate(MF);
  }

  return true;
}

void MemoryAccessAnalysis::compensate(MachineFunction &MF) {
  auto &LI = getAnalysis<MachineLoopInfo>();
  auto &RI = MF.getRegInfo();

  LLVM_DEBUG(
    dbgs() << "\n********** Patmos Memory Access Analysis **********\n";
    dbgs() << "********** Function: " << MF.getFunction().getName() << "**********\n";
    MF.dump();
  );

  assert(MF.getBlockNumbered(0)->pred_size() == 0);

  auto accesses_counts = memoryAccessAnalysis(MF.getBlockNumbered(0), &LI, countAccesses, getLoopBoundMax);

  // Find the max number of accesses among all final blocks in the function
  unsigned max_end_accesses = 0;
  const MachineBasicBlock *max_end_block = nullptr;
  for(auto bb_iter = MF.begin(); bb_iter != MF.end(); ++bb_iter){
    if(bb_iter->succ_size() == 0) {
      auto block_count = accesses_counts.first[&*bb_iter];
      if(block_count >= max_end_accesses) {
        max_end_accesses = block_count;
        max_end_block = &*bb_iter;
      }
    }
  }
  assert(max_end_block && "Didn't find a final block");

  auto compensation = memoryAccessCompensation(MF.getBlockNumbered(0), &LI, countAccesses);

  LLVM_DEBUG(
    dbgs() << "\nMemory Access Analysis Results:\n";
    dbgs() << "\tSingle Blocks:\n";
    for(auto mbb_iter=MF.begin(); mbb_iter != MF.end(); mbb_iter++){
      dbgs() << "\t\t" << mbb_iter->getName() << ": " << accesses_counts.first[&*mbb_iter] << "\n";
    }
    dbgs() << "\tLoops:\n";
    for(auto entry: accesses_counts.second){
      dbgs() << "\t\t" << entry.first->getName()
          << ": Predecessor Max: " << std::get<0>(entry.second)
          << ", Body Max: " << std::get<1>(entry.second) << "\n";
      dbgs() << "\t\t\tExits:\n";
      for(auto exit_entry: std::get<2>(entry.second)){
        dbgs() << "\t\t\t\t" << exit_entry.first->getName()
            << ": " << exit_entry.second << "\n";
      }
    }
    dbgs() << "\tMax Access Count in block '" << max_end_block->getName() << "': " << max_end_accesses << "\n";

    dbgs() << "\nMemory Access Compensation Results:\n";
    for(auto entry: compensation) {
      dbgs() << "\t" << entry.first.first->getName() << " -> " << entry.first.second->getName()
          << ": " << entry.second << "\n";
    }
  );

  auto new_vreg = [&](){return RI.createVirtualRegister(&Patmos::RRegsRegClass);};
  DebugLoc DL;

  if(max_end_accesses != 0) {
    // Tracks the virtual registers used at the end of each block as the counter
    std::map<MachineBasicBlock*, Register> block_regs;
    // The blocks that have yet to be assigned a register
    std::deque<MachineBasicBlock*> todo;
    todo.push_back(MF.getBlockNumbered(0));

    while(!todo.empty()) {
      auto *current = todo.front();
      todo.pop_front();

      LLVM_DEBUG(
        dbgs() << "\nCompensating in '" << current->getName() << "'\n";
      );

      if(current->pred_size() == 0) {
        // Initialize counter at entry block
        auto init_reg = MF.size() == 1? Patmos::R4 : new_vreg();
        block_regs[current] = init_reg;

        auto count_opcode = max_end_accesses > 4095? Patmos::LIl : Patmos::LIi;
        auto counter_insert = current->getFirstTerminator();
        auto *counter_init = MF.CreateMachineInstr(TII->get(count_opcode), DL);
        MachineInstrBuilder counter_init_builder(MF, counter_init);
        counter_init_builder.addDef(init_reg);
        counter_init_builder.addReg(Patmos::NoRegister).addImm(0); // Not predicated
        counter_init_builder.addImm(max_end_accesses);
        counter_init_builder.setMIFlags(MachineInstr::FrameSetup); // Ensure never predicated
        current->insert(counter_insert, counter_init);



        if(MF.size() == 1) {
          // Only one block, meaning no edges in 'compensation'
          // Therefore, add decrement in the block
          if(max_end_accesses <= 4095) {
            auto *init_dec_inst = MF.CreateMachineInstr(TII->get(Patmos::SUBi), DL);
            MachineInstrBuilder init_dec_inst_builder(MF, init_dec_inst);
            init_dec_inst_builder.addDef(init_reg);
            init_dec_inst_builder.addReg(Patmos::NoRegister).addImm(0);
            init_dec_inst_builder.addReg(init_reg); // decrement init register
            init_dec_inst_builder.addImm(max_end_accesses);
            current->insertAfter(counter_init, init_dec_inst);

            LLVM_DEBUG(
              dbgs() << "Inserted counter initializer and decrement (single-block function) in '" << current->getName() << "': "
                << max_end_accesses << "\n";
            );
          } else {
            report_fatal_error("Cannot handle memory access count > 4095 in single-block function");
          }
        } else {
          LLVM_DEBUG(
            dbgs() << "Inserted counter initializer in '" << current->getName() << "': ";
            RI.getOneDef(init_reg)->print(dbgs());
            dbgs() << " = " << max_end_accesses << "\n";
          );
        }

      } else if(current->pred_size() == 1) {
        auto *pred = *current->pred_begin();
        assert(block_regs.count(pred) && "Predecessor hasn't been assigned yet");


        if(compensation.count({pred, current})) {
          // Need to decrement counter.
          auto decremented_reg = new_vreg();
          block_regs[current] = decremented_reg;

          auto to_dec = compensation[{pred, current}];

          if(to_dec > 4095) {
            report_fatal_error("Not implemented compensation > 4095");
          } else {
            auto dec_insert = current->getFirstNonPHI();
            auto *dec_inst = MF.CreateMachineInstr(TII->get(Patmos::SUBi), DL);
            MachineInstrBuilder dec_inst_builder(MF, dec_inst);
            dec_inst_builder.addDef(decremented_reg);
            dec_inst_builder.addReg(Patmos::NoRegister).addImm(0);
            dec_inst_builder.addReg(block_regs[pred]); // decrement source register
            dec_inst_builder.addImm(to_dec);
            current->insert(dec_insert, dec_inst);

            LLVM_DEBUG(
              dbgs() << "Inserted counter decrement in '" << current->getName() << "' by: " << to_dec << "\n";
            );

          }
        } else {
          // Not updating counter, so just use predecessors register
          block_regs[current] = block_regs[pred];
          LLVM_DEBUG(
            dbgs() << "Not updating counter in '" << current->getName() << "'\n";
          );
        }

      } else {

        if(!block_regs.count(current)) {
          // Reserve register for block
          block_regs[current] = new_vreg();
        }

        // Multiple predecessors
        auto all_preds_assigned = std::all_of(current->pred_begin(), current->pred_end(), [&](auto pred){
          return block_regs.count(pred);
        });

        if(!all_preds_assigned) {
          // Put any missing successors on the queue
          std::for_each(current->succ_begin(), current->succ_end(), [&](auto succ){
            if(!block_regs.count(succ) && std::find(todo.begin(), todo.end(), succ) == todo.end()){
              todo.push_back(succ);
            }
          });

          // Return block to queue until predecessors have been assigned
          todo.push_back(current);

          LLVM_DEBUG(
            dbgs() << "Skipped '" << current->getName() << "'\n";
          );
          continue;
        }

        // All predecessors are assigned
        // Collect count decrements (source block, count, count register for source block)
        std::vector<std::tuple<MachineBasicBlock*, unsigned, Register>> counts;
        std::for_each(current->pred_begin(), current->pred_end(), [&](auto pred){
          counts.push_back(std::make_tuple(pred, compensation[{pred, current}], block_regs[pred]));
        });

        auto all_zero = std::all_of(counts.begin(), counts.end(), [&](auto c){
          return std::get<1>(c) == 0;
        });

        if(all_zero) {
          // No need to do any decrementing, so just use the existing register as PHI result

          auto *phi_reg_inst = MF.CreateMachineInstr(TII->get(Patmos::PHI), DL);
          MachineInstrBuilder phi_reg_inst_builder(MF, phi_reg_inst);
          phi_reg_inst_builder.addDef(block_regs[current]);

          for(auto count: counts) {
            auto pred = std::get<0>(count);
            auto count_reg = std::get<2>(count);
            phi_reg_inst_builder.addReg(count_reg);
            phi_reg_inst_builder.addMBB(pred);
          }
          current->insert(current->begin(), phi_reg_inst);

          LLVM_DEBUG(
            dbgs() << "Joined predecessor counts in '" << current->getName() << "': ";
            RI.getOneDef(block_regs[current])->dump();
          );

        } else {
          // First, create 2 PHI instructions, one for the source register from each predecessor
          // and the second for the amount that register should be decremented by
          auto phi_reg = new_vreg();
          auto phi_dec_count = new_vreg();

          auto *phi_reg_inst = MF.CreateMachineInstr(TII->get(Patmos::PHI), DL);
          MachineInstrBuilder phi_reg_inst_builder(MF, phi_reg_inst);
          phi_reg_inst_builder.addDef(phi_reg);

          auto *phi_dec_count_inst = MF.CreateMachineInstr(TII->get(Patmos::PHI), DL);
          MachineInstrBuilder phi_dec_count_inst_builder(MF, phi_dec_count_inst);
          phi_dec_count_inst_builder.addDef(phi_dec_count);

          for(auto count: counts) {
            auto pred = std::get<0>(count);
            auto dec_count = std::get<1>(count);
            auto count_reg = std::get<2>(count);
            phi_reg_inst_builder.addReg(count_reg);
            phi_reg_inst_builder.addMBB(pred);

            // Load dec_count into register in source block
            auto dec_reg = new_vreg();
            auto dec_opcode = dec_count > 4095? Patmos::LIl : Patmos::LIi;
            auto dec_insert = pred->getFirstTerminator();
            auto *dec_instr = MF.CreateMachineInstr(TII->get(dec_opcode), DL);
            MachineInstrBuilder dec_instr_builder(MF, dec_instr);
            dec_instr_builder.addDef(dec_reg);
            dec_instr_builder.addReg(Patmos::NoRegister).addImm(0);
            dec_instr_builder.addImm(dec_count);
            pred->insert(dec_insert, dec_instr);

            LLVM_DEBUG(
              dbgs() << "Inserted decrement count in predecessor '" << pred->getName()
                << "' as: "; RI.getOneDef(dec_reg)->print(dbgs());
                dbgs() << " = " << dec_count << "\n";
            );

            phi_dec_count_inst_builder.addReg(dec_reg);
            phi_dec_count_inst_builder.addMBB(pred);
          }

          current->insert(current->begin(), phi_dec_count_inst);
          current->insert(current->begin(), phi_reg_inst);

          // Decrement counter
          assert(block_regs.count(current) && "No register assigned block");
          auto dec_insert = current->getFirstNonPHI();
          auto *dec_inst = MF.CreateMachineInstr(TII->get(Patmos::SUBr), DL);
          MachineInstrBuilder dec_inst_builder(MF, dec_inst);
          dec_inst_builder.addDef(block_regs[current]);
          dec_inst_builder.addReg(Patmos::NoRegister).addImm(0);
          dec_inst_builder.addReg(phi_reg); // decrement source register
          dec_inst_builder.addReg(phi_dec_count); // Decrement by
          current->insert(dec_insert, dec_inst);

          LLVM_DEBUG(
            dbgs() << "Inserted counter decrement in '" << current->getName() << "': ";
            RI.getOneDef(block_regs[current])->print(dbgs());
            dbgs() << " = "; RI.getOneDef(phi_reg)->print(dbgs());
              dbgs() << " - ";  RI.getOneDef(phi_dec_count)->dump();
          );
        }
      }

      // Put any missing successors on the queue
      std::for_each(current->succ_begin(), current->succ_end(), [&](auto succ){
        if(!block_regs.count(succ) && std::find(todo.begin(), todo.end(), succ) == todo.end()){
          todo.push_back(succ);
        }
      });
    }

    bool found_end = false;
    // Add call to compensation function
    for(auto BB_iter = MF.begin(); BB_iter!= MF.end(); BB_iter++){
      auto *BB = &*BB_iter;
      if(BB->succ_size() == 0 && !found_end){
        found_end = true;
        // Put the max possible into r3 as input
        auto max_opcode = max_end_accesses > 4095? Patmos::LIl : Patmos::LIi;
        auto *max_inst = MF.CreateMachineInstr(TII->get(max_opcode), DL);
        MachineInstrBuilder max_inst_builder(MF, max_inst);
        max_inst_builder.addDef(Patmos::R3);
        max_inst_builder.addReg(Patmos::NoRegister).addImm(0);
        max_inst_builder.addImm(max_end_accesses);
        max_inst_builder.setMIFlags(MachineInstr::FrameSetup); // Ensure never predicated
        BB->insert(BB->getFirstTerminator(), max_inst);

        // Get instruction/register of counter
        assert(block_regs.count(BB) && "End block doesn't have a counter register");
        // First, find the instruction/register producing the remaining count
        auto count_reg = block_regs[BB];
        MachineInstr *count_reg_instr = MF.size() == 1? &*std::prev(BB->getFirstTerminator()) :  RI.getVRegDef(count_reg);
        assert(count_reg_instr && "Count register instruction not found");

        // Replace counter virtual register with r4, so that it becomes the second input
        auto *rem_copy_inst = MF.CreateMachineInstr(TII->get(Patmos::COPY), DL);
        MachineInstrBuilder rem_copy_inst_builder(MF, rem_copy_inst);
        rem_copy_inst_builder.addDef(Patmos::R4);
        rem_copy_inst_builder.addReg(count_reg);
        BB->insertAfter(max_inst, rem_copy_inst);

        // Insert call
        auto *call_instr = MF.CreateMachineInstr(TII->get(Patmos::CALLND), DL, true);
        MachineInstrBuilder call_instr_builder(MF, call_instr);
        call_instr_builder.addReg(Patmos::NoRegister).addImm(0);
        call_instr_builder.addExternalSymbol("__patmos_main_mem_access_compensation");
        call_instr_builder.addReg(Patmos::R3, RegState::ImplicitKill);
        call_instr_builder.addReg(Patmos::R4, RegState::ImplicitKill);
        call_instr_builder.addReg(Patmos::R5, RegState::ImplicitDefine);
        call_instr_builder.addReg(Patmos::SRB, RegState::ImplicitDefine);
        call_instr_builder.addReg(Patmos::SRO, RegState::ImplicitDefine);
        call_instr_builder.setMIFlags(MachineInstr::FrameSetup); // Ensure never predicated
        BB->insertAfter(rem_copy_inst, call_instr);

        LLVM_DEBUG(dbgs() << "\nInserted call to __patmos_main_mem_access_compensation in '" << BB->getName() << "'\n");
      } else if(BB->succ_size() == 0){
        report_fatal_error("Can't handle functions with multiple return points");
      }
    }
  }

  LLVM_DEBUG(
      dbgs() << "\********** Function: " << MF.getFunction().getName() << "**********\n";
      MF.dump();
    dbgs() << "\n********** Patmos Memory Access Analysis Finished **********\n";
  );
}

