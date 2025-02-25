#ifndef TARGET_PATMOS_PATMOSINTRINSICELIMINATION_H_
#define TARGET_PATMOS_PATMOSINTRINSICELIMINATION_H_

#include "Patmos.h"

#define DEBUG_TYPE "patmos-intrinsic-elimination"

namespace llvm {

class PatmosIntrinsicElimination : public FunctionPass {
public:
  // Pass identification, replacement for typeid
  static char ID;

  PatmosIntrinsicElimination() : FunctionPass(ID) {

  }

  /// getPassName - Return the pass' name.
  StringRef getPassName() const override {
    return "Patmos Instrinsic Elimination Pass";
  }

  bool runOnFunction(Function &F) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {

  }
};
}

#endif /* TARGET_PATMOS_PATMOSINTRINSICELIMINATION_H_ */
