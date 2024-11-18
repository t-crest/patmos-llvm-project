/*
 * This code (and the .cpp file) are copies of the original code that was removed from
 * llvm/include/llvm/Transforms/Utils/UnifyFunctionExitNodes.h and
 * llvm/lib/Transforms/Utils/UnifyFunctionExitNodes.cpp
 * in git commit 3cc523d935427baf62766e9e2cc7b65eca5925bb
 *
 * It was removed because no one used it, so we move it here because it is critical for
 * the single-path transformation.
 */

#ifndef TARGET_PATMOS_SINGLEPATH_UNIFYFUNCTIONEXITNODESLEGACYPASS_H_
#define TARGET_PATMOS_SINGLEPATH_UNIFYFUNCTIONEXITNODESLEGACYPASS_H_

#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {

class BasicBlock;

class UnifyFunctionExitNodesLegacyPass : public FunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid
  UnifyFunctionExitNodesLegacyPass();

  // We can preserve non-critical-edgeness when we unify function exit nodes
  void getAnalysisUsage(AnalysisUsage &AU) const override;

  bool runOnFunction(Function &F) override;
};

Pass *createUnifyFunctionExitNodesPass();

} // end namespace llvm

#endif /* TARGET_PATMOS_SINGLEPATH_UNIFYFUNCTIONEXITNODESLEGACYPASS_H_ */
