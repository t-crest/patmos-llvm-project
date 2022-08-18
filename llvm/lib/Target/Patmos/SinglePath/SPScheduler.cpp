//===---------- SchedulePostRAList.cpp - Scheduler ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "SPScheduler.h"
#include "SPListScheduler.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineInstr.h"


using namespace llvm;

static cl::opt<bool> DisableSPScheduler(
    "mpatmos-disable-singlepath-scheduler",
    cl::init(false),
    cl::desc("Disables the single-path-specific instruction scheduler. (reverts to simple Nop-padding)"),
    cl::Hidden);

static cl::opt<std::string> DisableSPSchedulerFun(
    "mpatmos-disable-singlepath-scheduler-function",
    cl::init(""),
    cl::desc("Disables the single-path-specific instruction scheduler for the named function"),
    cl::Hidden);

static cl::opt<int> DisableSPSchedulerFunBlock(
    "mpatmos-disable-singlepath-scheduler-function-block",
    cl::init(-1),
    cl::desc("Disables the single-path-specific instruction scheduler for the named function's numbered block"),
    cl::Hidden);

STATISTIC(SPInstructions,     "Number of instruction bundles in single-path code (both single and double)");

char SPScheduler::ID = 0;

FunctionPass *llvm::createSPSchedulerPass(const PatmosTargetMachine &tm) {
  return new SPScheduler(tm);
}

bool SPScheduler::runOnMachineFunction(MachineFunction &mf){

  // Only schedule single-path function
  if(! mf.getInfo<PatmosMachineFunctionInfo>()->isSinglePath()){
    return false;
  }

  LLVM_DEBUG( dbgs() << "Running SPScheduler on function '" <<  mf.getName() << "'\n");

  auto reduceAnalysis = &getAnalysis<PatmosSPReduce>();
  auto rootScope = reduceAnalysis->RootScope;

  for(auto mbbIter = mf.begin(), mbbEnd = mf.end(); mbbIter != mbbEnd; ++mbbIter){
    auto mbb = mbbIter;
    LLVM_DEBUG( dbgs() << "MBB before scheduling: \n"; mbb->dump());

    bool disable = DisableSPScheduler
        ||
        (DisableSPSchedulerFun != "" && DisableSPSchedulerFun == mf.getName() &&
            (DisableSPSchedulerFunBlock == -1 || DisableSPSchedulerFunBlock == mbb->getNumber()))
            ;

    if(!disable) {
      runListSchedule(&*mbb);
    } else {
      LLVM_DEBUG( dbgs() << "Disabled SPScheduler for '" << mf.getName() << "' block: " << mbb->getNumber() << "\n");
    }

    for(auto instrIter = mbb->begin(), instrEnd = mbb->end();
        instrIter != instrEnd; )
    {
      SPInstructions++;
      auto latency = calculateLatency(instrIter, disable);
	  for(auto i = 0; i<latency; i++){
	    TM.getInstrInfo()->insertNoop(*mbb, std::next(instrIter));
	  }
	  instrIter = std::next(instrIter, 1+latency); // Make sure to skip the newly added noops
    }
  }

  LLVM_DEBUG( dbgs() << "AFTER Single-Path Schedule\n"; mf.dump() );
  LLVM_DEBUG({
      dbgs() << "Scope tree after scheduling:\n";
      rootScope->dump(dbgs(), 0, true);
  });
  return true;
}

unsigned SPScheduler::calculateLatency(MachineBasicBlock::iterator instr, bool branches_only) const{
  if((instr->isBranch() || instr->isCall() || instr->isReturn()) && instr->hasDelaySlot()){
    // We simply add 3 nops after any branch, call or return, as its
    // the highest possible delay.
    return 3;
  } else if (branches_only &&
      (instr->mayLoad() || (instr->getOpcode() == Patmos::MUL) || (instr->getOpcode() == Patmos::MULU))
  ){
    return 1;
  } else {
    return 0;
  }
}

/////////////////////////////////////////////
// Functions for use with the list scheduler
/////////////////////////////////////////////

/// If set contains Patmos::S0, replaces it with Patmos::P0-Patmos::P7.
///
/// This is needed because S0 contains the others but using '==' doesn't
/// show this (i.e. S0 != P1).
void convert_s0_to_p(std::set<Register> &regs) {
  if(regs.count(Patmos::S0)){
    regs.erase(Patmos::S0);
    std::set<Register> predicate_regs = {
        Patmos::P0,Patmos::P1,Patmos::P2,Patmos::P3,
        Patmos::P4,Patmos::P5,Patmos::P6,Patmos::P7};
    regs.insert(predicate_regs.begin(), predicate_regs.end());
  }
}

std::set<Register> reads(const MachineInstr *instr){
  std::set<Register> result;
  std::for_each(instr->operands_begin(), instr->operands_end(), [&](auto op){
    if(op.isReg() && op.isUse()) {
      result.insert(op.getReg());
    }
  });

  // single-path functions take p7 as the predicate that enables/disables them.
  // This is not encoded is the def/use list, so add it here.
  // This is a workaround. Optimally, the list of implicits would include P7
  // but could not be made to work.
  if(instr->isCall()) {
    result.insert(Patmos::P7);
  }

  convert_s0_to_p(result);
  return result;
}

