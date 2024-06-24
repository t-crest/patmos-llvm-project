//===-- PatmosStackCachePromotion.h - Promote values ti the stack-cache. -=====//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#ifndef PATMOSSTACKCACHEPROMOTION
#define PATMOSSTACKCACHEPROMOTION

#include "Patmos.h"
#include "PatmosTargetMachine.h"
#include "PatmosRegisterInfo.h"


#include <llvm/IR/Module.h>
#include <llvm/CodeGen/MachineFunctionPass.h>

#define DEBUG_TYPE "patmos-stack-cache-promotion"

namespace llvm {

class PatmosStackCachePromotion : public MachineFunctionPass {

private:

  const PatmosTargetMachine &TM;
  const PatmosSubtarget &STC;
  const PatmosInstrInfo *TII;
  const PatmosRegisterInfo *TRI;

  std::set<Function*> annotFuncs;

  void getAnnotatedFunctions(Module *M){
    for (Module::global_iterator I = M->global_begin(),
                                 E = M->global_end();
         I != E;
         ++I) {

      if (I->getName() == "llvm.global.annotations") {
        ConstantArray *CA = dyn_cast<ConstantArray>(I->getOperand(0));
        for(auto OI = CA->op_begin(); OI != CA->op_end(); ++OI){
          ConstantStruct *CS = dyn_cast<ConstantStruct>(OI->get());
          Function *FUNC = dyn_cast<Function>(CS->getOperand(0)->getOperand(0));
          GlobalVariable *AnnotationGL = dyn_cast<GlobalVariable>(CS->getOperand(1)->getOperand(0));
          StringRef annotation = dyn_cast<ConstantDataArray>(AnnotationGL->getInitializer())->getAsCString();
          if(annotation.compare("stack_cache")==0){
            annotFuncs.insert(FUNC);
            errs() << "Found annotated function " << FUNC->getName()<<"\n";
          }
        }
      }
    }
  }

  bool shouldInstrumentFunc(Function &F){
    if (F.isDeclaration()) {
      return false;
    }

    // Additional heuristic: Check for specific function names that are commonly found in libraries
    StringRef FunctionName = F.getName();
    if (FunctionName.startswith("_Z") || FunctionName.startswith("__") || FunctionName.startswith("llvm.")) {
      return false;
    }

    // Additional module-level checks
    const Module *M = F.getParent();
    if (M) {
      // Optionally check the module name to see if it matches common library patterns
      // Example: Functions from standard libraries often have specific module identifiers
      StringRef ModuleIdentifier = M->getModuleIdentifier();
      if (ModuleIdentifier.endswith(".so") || ModuleIdentifier.endswith(".a") || ModuleIdentifier.contains("lib")) {
        return false;
      }
    }

    // By default, assume the function is defined in the current compilation unit
    return true;
    //return F.hasInternalLinkage() || F.hasPrivateLinkage() || F.hasLinkOnceODRLinkage() || F.hasLinkOnceLinkage();
    //return !F.isDeclaration();
    //return annotFuncs.find(&F)!=annotFuncs.end();
  }

public:
  static char ID;
  PatmosStackCachePromotion(const PatmosTargetMachine &tm):
                                                              MachineFunctionPass(ID), TM(tm),
                                                              STC(*tm.getSubtargetImpl()),
                                                              TII(static_cast<const PatmosInstrInfo*>(tm.getInstrInfo())),
                                                              TRI(static_cast<const PatmosRegisterInfo*>(tm.getRegisterInfo()))
  {}

  /// getPassName - Return the pass' name.
  StringRef getPassName() const override {
    return "Patmos StackCache-Promotion pass (machine code)";
  }

  virtual bool doInitialization(Module &M) override{
    getAnnotatedFunctions(&M);
    return false;
  }

  void processMachineInstruction(MachineInstr& MI);
  void calcOffsets(MachineFunction& MF);
  bool runOnMachineFunction(MachineFunction &MF) override ;

  bool replaceOpcodeIfSC(unsigned int OPold, unsigned int OPnew,
                         MachineInstr &MI, MachineFunction &MF);
};

} // End llvm namespace

#endif
