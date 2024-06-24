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
#include <unordered_set>

using namespace llvm;

static cl::opt<bool> EnableStackCachePromotion(
    "mpatmos-enable-stack-cache-promotion", cl::init(false),
    cl::desc("Enable the compiler to promote data to the stack cache"));

char PatmosStackCachePromotion::ID = 0;

/// createDataCacheAccessEliminationPass - Returns a new
/// DataCacheAccessElimination \see DataCacheAccessElimination
FunctionPass *
llvm::createPatmosStackCachePromotionPass(const PatmosTargetMachine &tm) {
  return new PatmosStackCachePromotion(tm);
}

void getDeps(MachineInstr* MI,
             const MachineFunction &MF,
             std::vector<MachineInstr *>& Dependencies) {
  if (std::find(Dependencies.begin(), Dependencies.end(), MI) != Dependencies.end())
    return;
  Dependencies.push_back(MI);
  const MachineRegisterInfo &MRI = MF.getRegInfo();
  for (auto &MO : MI->operands()) {
    if (MO.isReg()) {
      unsigned Reg = MO.getReg();
      if (Register::isVirtualRegister(Reg)) {
        MachineInstr *DefMI = MRI.getVRegDef(Reg);
        if (DefMI) {
          getDeps(DefMI, MF, Dependencies);
        }
      }
    }
  }
  return;
}

std::vector<MachineInstr *> getDeps(const MachineInstr &MI,
                                    const MachineFunction &MF) {
  const MachineRegisterInfo &MRI = MF.getRegInfo();
  std::vector<MachineInstr *> Dependencies;

  for (unsigned i = 0; i < MI.getNumOperands(); ++i) {
    auto &MO = MI.getOperand(i);
    if (MO.isReg() && (MI.mayStore() ? MO.isUse() && i == 2 : MO.isUse())) {
      unsigned Reg = MO.getReg();
      if (Register::isVirtualRegister(Reg)) {
        MachineInstr *DefMI = MRI.getVRegDef(Reg);
        if (DefMI) {
          getDeps(DefMI, MF, Dependencies);
        }
      }
    }
  }
  return Dependencies;
}

const GlobalVariable* findGlobalVariable(const Value *V) {
  if (auto *AI = dyn_cast<GlobalVariable>(V))
    return AI;

  if (auto *BCI = dyn_cast<BitCastInst>(V))
    return findGlobalVariable(BCI->getOperand(0));
  if (auto *GEP = dyn_cast<GetElementPtrInst>(V))
    return findGlobalVariable(GEP->getPointerOperand());
  if (auto *CE = dyn_cast<ConstantExpr>(V)) {
    if (CE->getOpcode() == Instruction::BitCast) {
      return findGlobalVariable(CE->getOperand(0));
    }
  }

  return nullptr;
}

bool isFIAPointerToExternalSymbol(const int ObjectFI, const MachineFunction &MF) {
  // Get the frame information from the machine function
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  // Check if the frame index is valid
  if (ObjectFI < MFI.getObjectIndexBegin() || ObjectFI >= MFI.getObjectIndexEnd()) {
    return false;
  }

  // Check if the frame index is associated with a fixed object
  if (MFI.isFixedObjectIndex(ObjectFI)) {
    return false;
  }

  const AllocaInst *AI = MFI.getObjectAllocation(ObjectFI);
  //LLVM_DEBUG(dbgs() << "AllocaInst: " << *AI << "\n");

  const Function &F = MF.getFunction();
  const Module *M = F.getParent();

  std::vector<const StoreInst*> stores;

  for (const BasicBlock &BB : F) {
    for (const Instruction &I : BB) {
      if (const auto *store = dyn_cast<StoreInst>(&I)) {
        if (store->getPointerOperand() == AI) {
          stores.push_back(store);
        }
      }
    }
  }

  if (std::any_of(stores.begin(), stores.end(), [&](const StoreInst* store){
      return store->getValueOperand()->getType()->isPointerTy();
    })) {
    return true;
  }


  for (const GlobalVariable &GV : M->globals()) {
    if (std::any_of(stores.begin(), stores.end(), [&](const StoreInst* store){
      if (const GlobalVariable *AI = findGlobalVariable(store->getValueOperand())) {
        return AI == &GV;
      }
      return false;
        })) {
      //LLVM_DEBUG(dbgs() << "store from: " << GV << "\n");
      return true;
    }
  }

  return false;
}

bool hasFIonSC(const std::vector<MachineInstr *> deps,
               MachineFunction &MF) {
  PatmosMachineFunctionInfo &PMFI =
      *MF.getInfo<PatmosMachineFunctionInfo>();

  bool onSC = false;
  for (const auto MI : deps) {
    //LLVM_DEBUG(dbgs() << "Checking MI: " << *MI);
    for (const auto &op : MI->operands()) {
      if (op.isFI() && isFIAPointerToExternalSymbol(op.getIndex(), MF)) {
        LLVM_DEBUG(dbgs() << "Removing FI from SC: " << op.getIndex());
        PMFI.removeStackCacheAnalysisFIs(op.getIndex());
        return false;
      }
      if (op.isFI() &&
          std::any_of(PMFI.getStackCacheAnalysisFIs().begin(),
                      PMFI.getStackCacheAnalysisFIs().end(),
                      [&op](auto elem) { return elem == op.getIndex(); }))
        onSC |= true;
    }
  }
  return onSC;
}

