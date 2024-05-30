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

bool isFIusedInCall(MachineFunction &MF, unsigned FI) {
  for (const auto &MBB : MF) {
    for (const auto &MI : MBB) {
      if (MI.isCall()) {
        for (const MachineOperand &MO : MI.operands()) {
          if (MO.isFI()) {
            int FrameIndex = MO.getIndex();
            if (FrameIndex == FI)
              return true;
          }
        }
      }
    }
  }
  return false;
}

std::vector<MachineInstr *> getDeps(const MachineInstr &MI,
                                    const MachineFunction &MF) {
  const MachineRegisterInfo &MRI = MF.getRegInfo();
  std::vector<MachineInstr *> Dependencies;

  for (auto &MO : MI.operands()) {
    if (MO.isReg() && MO.isUse()) {
      unsigned Reg = MO.getReg();
      if (Register::isVirtualRegister(Reg)) {
        MachineInstr *DefMI = MRI.getVRegDef(Reg);
        if (DefMI) {
          auto lower = getDeps(*DefMI, MF);
          Dependencies.push_back(DefMI);
          Dependencies.insert(Dependencies.end(), lower.begin(), lower.end());
        }
      }
    }
  }
  return Dependencies;
}

bool hasFIonSC(const std::vector<MachineInstr *> deps,
               const MachineFunction &MF) {
  const PatmosMachineFunctionInfo &PMFI =
      *MF.getInfo<PatmosMachineFunctionInfo>();
  for (const auto MI : deps) {
    for (const auto &op : MI->operands()) {
      if (op.isFI() &&
          std::any_of(PMFI.getStackCacheAnalysisFIs().begin(),
                      PMFI.getStackCacheAnalysisFIs().end(),
                      [&op](auto elem) { return elem == op.isFI(); }))
        return true;
    }
  }
  return false;
}

bool PatmosStackCachePromotion::runOnMachineFunction(MachineFunction &MF) {
  if (EnableStackCachePromotion) {
    LLVM_DEBUG(dbgs() << "Enabled Stack Cache promotion for: "
                      << MF.getFunction().getName() << "\n");

    // Calculate the amount of bytes to store on SC
    MachineFrameInfo &MFI = MF.getFrameInfo();
    PatmosMachineFunctionInfo &PMFI = *MF.getInfo<PatmosMachineFunctionInfo>();

    for (unsigned FI = 0, FIe = MFI.getObjectIndexEnd(); FI != FIe; FI++) {
      // if (!MFI.isFixedObjectIndex(FI) && !isFIusedInCall(MF, FI))
      PMFI.addStackCacheAnalysisFI(FI);
    }

    for (auto &BB : MF) {
      for (auto &MI : BB) {
        if (MI.getOpcode() == Patmos::LWC) {
          auto Dependencies = getDeps(MI, MF);
          if (Dependencies.size() == 0)
            continue;

          if (!hasFIonSC(Dependencies, MF))
            continue;

          MI.setDesc(TII->get(Patmos::LWS));

          MI.dump();
          LLVM_DEBUG(dbgs() << "\tDepends on: \n");
          for (auto &DMI : Dependencies) {
            LLVM_DEBUG(dbgs() << *DMI);
          }
          LLVM_DEBUG(dbgs() << "\n");
          LLVM_DEBUG(dbgs() << "\n");
        }
      }
    }

    // Iterate over every Instr x
    // For each x -> find instrs y that x depends on && transitively
    // for each y -> if has FI && Is on SC -> convert to L.S/S.S
  }
  return true;
}
