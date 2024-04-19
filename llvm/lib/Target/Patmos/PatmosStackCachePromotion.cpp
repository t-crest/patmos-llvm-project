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
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

STATISTIC(LoadsConverted,
          "Number of data-cache loads converted to stack cache loads");
STATISTIC(SavesConverted,
          "Number of data-cache saves converted to stack cache saves");

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

void PatmosStackCachePromotion::processMachineInstruction(
    MachineInstr& MI) {
  // TODO Convert access to SC access

  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();


  unsigned FIOperandNum;
  int FrameIndex;
  if (TII->isLoadFromStackSlot(MI, FrameIndex)) { // For some reason the FrameDisplacement operand [4] sometimes is not 0 here?
    FIOperandNum = 3;
  } else if (TII->isStoreToStackSlot(MI, FrameIndex)) { // For some reason the FrameDisplacement operand [3] sometimes is not 0 here?
    FIOperandNum = 2;
  } else {
    return;
  }

  int FrameOffset       = MFI.getObjectOffset(FrameIndex);
  int FrameDisplacement = MI.getOperand(FIOperandNum+1).getImm();

  int Offset = FrameOffset;

  unsigned opcode = MI.getOpcode();

  // ensure that the offset fits the instruction
  switch (opcode)
  {
  case Patmos::LWC: case Patmos::LWM:
  case Patmos::SWC: case Patmos::SWM:
  case Patmos::PSEUDO_PREG_SPILL:
  case Patmos::PSEUDO_PREG_RELOAD:
    // 9 bit
    assert((Offset & 0x3) == 0);
    Offset = (Offset >> 2) + FrameDisplacement;
    break;
  case Patmos::LHC: case Patmos::LHM:
  case Patmos::LHUC: case Patmos::LHUM:
  case Patmos::SHC: case Patmos::SHM:
    // 8 bit
    assert((Offset & 0x1) == 0);
    Offset = (Offset >> 1) + FrameDisplacement;

    break;
  case Patmos::LBC: case Patmos::LBM:
  case Patmos::LBUC: case Patmos::LBUM:
  case Patmos::SBC: case Patmos::SBM:
    // 7 bit
    Offset += FrameDisplacement;
    break;
  case Patmos::ADDi:
    // 12 bit
    Offset += FrameDisplacement;
    break;
  case Patmos::ADDl:
  case Patmos::DBG_VALUE:
    // all should be fine
    Offset += FrameDisplacement;
    break;
  default:
    llvm_unreachable("Unexpected operation with FrameIndex encountered.");
  }

  switch (opcode)
  {
  case Patmos::LWC: case Patmos::LWM: opcode = Patmos::LWS; break;
  case Patmos::LHC: case Patmos::LHM: opcode = Patmos::LHS; break;
  case Patmos::LHUC: case Patmos::LHUM: opcode = Patmos::LHUS; break;
  case Patmos::LBC: case Patmos::LBM: opcode = Patmos::LBS; break;
  case Patmos::LBUC: case Patmos::LBUM: opcode = Patmos::LBUS; break;
  case Patmos::SWC: case Patmos::SWM: opcode = Patmos::SWS; break;
  case Patmos::SHC: case Patmos::SHM: opcode = Patmos::SHS; break;
  case Patmos::SBC: case Patmos::SBM: opcode = Patmos::SBS; break;
  case Patmos::ADDi: case Patmos::ADDl: case Patmos::DBG_VALUE:
    break;
  default:
    llvm_unreachable("Unexpected operation with FrameIndex encountered.");
  }

  // Update Data Cache instr to use Stack Cache
  const MCInstrDesc &newMCID = TII->get(opcode);
  MI.setDesc(newMCID);

  MI.getOperand(FIOperandNum).ChangeToRegister(Patmos::R0, false, false, false);
  MI.getOperand(FIOperandNum+1).ChangeToImmediate(Offset);

  /*

  Register dataReg;
  uint64_t basePtrReg;
  uint64_t unknown1;
  uint64_t offset;
  uint64_t FrameDisplacement;

  if (llvm::isMainMemLoadInst(MI.getOpcode())) {
    basePtrReg = MI.getOperand(1).getReg();
    dataReg = MI.getOperand(0).getReg();
    offset = MFI.getObjectOffset(MI.getOperand(3).getIndex());
    FrameDisplacement = MI.getOperand(4).getImm();
    unknown1 = MI.getOperand(2).getImm();

    auto dataRegName = TII->getRegisterInfo().getRegAsmName(dataReg);
    auto basePtrRegName =
        basePtrReg > 0 ? TII->getRegisterInfo().getRegAsmName(basePtrReg) : "?";

    LLVM_DEBUG(dbgs() << TII->getName(MI.getOpcode()) << " " << dataRegName
                      << " = "
                      << "[" << basePtrRegName << " + " << offset << "]"
                      << "\n");

  } else if (llvm::isMainMemStoreInst(MI.getOpcode())) {
    basePtrReg = MI.getOperand(0)
                     .getReg(); // This is not known yet at this point in time
    dataReg = MI.getOperand(4).getReg();
    offset = MFI.getObjectOffset(
        MI.getOperand(2)
            .getIndex()); // This is not final yet at this point in time
    FrameDisplacement = MI.getOperand(3).getImm(); // Always has to be zero
    unknown1 = MI.getOperand(1).getImm();

    auto dataRegName = TII->getRegisterInfo().getRegAsmName(dataReg);
    auto basePtrRegName =
        basePtrReg > 0 ? TII->getRegisterInfo().getRegAsmName(basePtrReg) : "?";

    LLVM_DEBUG(dbgs() << TII->getName(MI.getOpcode()) << "[" << basePtrRegName
                      << " + " << offset << "] = " << dataRegName << "\n");
  } else {
    return;
  }*/
}