bool instructionDependsOnGlobal(const MachineInstr &MI, const MachineFunction& MF) {
  for (const MachineOperand &MO : MI.operands()) {
    if (MO.isGlobal()) {
      const GlobalValue *GV = MO.getGlobal();
      if (isa<GlobalVariable>(GV)) {
        return true;
      }
    } else if (MO.isFI()) {
      //if (isFIAPointerToExternalSymbol(MO.getIndex(), MF)) return true;
      const MachineFrameInfo &MFI = MF.getFrameInfo();

      const AllocaInst *AI = MFI.getObjectAllocation(MO.getIndex());
      if (!AI && !MFI.isFixedObjectIndex(MO.getIndex()))
        return true;
      //if (isFIAPointerToExternalSymbol(, MF)) return true;
    }
  }
  return false;
}

void removeDepFIs(std::vector<MachineInstr *>& deps, MachineFunction &MF) {
  PatmosMachineFunctionInfo &PMFI =
      *MF.getInfo<PatmosMachineFunctionInfo>();

  for (const auto MI : deps) {
    for (const auto &op : MI->operands()) {
      if (op.isFI()) {
        PMFI.removeStackCacheAnalysisFIs(op.getIndex());
      }
    }
  }
}

bool PatmosStackCachePromotion::replaceOpcodeIfSC(unsigned OPold, unsigned OPnew, MachineInstr& MI,
                       MachineFunction &MF) {
  if (std::any_of(MI.operands_begin(), MI.operands_end(), [](auto element){return element.isFI();})) {
    return false;
  }

  if (MI.getOpcode() == OPold) {
    //LLVM_DEBUG(dbgs() << MI);

    auto Dependencies = getDeps(MI, MF);

    /*LLVM_DEBUG(dbgs() << "\tDepends on: \n");
    for (auto &DMI : Dependencies) {
      LLVM_DEBUG(dbgs() << "\t" << *DMI);
    }
    LLVM_DEBUG(dbgs() << "\n");*/

    if (Dependencies.size() == 0)
      return false;

    for (const auto& dep : Dependencies) {
      removeDepFIs(Dependencies, MF);
      if (instructionDependsOnGlobal(*dep, MF)) return false;
    }

    if (!hasFIonSC(Dependencies, MF)) {
      removeDepFIs(Dependencies, MF);
      return false;
    }
    //removeDepFIs(Dependencies, MF);
    return true;
    LLVM_DEBUG(dbgs() << "Updating op...\n");


    /*LLVM_DEBUG(dbgs() << "\tDepends on: \n");
    for (auto &DMI : Dependencies) {
      LLVM_DEBUG(dbgs() << *DMI);
    }
    LLVM_DEBUG(dbgs() << "\n");
    LLVM_DEBUG(dbgs() << "\n");*/

    MI.setDesc(TII->get(OPnew));
    return true;
  }
  return false;
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
    if (isa<GetElementPtrInst>(Inst) || isa<CallInst>(Inst)) {
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

bool PatmosStackCachePromotion::runOnMachineFunction(MachineFunction &MF) {
  if (EnableStackCachePromotion) {
    LLVM_DEBUG(dbgs() << "Enabled Stack Cache promotion for: "
                      << MF.getFunction().getName() << "\n");

    MachineFrameInfo &MFI = MF.getFrameInfo();
    PatmosMachineFunctionInfo &PMFI = *MF.getInfo<PatmosMachineFunctionInfo>();

    std::unordered_set<unsigned> stillPossibleFIs;
    for (unsigned FI = 0, FIe = MFI.getObjectIndexEnd(); FI != FIe; FI++) {
      if (!MFI.isFixedObjectIndex(FI) && MFI.isAliasedObjectIndex(FI) && !isFrameIndexUsedAsPointer(MF, FI)) {
        PMFI.addStackCacheAnalysisFI(FI);
      } else {
        stillPossibleFIs.insert(FI);
      }
    }

    /*const std::vector<std::pair<unsigned, unsigned>> mappings = {
        {Patmos::LWC, Patmos::LWS},
        {Patmos::LHC, Patmos::LHS},
        {Patmos::LBC, Patmos::LBS},
        {Patmos::LHUC, Patmos::LHUS},
        {Patmos::LBUC, Patmos::LBUS},

        {Patmos::SWC, Patmos::SWS},
        {Patmos::SHC, Patmos::SHS},
        {Patmos::SBC, Patmos::SBS},
    };

    for (auto &BB : MF) {
      for (auto &MI : BB) {
        std::any_of(mappings.begin(), mappings.end(), [&MI, &MF, this](auto elem){return replaceOpcodeIfSC(elem.first, elem.second, MI, MF);});
      }
    }*/

    // Iterate over every Instr x
    // For each x -> find instrs y that x depends on && transitively
    // for each y -> if has FI && Is on SC -> convert to L.S/S.S

    for (const int FI : PMFI.getStackCacheAnalysisFIs()) {
      LLVM_DEBUG(dbgs() << "FI on Stack Cache: " << FI << "\n");
    }
  }
  return true;
}