std::set<Register> writes(const MachineInstr *instr){
  std::set<Register> result;
  std::for_each(instr->operands_begin(), instr->operands_end(), [&](auto op){
    if(op.isReg() && op.isDef()) {
      result.insert(op.getReg());
    }
  });
  convert_s0_to_p(result);
  return result;
}

bool poisons(const MachineInstr *instr) {
  return isLoadInst(instr->getOpcode());
}

bool memory_access(const MachineInstr *instr) {
  auto opcode = instr->getOpcode();
  return isLoadInst(opcode) || isStoreInst(opcode) || instr->isCall() || instr->isReturn() ||
      opcode == Patmos::SRESi || opcode == Patmos::SFREEi ||
      opcode == Patmos::SENSi || opcode == Patmos::SENSr;
}

unsigned latency(const MachineInstr *instr) {
  auto opcode = instr->getOpcode();
  if(isLoadInst(opcode) || opcode == Patmos::MUL || opcode == Patmos::MULU) {
    return 1;
  } else {
    return 0;
  }
}

bool is_constant(Register r) {
  return r == Patmos::R0 || r == Patmos::P0;
}

bool conditional_branch(const MachineInstr *instr) {
  auto opcode = instr->getOpcode();
  switch(opcode){
  case Patmos::BR:
  case Patmos::BRCF:
  case Patmos::BRCFND:
  case Patmos::BRCFNDu:
  case Patmos::BRCFR:
  case Patmos::BRCFRND:
  case Patmos::BRCFRNDu:
  case Patmos::BRCFRu:
  case Patmos::BRCFT:
  case Patmos::BRCFTND:
  case Patmos::BRCFTNDu:
  case Patmos::BRCFTu:
  case Patmos::BRCFu:
  case Patmos::BRND:
  case Patmos::BRNDu:
  case Patmos::BRR:
  case Patmos::BRRND:
  case Patmos::BRRNDu:
  case Patmos::BRRu:
  case Patmos::BRT:
  case Patmos::BRTND:
  case Patmos::BRTNDu:
  case Patmos::BRTu:
  case Patmos::BRu:
    return true;
  default:
    return instr->isReturn();
  }
}

/////////////////////////////////////////////
// End of functions for use with the list scheduler
/////////////////////////////////////////////

void SPScheduler::runListSchedule(MachineBasicBlock *mbb) {
  // Scheduler cannot handle the PSEUDO_LOOPBOUND pseudo-instruction,
  // so if it's there, move it to the end of the instruction list
  // so its skipped
  auto found_loopbound = std::find_if(mbb->instr_begin(), mbb->instr_end(), [&](auto &instr) {
    return instr.getOpcode() == Patmos::PSEUDO_LOOPBOUND;
  });
  bool was_loopbound = false;
  if (found_loopbound != mbb->instr_end()) {
    auto *loopbound_instr = &*found_loopbound;
    mbb->remove_instr(loopbound_instr);
    mbb->insert(mbb->instr_end(), loopbound_instr);
    was_loopbound = true;
  }
  auto last_instr = [&](){
    auto result = mbb->instr_end();
    if(was_loopbound) {
      result--;
    }
    return result;
  };

  auto schedule = list_schedule(
    mbb->instr_begin(), last_instr(),
    reads, writes, poisons, memory_access, latency, is_constant, conditional_branch
  );
  LLVM_DEBUG(
    dbgs() << "List Schedule (New Order <- old order):\n";
    for(auto entry: schedule) {
      dbgs() << entry.first << "<-" << entry.second << "\n";
    }
  );
  auto original_instr_count = std::distance(mbb->instr_begin(), last_instr());
  auto last_new_idx = schedule.rbegin()->first;

  // Get a map from new index to the instruction that should be in it
  std::map<unsigned, MachineInstr*> instr_map;
  for(auto entry: schedule) {
    instr_map[entry.first] = &(*std::next(mbb->instr_begin(), entry.second));
  }

  // We rebuild the schedule by extracting each instruction needed at a given index
  // and inserting it at the end of the list in the new order.
  for(auto idx = 0; idx <= last_new_idx; idx++) {
    if(instr_map.count(idx)) {
      auto *instr = instr_map[idx];
      mbb->remove_instr(instr);
      mbb->insert(last_instr(), instr);
    } else {
      // Nop is needed in at this index
      TM.getInstrInfo()->insertNoop(*mbb, last_instr());
    }
  }

  auto check_schedule = [&](){
    for(auto iter=mbb->instr_begin(); iter != last_instr(); iter++) {
      auto * instr = &(*iter);
      auto idx = std::distance(mbb->instr_begin(), iter);
      if(!instr_map.count(idx)) {
        if(instr->getOpcode() != Patmos::NOP) {
          return false;
        }
      } else {
        if(instr_map[idx] != instr) {
          return false;
        }
      }
    }
    return true;
  };
  LLVM_DEBUG( dbgs() << "MBB after list scheduling: \n"; mbb->dump());
  assert(check_schedule() && "Schedule was not reordered correctly");
}




