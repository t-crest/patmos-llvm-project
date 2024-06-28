//===-- PatmosStackCachePromotion.cpp - Analysis pass to determine which FIs can
//be promoted to the stack cache
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
#include "PatmosMachineFunctionInfo.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/InstIterator.h"
#include <queue>
#include <unordered_map>
#include <unordered_set>

using namespace llvm;

static cl::opt<bool> EnableStackCachePromotion(
    "mpatmos-enable-stack-cache-promotion", cl::init(false),
    cl::desc("Enable the compiler to promote data to the stack cache"));

static cl::opt<bool> EnableArrayStackCachePromotion(
    "mpatmos-enable-array-stack-cache-promotion", cl::init(false),
    cl::desc("Enable the compiler to promote arrays to the stack cache"));

char PatmosStackCachePromotion::ID = 0;

/// createDataCacheAccessEliminationPass - Returns a new
/// DataCacheAccessElimination \see DataCacheAccessElimination
FunctionPass *
llvm::createPatmosStackCachePromotionPass(const PatmosTargetMachine &tm) {
  return new PatmosStackCachePromotion(tm);
}


void collectPointerUses(const Value *V, std::unordered_set<const Instruction*> &Visited) {
  for (const User *U : V->users()) {
    if (const Instruction *Inst = dyn_cast<Instruction>(U)) {
      if (Visited.insert(Inst).second) {
        // Recursively track uses through bitcasts, phis, selects, etc.
        if (isa<BitCastInst>(Inst) || isa<PHINode>(Inst) || isa<SelectInst>(Inst)) {
          collectPointerUses(Inst, Visited);
        } else if (isa<LoadInst>(Inst) || isa<StoreInst>(Inst) || isa<GetElementPtrInst>(Inst) || isa<CallInst>(Inst)) {
          Visited.insert(Inst);
        }
      }
    }
  }
}

bool isAllocaUsedAsPointer(const AllocaInst *AI) {
  std::unordered_set<const Instruction*> Visited;
  collectPointerUses(AI, Visited);

  for (const Instruction *Inst : Visited) {
    if (isa<GetElementPtrInst>(Inst) || (isa<CallInst>(Inst) && !(dyn_cast<CallInst>(Inst)->isInlineAsm()))) {
      return true;
    }
  }
  return false;
}

bool isFrameIndexUsedAsPointer(MachineFunction &MF, int ObjectFI) {
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  // Check if the frame index is valid
  if (ObjectFI < MFI.getObjectIndexBegin() || ObjectFI >= MFI.getObjectIndexEnd()) {
    return true;
  }

  const AllocaInst *AI = MFI.getObjectAllocation(ObjectFI);
  if (!AI) return true;

  return isAllocaUsedAsPointer(AI);
}

bool isIndirectUseRecursive(MachineInstr &MI, MachineRegisterInfo &MRI, int FI, SmallPtrSetImpl<MachineInstr *> &Visited) {
  if (Visited.count(&MI))
    return false;
  Visited.insert(&MI);

  for (auto &MO : MI.operands()) {
    if (MO.isReg() && Register::isVirtualRegister(MO.getReg())) { // TODO Check this
      MachineInstr *DefMI = MRI.getVRegDef(MO.getReg());
      if (!DefMI)
        continue;

      for (auto &DefMO : DefMI->operands()) {
        if (DefMO.isFI() && DefMO.getIndex() == FI) {
          return true;
        }
      }

      if (isIndirectUseRecursive(*DefMI, MRI, FI, Visited)) {
        return true;
      }
    }
  }

  return false;
}

bool isFrameIndexUsedIndirectly(MachineInstr &MI, MachineRegisterInfo &MRI, int FI) {
  // Check if the instruction is a memory load/store
  if (!MI.mayLoad() && !MI.mayStore())
    return false;

  SmallPtrSet<MachineInstr *, 8> Visited;
  for (auto &MO : MI.operands()) {
    if (MO.isReg() && isIndirectUseRecursive(MI, MRI, FI, Visited)) {
      return true;
    }
  }

  return false;
}


std::unordered_set<MachineInstr*> findIndirectUses(MachineFunction &MF, int FI) {
  MachineRegisterInfo &MRI = MF.getRegInfo();

  std::unordered_set<MachineInstr*> IndirectUses;
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      if (std::any_of(MI.operands_begin(), MI.operands_end(), [](const MachineOperand& op){
            return op.isFI();
          }))
        continue;
      if (isFrameIndexUsedIndirectly(MI, MRI, FI)) {
        IndirectUses.insert(&MI);
      }
    }
  }

  // For demonstration purposes, print the indirect uses found
  LLVM_DEBUG({
    for (MachineInstr *MI : IndirectUses) {
      dbgs() << "Indirect use of frame index " << FI << ": " << *MI;
    }
  });
  return IndirectUses;
}

bool PatmosStackCachePromotion::runOnMachineFunction(MachineFunction &MF) {
  if (EnableStackCachePromotion) {
    LLVM_DEBUG(dbgs() << "Enabled Stack Cache promotion for: "
                      << MF.getFunction().getName() << "\n");

    MachineFrameInfo &MFI = MF.getFrameInfo();
    PatmosMachineFunctionInfo &PMFI = *MF.getInfo<PatmosMachineFunctionInfo>();

    std::unordered_set<unsigned> StillPossibleFIs;
    for (unsigned FI = 0, FIe = MFI.getObjectIndexEnd(); FI != FIe; FI++) {
      if (!MFI.isFixedObjectIndex(FI) && MFI.isAliasedObjectIndex(FI)) {
        if (!isFrameIndexUsedAsPointer(MF, FI)) {
          PMFI.addStackCacheAnalysisFI(FI);
        } else {
          StillPossibleFIs.insert(FI);
        }
      }
    }

    if (EnableArrayStackCachePromotion) {

      // Logic for handling arrays on SC
      const std::unordered_map<unsigned, unsigned> Mappings = {
          {Patmos::LWC, Patmos::LWS},   {Patmos::LHC, Patmos::LHS},
          {Patmos::LBC, Patmos::LBS},   {Patmos::LHUC, Patmos::LHUS},
          {Patmos::LBUC, Patmos::LBUS},

          {Patmos::SWC, Patmos::SWS},   {Patmos::SHC, Patmos::SHS},
          {Patmos::SBC, Patmos::SBS},
      };

      for (const auto FI : StillPossibleFIs) {
        const auto &Uses = findIndirectUses(MF, FI);

        const bool AllConvertible = std::all_of(
            Uses.begin(), Uses.end(), [&Mappings](MachineInstr *Inst) {
              return Mappings.find(Inst->getOpcode()) != Mappings.end();
            });

        if (AllConvertible) {
          LLVM_DEBUG(dbgs() << "All instructions referencing FI: " << FI
                            << " are convertible"
                            << "\n");

          // Put FI on SC
          PMFI.addStackCacheAnalysisFI(FI);

          // Now convert all instructions
          for (MachineInstr *Use : Uses) {
            const unsigned OPnew = Mappings.at(Use->getOpcode());
            Use->setDesc(TII->get(OPnew));
          }
        }
      }
    }

    for (const int FI : PMFI.getStackCacheAnalysisFIs()) {
      LLVM_DEBUG(dbgs() << "FI on Stack Cache: " << FI << "\n");
    }
  }
  return true;
}
