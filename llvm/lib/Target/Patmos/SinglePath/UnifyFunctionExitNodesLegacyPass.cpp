
#include "Patmos.h"
#include "UnifyFunctionExitNodesLegacyPass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/InitializePasses.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"
using namespace llvm;

char UnifyFunctionExitNodesLegacyPass::ID = 0;

UnifyFunctionExitNodesLegacyPass::UnifyFunctionExitNodesLegacyPass()
    : FunctionPass(ID) {
  initializeUnifyFunctionExitNodesLegacyPassPass(
    *PassRegistry::getPassRegistry());
}

INITIALIZE_PASS(UnifyFunctionExitNodesLegacyPass, "mergereturn",
                "Unify function exit nodes", false, false)

Pass *llvm::createUnifyFunctionExitNodesPass() {
  return new UnifyFunctionExitNodesLegacyPass();
}

void UnifyFunctionExitNodesLegacyPass::getAnalysisUsage(
    AnalysisUsage &AU) const {
  // We preserve the non-critical-edgeness property
  AU.addPreservedID(BreakCriticalEdgesID);
  // This is a cluster of orthogonal Transforms
  AU.addPreservedID(LowerSwitchID);
}

bool unifyUnreachableBlocks(Function &F) {
  std::vector<BasicBlock *> UnreachableBlocks;

  for (BasicBlock &I : F)
    if (isa<UnreachableInst>(I.getTerminator()))
      UnreachableBlocks.push_back(&I);

  if (UnreachableBlocks.size() <= 1)
    return false;

  BasicBlock *UnreachableBlock =
      BasicBlock::Create(F.getContext(), "UnifiedUnreachableBlock", &F);
  new UnreachableInst(F.getContext(), UnreachableBlock);

  for (BasicBlock *BB : UnreachableBlocks) {
    BB->back().eraseFromParent(); // Remove the unreachable inst.
    BranchInst::Create(UnreachableBlock, BB);
  }

  return true;
}

bool unifyReturnBlocks(Function &F) {
  std::vector<BasicBlock *> ReturningBlocks;

  for (BasicBlock &I : F)
    if (isa<ReturnInst>(I.getTerminator()))
      ReturningBlocks.push_back(&I);

  if (ReturningBlocks.size() <= 1)
    return false;

  // Insert a new basic block into the function, add PHI nodes (if the function
  // returns values), and convert all of the return instructions into
  // unconditional branches.
  BasicBlock *NewRetBlock = BasicBlock::Create(F.getContext(),
                                               "UnifiedReturnBlock", &F);

  PHINode *PN = nullptr;
  if (F.getReturnType()->isVoidTy()) {
    ReturnInst::Create(F.getContext(), nullptr, NewRetBlock);
  } else {
    // If the function doesn't return void... add a PHI node to the block...
    PN = PHINode::Create(F.getReturnType(), ReturningBlocks.size(),
                         "UnifiedRetVal");
    PN->insertInto(NewRetBlock, NewRetBlock->end());
    ReturnInst::Create(F.getContext(), PN, NewRetBlock);
  }

  // Loop over all of the blocks, replacing the return instruction with an
  // unconditional branch.
  for (BasicBlock *BB : ReturningBlocks) {
    // Add an incoming element to the PHI node for every return instruction that
    // is merging into this new block...
    if (PN)
      PN->addIncoming(BB->getTerminator()->getOperand(0), BB);

    BB->back().eraseFromParent(); // Remove the return insn
    BranchInst::Create(NewRetBlock, BB);
  }

  return true;
}

// Unify all exit nodes of the CFG by creating a new BasicBlock, and converting
// all returns to unconditional branches to this new basic block. Also, unify
// all unreachable blocks.
bool UnifyFunctionExitNodesLegacyPass::runOnFunction(Function &F) {
  bool Changed = false;
  Changed |= unifyUnreachableBlocks(F);
  Changed |= unifyReturnBlocks(F);
  return Changed;
}
