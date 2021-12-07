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

  copyRel = R_RISCV_COPY;
  noneRel = R_RISCV_NONE;
  pltRel = R_RISCV_JUMP_SLOT;
  relativeRel = R_RISCV_RELATIVE;
  iRelativeRel = R_RISCV_IRELATIVE;
  symbolicRel = R_RISCV_32;
  tlsModuleIndexRel = R_RISCV_TLS_DTPMOD32;
  tlsOffsetRel = R_RISCV_TLS_DTPREL32;
  tlsGotRel = R_RISCV_TLS_TPREL32;
  gotRel = symbolicRel;

  // .got[0] = _DYNAMIC
  gotBaseSymInGotPlt = false;
  gotHeaderEntriesNum = 1;

  // .got.plt[0] = _dl_runtime_resolve, .got.plt[1] = link_map
  gotPltHeaderEntriesNum = 2;

  pltHeaderSize = 32;
  pltEntrySize = 16;
  ipltEntrySize = 16;

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
  write32be(buf, mainPart->dynamic->getVA());
}

void Patmos::writeGotPlt(uint8_t *buf, const Symbol &s) const {
  // TODO: Find out, why this Method exists in other Targets ?
  write32be(buf, in.plt->getVA());    
}

void Patmos::writePltHeader(uint8_t *buf) const {
  // TODO: Find out, why this Method exists in other Targets ?
}

void Patmos::writePlt(uint8_t *buf, const Symbol &sym,
                     uint64_t pltEntryAddr) const {
  // TODO: Find out, why this Method exists in other Targets ?
}

RelType Patmos::getDynRel(RelType type) const {
  return type == target->symbolicRel ? type : static_cast<RelType>(R_RISCV_NONE);
}

RelExpr Patmos::getRelExpr(const RelType type, const Symbol &s,
                          const uint8_t *loc) const {
  // TODO: Find out, why this Method exists in other Targets ?

  // different Instruction Types to later calculate the Values in relocate

  switch (type) {
  case R_PATMOS_NONE:
  case R_PATMOS_CFLI_ABS:
  case R_PATMOS_ALUI_ABS:
  case R_PATMOS_ALUL_ABS:
  case R_PATMOS_MEMB_ABS:
  case R_PATMOS_MEMH_ABS:
  case R_PATMOS_MEMW_ABS:
  case R_PATMOS_ABS_32:
  case R_PATMOS_CFLI_PCREL:
    return R_NONE;
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
  // Temporarly left at Example implementation in RISCV.cpp - except change from Little Endian to Big Endian
  // Relocate Types defined in /llvm/include/llvm/BinaryFormat/ELF.h
  
  

  const unsigned bits = config->wordsize * 8;

  switch (rel.type) {
  case R_PATMOS_NONE:
  case R_PATMOS_CFLI_ABS:
  case R_PATMOS_ALUI_ABS:
  case R_PATMOS_ALUL_ABS:
  case R_PATMOS_MEMB_ABS:
  case R_PATMOS_MEMH_ABS:
  case R_PATMOS_MEMW_ABS:
  case R_PATMOS_ABS_32:
  case R_PATMOS_CFLI_PCREL:
    write32le(loc,val);
    return;
  default:
    llvm_unreachable("unknown relocation");
  }
}

TargetInfo *elf::getPatmosTargetInfo() {
  static Patmos target;
  return &target;
}
