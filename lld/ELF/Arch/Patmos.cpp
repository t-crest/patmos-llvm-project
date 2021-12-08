//===- Patmos.cpp ----------------------------------------------------------===//
//
// Addaption of File lld/ELF/Arch/RISCV.cpp of the LLVM Project, 
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "Symbols.h"
#include "SyntheticSections.h"
#include "Target.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {

class Patmos final : public TargetInfo {
public:
  Patmos();
  uint32_t calcEFlags() const override;
  void writeGotHeader(uint8_t *buf) const override;
  void writeGotPlt(uint8_t *buf, const Symbol &s) const override;
  void writePltHeader(uint8_t *buf) const override;
  void writePlt(uint8_t *buf, const Symbol &sym,
                uint64_t pltEntryAddr) const override;
  RelType getDynRel(RelType type) const override;
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};

} // end anonymous namespace

// How to hande
// difference 32-bit and 64-bit bundle format


Patmos::Patmos() {
  noneRel = R_PATMOS_NONE;
  
  defaultCommonPageSize = 0x1000;
  defaultMaxPageSize = 0x1000;
}

static uint32_t getEFlags(InputFile *f) {
  // TODO: Find out, why this Method exists in other Targets ?
  return cast<ObjFile<ELF32BE>>(f)->getObj().getHeader().e_flags;
}

uint32_t Patmos::calcEFlags() const {
  // FLAGS for other Targest are defined in llvm/BinaryFormat/ELF.h
  // Where is this Method calcEFlags() needed in lld 

  // If there are only binary input files (from -b binary), use a
  // value of 0 for the ELF header flags.
  if (objectFiles.empty())
    return 0;

  uint32_t target = getEFlags(objectFiles.front());

  return target;
}

void Patmos::writeGotHeader(uint8_t *buf) const {
  // TODO: Find out, why this Method exists in other Targets ?
  // GOT - Global Operations Table

  write32be(buf, mainPart->dynamic->getVA());  
}

void Patmos::writeGotPlt(uint8_t *buf, const Symbol &s) const {
  // TODO: Find out, why this Method exists in other Targets ?
  // PLT - Procedure Logik Table

  write32be(buf, mainPart->dynamic->getVA());    
}

void Patmos::writePltHeader(uint8_t *buf) const {
  // TODO: Find out, why this Method exists in other Targets ?
}

void Patmos::writePlt(uint8_t *buf, const Symbol &sym,
                     uint64_t pltEntryAddr) const {
  // TODO: Find out, why this Method exists in other Targets ?
}

RelType Patmos::getDynRel(RelType type) const {
  return type == target->symbolicRel ? type : static_cast<RelType>(R_PATMOS_NONE);
}

RelExpr Patmos::getRelExpr(const RelType type, const Symbol &s,
                          const uint8_t *loc) const {
  // get which Relocation Value is returned

  switch (type) {
  case R_PATMOS_NONE:
    return R_NONE;
  case R_PATMOS_CFLI_ABS:
    return R_ABS;  
  case R_PATMOS_ALUI_ABS:
    return R_ABS;  
  case R_PATMOS_ALUL_ABS:
    return R_ABS; 
  case R_PATMOS_MEMB_ABS:
    return R_ABS; 
  case R_PATMOS_MEMH_ABS:
    return R_ABS; 
  case R_PATMOS_MEMW_ABS:
    return R_ABS; 
  case R_PATMOS_ABS_32:
    return R_ABS;  
  case R_PATMOS_CFLI_PCREL:
    return R_PC;
  default:
    error(getErrorLocation(loc) + "unknown relocation (" + Twine(type) +
          ") against symbol " + toString(s));
    return R_NONE;
  }
}

// Extract bits V[Begin:End], where range is inclusive, and Begin must be < 63.
static uint32_t extractBits(uint64_t v, uint32_t begin, uint32_t end) {
  return (v & ((1ULL << (begin + 1)) - 1)) >> end;
}

void Patmos::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  // Relocate Types defined in /llvm/include/llvm/BinaryFormat/ELF.h
  

  switch (rel.type) {
  case R_PATMOS_CFLI_ABS: {
    checkInt(loc, static_cast<int64_t>(val) >> 2, 22, rel);

    uint32_t insn = read32be(loc);
    uint32_t imm =  extractBits(val, 23, 2);
    insn |= imm;
    write32be(loc,insn);
    return;
  }
  case R_PATMOS_ALUI_ABS:
    //error("Loc:" + getErrorLocation(loc) + " val: " + toString(val));
  case R_PATMOS_ALUL_ABS:
  case R_PATMOS_MEMB_ABS:
  case R_PATMOS_MEMH_ABS:
  case R_PATMOS_MEMW_ABS:
  case R_PATMOS_ABS_32:
    return;
  case R_PATMOS_CFLI_PCREL: {
    checkInt(loc, static_cast<int64_t>(val) >> 2, 22, rel);

    uint32_t insn = read32be(loc);
    uint32_t imm =  extractBits(val, 23, 2);
    insn |= imm;
    write32be(loc,insn);
    return;
  }
  default:
    llvm_unreachable("unknown relocation");
  }
}

TargetInfo *elf::getPatmosTargetInfo() {
  static Patmos target;
  return &target;
}
