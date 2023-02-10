#ifndef TARGET_PATMOS_SINGLEPATH_CONSTANTLOOPDOMINATORANALYSIS_H_
#define TARGET_PATMOS_SINGLEPATH_CONSTANTLOOPDOMINATORANALYSIS_H_

#include "llvm/CodeGen/MachineBasicBlock.h"
#include <set>
#include <map>
#include <queue>

#define DEBUG_TYPE "patmos-singlepath"

// Uncomment to always print debug info
//#define LLVM_DEBUG(...) __VA_ARGS__

namespace llvm {

/// Computes the intersection of two sets, returning the result without
/// changing the given sets
template<typename T>
static std::set<T> get_intersection(std::set<T>& x, std::set<T>& y){
  std::set<T> intersected;
  std::set_intersection(
	x.begin(), x.end(),
	y.begin(), y.end(),
	std::inserter(intersected, intersected.begin())
  );
  return intersected;
}

/// Pretty-prints the given list of MBBs to the given stream (doesn't end with space or newline)
template<typename OUT, typename T>
static OUT& dump_mbb_list(OUT &out, T& list)
{
  out << "{";
  for(auto x: list) {
    out << x->getName() << ", ";
  }
  out << "}";
  return out;
}

/// Returns the FCFG successors of "current" assuming start_mbb is the root of the FCFG
template<
  typename MachineBasicBlock,
  typename MachineLoopInfo
>
static std::set<MachineBasicBlock*> fcfg_successors(
	const MachineBasicBlock *current,
	const MachineBasicBlock *start_mbb,
	const MachineLoopInfo *LI
){
	std::set<MachineBasicBlock*> successors;

	// Collect possible successors
	if(current != start_mbb && LI->isLoopHeader(current)) {
		LLVM_DEBUG(dbgs() << "header block.\n");

		auto loop = LI->getLoopFor(current);

		// Use its exits to continue exploring the current loop
		SmallVector<std::pair<MachineBasicBlock*, MachineBasicBlock*>> exits;
		loop->getExitEdges(exits);
		for(auto exit: exits) {
			if( // Exit to this loop
			   LI->getLoopDepth(exit.second) == LI->getLoopDepth(start_mbb) ||
			   // Exit to header of next loop
			   LI->getLoopDepth(exit.second) == (LI->getLoopDepth(start_mbb) + 1)
			) {
			  successors.insert(exit.second);
			  LLVM_DEBUG(dbgs() << "Exit to current loop: "
					  << exit.first->getName() << " -> " << exit.second->getName() <<"\n");
			} else {
			  LLVM_DEBUG(dbgs() << "Exit to outer loop: "
					  << exit.first->getName() << " -> " << exit.second->getName() <<"\n");
			}
		}
	} else {
	  successors.insert(current->succ_begin(), current->succ_end());
	}

	// Remove any successors not in the FCFG
	auto iter = successors.begin();
	while(iter != successors.end()){
	  auto *succ = *iter;
	  if (
		// 'succ' is a block in the outer loop
		LI->getLoopDepth(succ) < LI->getLoopDepth(start_mbb) ||
		// 'succ' is a header of a loop in the outer loop
		(LI->getLoopDepth(succ) == LI->getLoopDepth(start_mbb) &&
			LI->getLoopFor(succ) != LI->getLoopFor(start_mbb)
		)
	  ) {
		LLVM_DEBUG(dbgs() << "Ignore exit target " << succ->getName() <<"\n");
		successors.erase(succ);
		iter = successors.begin();
	  } else if (succ == start_mbb) {
		LLVM_DEBUG(dbgs() << "Ignore back edge to " << succ->getName() <<"\n");
		successors.erase(succ);
		iter = successors.begin();
	  } else {
		iter++;
	  }
	}
	return successors;
};

/// Performs a modified dominator analysis, where the bounds of loops are taken into account.
/// A loop dominates its successors only if it has constant bounds, i.e., its minimum and maximum
/// iteration bounds are the same.
///
/// Returns the dominators of the end blocks of the loop identified by the given header
/// (or the entry block of a function) unless 'end_doms_only' is false.
///
/// The entry block of the function is assumed to not be the header of a loop.
///
/// Inputs:
/// * start_mbb: The entry block to start analysis from
/// * LI: MachineLoopInfo of the block's function
/// * constantBounds: function that given a loop header block returns whether the loop
///                   is constant (lower and upper bound are equal)
/// * end_doms_only: Whether resulting map should only contain the end blocks of the
///                  given function/loop/fcfg
///
/// This function is templated such that it can be tested. Normal use should just use LLVM's
/// MachineBasicBlock and MachineLoopInfo.
template<
  typename MachineBasicBlock,
  typename MachineLoopInfo
>
std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>>
constantLoopDominatorsAnalysis(
    const MachineBasicBlock *start_mbb,
    const MachineLoopInfo *LI,
    bool (*constantBounds)(const MachineBasicBlock *mbb),
	bool end_doms_only=true
) {
  LLVM_DEBUG(dbgs() << "\nStarting Bounded Dominator Analysis: " << start_mbb->getName() << "\n");
  std::set<const MachineBasicBlock*> end_mbbs;
  std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>> dominators;
  std::map<
  	  const MachineBasicBlock*,
  	  std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>>
  > loop_dominators;

  // Start by exploring the graph
  std::deque<const MachineBasicBlock *> queue;

  dominators[start_mbb] = {start_mbb};

  queue.push_back(start_mbb);
  while(!queue.empty()) {
      auto *current = queue.front();
      queue.pop_front();

      LLVM_DEBUG(dbgs() << "\nExplore next: " << current->getName() <<"\n");

      if(current->succ_size() == 0){
        // The end block
        end_mbbs.insert(current);
        LLVM_DEBUG(dbgs() << "End block.\n");
      } else {
    	auto is_header = [&](const MachineBasicBlock *block ){
    		return block != start_mbb && LI->isLoopHeader(block);
    	};

        for(auto succ: fcfg_successors(current, start_mbb, LI)){
    	  // Enqueue only if not already in queue
          if(std::find(queue.begin(), queue.end(), succ) == queue.end()){
        	  queue.push_back(succ);
          }

          // If loop header, analyze recursively (if not already analyzed)
          if(is_header(succ) && !loop_dominators.count(succ)) {
			LLVM_DEBUG(dbgs() << "successor header block: " << succ->getName() << ".\n");
			loop_dominators[succ] = constantLoopDominatorsAnalysis(succ, LI, constantBounds, false);
          }

          auto refine_with = dominators[current];
          if(is_header(succ) && constantBounds(succ)) {
			LLVM_DEBUG(dbgs() << "Refine " << succ->getName() << " with constant header.\n");
			auto loop = LI->getLoopFor(succ);

			// Find blocks that dominate all latches
			SmallVector<MachineBasicBlock*> latches;
			loop->getLoopLatches(latches);
			assert(latches.size() > 0 && "Must have at least 1 latch");
			auto latch_doms = loop_dominators[succ][latches[0]];
			for(auto latch: latches) {
			  latch_doms=get_intersection(latch_doms, loop_dominators[succ][latch]);
			}
			LLVM_DEBUG(dbgs() << "Latch doms: "; dump_mbb_list(dbgs(), latch_doms) << "\n");

			// Find blocks that dominate all exits
			SmallVector<MachineBasicBlock*> exits;
			loop->getExitingBlocks(exits);
			assert(exits.size() > 0 && "Must have at least 1 exit");
			auto exit_doms = loop_dominators[succ][exits[0]];
			for(auto exit: exits) {
			  exit_doms=get_intersection(exit_doms, loop_dominators[succ][exit]);
			}

			// Find blocks that are strictly dominated by all exits
			std::set<const MachineBasicBlock*> exit_set(exits.begin(), exits.end());
			std::set<const MachineBasicBlock*> doms_by_exits;
			for(auto block_doms: loop_dominators[succ]){
				if(!exit_set.count(block_doms.first) &&
					// All exits dominate the block
					std::all_of(exit_set.begin(), exit_set.end(),
						[&](auto exit){ return block_doms.second.count(exit);})
				){
					exit_doms.insert(block_doms.first);
				}
			}

			// Get all that adhere to both exits and latches
			auto intersected = get_intersection(latch_doms, exit_doms);

			refine_with.insert(intersected.begin(), intersected.end());
          }

          LLVM_DEBUG(
			  dbgs() << "Refine " << succ->getName() << " from: ";
          	  dump_mbb_list(dbgs(), dominators[succ]) <<

              "\nRefine " << succ->getName() << " with: ";
              dump_mbb_list(dbgs(), refine_with) << "\n"
          );

          if(dominators[succ].size() == 0) {
        	  // No refining done on this block before
        	  dominators[succ] = refine_with;
          } else {
        	  // Refine
        	  dominators[succ] = get_intersection(dominators[succ], refine_with);
          }
          // Only self dominant if in constant loop (even loop header)
          if(!is_header(succ) || constantBounds(succ)) {
        	  LLVM_DEBUG(dbgs() << "Self dominant.\n");
        	  dominators[succ].insert(succ); // Self domination
          }

          LLVM_DEBUG(
			  dbgs() << "Refine " << succ->getName() << " to: ";
			  dump_mbb_list(dbgs(), dominators[succ]) << "\n");
        }
      }
  }

  // Extract dominators of block within loops into result
  for(auto loop_doms: loop_dominators){
	  for(auto block_doms: loop_doms.second) {
		  dominators[block_doms.first] = dominators[loop_doms.first];
		  if(block_doms.first != loop_doms.first && constantBounds(loop_doms.first)) {
			  dominators[block_doms.first].insert(block_doms.second.begin(), block_doms.second.end());
		  }
	  }
  }

  if(end_doms_only) {
	  LLVM_DEBUG(dbgs() << "End dominators only\n");
	  std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>> end_doms;
	  for(auto end: end_mbbs){
	    end_doms[end] = dominators[end];
	  }
	  dominators = end_doms;
  }

  LLVM_DEBUG(
    dbgs() << "\nConstant-Loop Dominator Analysis Results: " << start_mbb->getName() << "\n";
    for(auto end: dominators){
      dbgs() << end.first->getName() << ": ";
      dump_mbb_list(dbgs(), end.second) << "\n";
    }
    dbgs() << "\n";
  );

  return dominators;
}

}
#endif /* TARGET_PATMOS_SINGLEPATH_CONSTANTLOOPDOMINATORANALYSIS_H_ */