void PatmosStackCachePromotion::calcOffsets(MachineFunction& MF) {
  MachineFrameInfo &MFI = MF.getFrameInfo();

  SmallVector<int, 8> ObjectsToAllocate;

  for (unsigned i = 0, e = MFI.getObjectIndexEnd(); i != e; ++i) {
    if (MFI.isObjectPreAllocated(i) && MFI.getUseLocalStackAllocationBlock())
      continue;
    if (MFI.isDeadObjectIndex(i))
      continue;

    ObjectsToAllocate.push_back(i);
  }


  int64_t offset = 0;
  Align maxalign = MFI.getMaxAlign();
  for (auto &FrameIdx : ObjectsToAllocate) {
    // AdjustStackOffset
    Align Alignment = MFI.getObjectAlign(FrameIdx);

    maxalign = std::max(maxalign, Alignment);
    offset = alignTo(offset, Alignment);

    MFI.setObjectOffset(FrameIdx, offset);
    offset += MFI.getObjectSize(FrameIdx);

  }
}

bool PatmosStackCachePromotion::runOnMachineFunction(MachineFunction &MF) {
  if (EnableStackCachePromotion) {
    LLVM_DEBUG(dbgs() << "Enabled Stack Cache promotion for: "
                      << MF.getFunction().getName() << "\n");

    // Calculate the amount of bytes to store on SC
    MachineFrameInfo &MFI = MF.getFrameInfo();
    unsigned stackSize = MFI.estimateStackSize(MF);

    if (!stackSize)
      return true; // prevent sres 0

    if (stackSize > STC.getStackCacheSize())
      return true; // TODO right now we simply abort and therefore push everything to the shadow stack, but only the exceeding data should be put on the shadow stack

    LLVM_DEBUG(dbgs() << "Storing " << MFI.getObjectIndexEnd()
                      << " MFIs resulting in " << stackSize
                      << " bytes to SC\n");

    // Insert Reserve before function start
    auto startofblock = MF.begin();
    auto startInstr = startofblock->begin();

    auto *reserveInstr = MF.CreateMachineInstr(TII->get(Patmos::SRESi),
                                              startInstr->getDebugLoc());
    MachineInstrBuilder reserveInstrBuilder(MF, reserveInstr);
    AddDefaultPred(reserveInstrBuilder);
    reserveInstrBuilder.addImm(stackSize);

    startofblock->insert(startInstr, reserveInstr);

    calcOffsets(MF);

    for (auto& BB : MF) {
      for (auto& instr : BB) {
        processMachineInstruction(instr);
      }
    }

    // Insert Free after function End
    auto endofBlock = MF.rbegin();
    auto endInstr = endofBlock->rbegin();

    auto *freeInstr = MF.CreateMachineInstr(TII->get(Patmos::SFREEi),
                                            endInstr->getDebugLoc());
    MachineInstrBuilder freeInstrBuilder(MF, freeInstr);
    AddDefaultPred(freeInstrBuilder);
    freeInstrBuilder.addImm(stackSize);

    endofBlock->insert(endInstr->getIterator(), freeInstr);

    //auto *ensInstr = MF.CreateMachineInstr(TII->get(Patmos::SENSr),
    //                                        endInstr->getDebugLoc());
    //MachineInstrBuilder ensInstrBuilder(MF, ensInstr);
    //AddDefaultPred(ensInstrBuilder);
    //ensInstrBuilder.addImm(stackSize);

    //endofBlock->insert(endInstr->getIterator(), ensInstr);
  }
  return true;
}
