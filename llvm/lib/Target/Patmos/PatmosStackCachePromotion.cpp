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
                      [&op](auto elem) { return elem == op.getIndex(); }))
        return true;
    }
  }
  return false;
}

bool PatmosStackCachePromotion::replaceOpcodeIfSC(unsigned OPold, unsigned OPnew, MachineInstr& MI,
                       MachineFunction &MF) {
  if (MI.getOpcode() == OPold) {
    auto Dependencies = getDeps(MI, MF);
    if (Dependencies.size() == 0)
      return false;

    if (!hasFIonSC(Dependencies, MF))
      return false;

    MI.setDesc(TII->get(OPnew));

    MI.dump();
    LLVM_DEBUG(dbgs() << "\tDepends on: \n");
    for (auto &DMI : Dependencies) {
      LLVM_DEBUG(dbgs() << *DMI);
    }
    LLVM_DEBUG(dbgs() << "\n");
    LLVM_DEBUG(dbgs() << "\n");
    return true;
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
      LLVM_DEBUG(dbgs() << "Adding FI to Stack Cache: " << FI << "\n");
      PMFI.addStackCacheAnalysisFI(FI);
    }

    const std::vector<std::pair<unsigned, unsigned>> mappings = {
        {Patmos::LWC, Patmos::LWS},
        /*{Patmos::LHC, Patmos::LHS},
        {Patmos::LBC, Patmos::LBS},
        {Patmos::LHUC, Patmos::LHUS},
        {Patmos::LBUC, Patmos::LBUS},*/

        /*{Patmos::SWC, Patmos::SWS},
        {Patmos::SHC, Patmos::SHS},
        {Patmos::SBC, Patmos::SBS},*/
    };

    for (auto &BB : MF) {
      for (auto &MI : BB) {
        std::any_of(mappings.begin(), mappings.end(), [&MI, &MF, this](auto elem){return replaceOpcodeIfSC(elem.first, elem.second, MI, MF);});
        /*replaceOpcodeIfSC(Patmos::LWC, Patmos::LWS, MI, MF);
        replaceOpcodeIfSC(Patmos::LHC, Patmos::LHS, MI, MF);
        replaceOpcodeIfSC(Patmos::LBC, Patmos::LBS, MI, MF);
        replaceOpcodeIfSC(Patmos::LHUC, Patmos::LHUS, MI, MF);
        replaceOpcodeIfSC(Patmos::LBUC, Patmos::LBUS, MI, MF);

        replaceOpcodeIfSC(Patmos::SWC, Patmos::SWS, MI, MF);
        replaceOpcodeIfSC(Patmos::SHC, Patmos::SHS, MI, MF);
        replaceOpcodeIfSC(Patmos::SBC, Patmos::SBS, MI, MF);*/
      }
    }

    // Iterate over every Instr x
    // For each x -> find instrs y that x depends on && transitively
    // for each y -> if has FI && Is on SC -> convert to L.S/S.S
  }
  return true;
}
