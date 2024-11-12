//===- Patmos.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// Patmos ABI Implementation
//===----------------------------------------------------------------------===//
namespace {
class PatmosABIInfo : public DefaultABIInfo {
public:
  PatmosABIInfo(CodeGen::CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}
};

class PatmosTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  PatmosTargetCodeGenInfo(CodeGenTypes &CGT)
    : TargetCodeGenInfo(std::make_unique<PatmosABIInfo>(CGT)) {}

  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &CGM) const override {
    const FunctionDecl *FD = dyn_cast<FunctionDecl>(D);
    if (!FD) return;
    llvm::Function *Fn = cast<llvm::Function>(GV);
    auto is_cet_fun = CGM.getCodeGenOpts().PatmosEnableCet &&
        std::any_of(CGM.getCodeGenOpts().PatmosCetFuncs.begin(), CGM.getCodeGenOpts().PatmosCetFuncs.end(),
            [&](auto fn_name){ return fn_name == FD->getName();});
    if (FD->hasAttr<SinglePathAttr>() || is_cet_fun) {
      Fn->addFnAttr("sp-root");
      Fn->addFnAttr(llvm::Attribute::NoInline);
    }
  }
};
}

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createPatmosTargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<PatmosTargetCodeGenInfo>(CGM.getTypes());
}