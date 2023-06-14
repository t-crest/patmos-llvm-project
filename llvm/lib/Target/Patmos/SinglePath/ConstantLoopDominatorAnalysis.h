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

/// Returns the header of the outermost loop, containing the given loop ("inner"),
/// within the current loop ("limit").
template<
  typename MachineBasicBlock,
  typename MachineLoopInfo
>
static MachineBasicBlock* outermost_inner_loop_header(
		const MachineBasicBlock *inner,
		const MachineBasicBlock *limit,
		const MachineLoopInfo *LI
) {
	assert(LI->isLoopHeader(inner));
	assert(LI->isLoopHeader(limit) || limit->pred_size()==0);
	auto inner_loop = LI->getLoopFor(inner);
	auto limit_loop = LI->getLoopFor(limit);
	assert(inner && "Inner loop was null");

	auto outer_depth = limit_loop? limit_loop->getLoopDepth():0;

	assert(inner_loop->getLoopDepth() > (outer_depth));
	while(inner_loop->getLoopDepth() != (outer_depth+1)) {
		inner_loop = inner_loop->getParentLoop();
	}
	assert(limit_loop == inner_loop->getParentLoop());
	return inner_loop->getHeader();
}

template<
  typename MachineBasicBlock,
  typename MachineLoopInfo
>
static std::set<MachineBasicBlock*> fcfg_predecessors(
	const MachineBasicBlock *current,
	const MachineBasicBlock *start_mbb,
	const MachineLoopInfo *LI
){
	std::set<MachineBasicBlock*> predecessors;
	auto loop = LI->getLoopFor(start_mbb);

	for(auto pred_iter = current->pred_begin(); current != start_mbb && pred_iter != current->pred_end(); pred_iter++){
		auto pred = *pred_iter;
		auto *pred_loop = LI->getLoopFor(pred);

		if(pred == current) {
			continue;
		}

		if(pred_loop == loop) {
			predecessors.insert(pred);
		} else if(!loop || loop->contains(pred)) {
			auto header = outermost_inner_loop_header(pred_loop->getHeader(), start_mbb, LI);
			if(header != current) {
				/// Predecessor is in nested loop, use header of outermost inner loop
				predecessors.insert(header);
			}
		} else {
			/// Predecessor is in outer loop
		}
	}

	return predecessors;
}

/// Recursive implementation of fcfg_topo_sort.
/// "current" is the current block of the sort to explore.
/// "start_mbb" is the fcfg header node.
template<
  typename MachineBasicBlock,
  typename MachineLoopInfo
>
static void fcfg_topo_sort_rec(
		const MachineBasicBlock* current,
		const MachineBasicBlock *start_mbb,
		const MachineLoopInfo *LI,
		std::deque<const MachineBasicBlock*> &stack
){
	if(std::find(stack.begin(), stack.end(), current) == stack.end()){
		auto succs = fcfg_successors(current, start_mbb, LI);
		if(!succs.empty()) {
			for(auto succ: succs) {
				fcfg_topo_sort_rec(succ, start_mbb, LI, stack);
			}
		}
		stack.push_back(current);
	}
}

/// Returns the reverse topological sort of the fcfg with the given start node.
/// The first element in the sort if the last of the given list.
/// Use ".pop_back" to get the correct ordering of the sort.
template<
  typename MachineBasicBlock,
  typename MachineLoopInfo
>
static std::deque<const MachineBasicBlock*> fcfg_topo_sort(
		const MachineBasicBlock *start_mbb,
		const MachineLoopInfo *LI
){
	std::deque<const MachineBasicBlock*> stack;
	fcfg_topo_sort_rec(start_mbb, start_mbb, LI, stack);
	return stack;
}

/// Recursive version of 'constantLoopDominatorsAnalysis'.
///
/// Returns the cl-dominators of each block (i.e. x -> y, means y cl-dom y).
/// Also returns the set that would cl-dominate a node which follows the loop
/// if such a node existed (and regardless of whether such a node exists).
/// This is used for calculating cl-doms of outer loops. This second result
/// is not useful when returning from analyzing the function entry
///
/// The entry block of the function is assumed to not be the header of a loop.
///
/// Inputs:
/// * start_mbb: The header of the loop to be analyzed (or the function entry initially)
/// * LI: MachineLoopInfo of the block's function
/// * constantBounds: function that given a loop header block returns whether the loop
///                   is constant (lower and upper bound are equal)
///
/// This function is templated such that it can be tested. Normal use should just use LLVM's
/// MachineBasicBlock and MachineLoopInfo.
template<
  typename MachineBasicBlock,
  typename MachineLoopInfo
>
static std::pair<
  std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>>,
  std::set<const MachineBasicBlock*>
>
constantLoopDominatorsAnalysisImpl(
    const MachineBasicBlock *start_mbb,
    const MachineLoopInfo *LI,
    bool (*constantBounds)(const MachineBasicBlock *mbb)
) {
  LLVM_DEBUG(dbgs() << "\nStarting Bounded Dominator Analysis: " << start_mbb->getName() << "\n");
  std::set<const MachineBasicBlock*> end_mbbs;
  std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>> dominators;
  std::map<
    const MachineBasicBlock*,
    std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>>
  > loop_dominators;
  std::map<
    const MachineBasicBlock*,
    std::set<const MachineBasicBlock*>
  > loop_misc;
  auto is_fcfg_header = [&](const MachineBasicBlock *block ){
    return block != start_mbb && LI->isLoopHeader(block);
  };
  auto rev_topo = fcfg_topo_sort(start_mbb, LI);

  while(!rev_topo.empty()) {
	auto * current = rev_topo.back();
	rev_topo.pop_back();

	// If loop header, analyze recursively
	if(is_fcfg_header(current)) {
	  assert(!loop_dominators.count(current) && "Loop already recursively handled");
	  LLVM_DEBUG(dbgs() << "Header block: "<< current->getName() <<"\n");
	  auto loop_result = constantLoopDominatorsAnalysisImpl(current, LI, constantBounds);
	  loop_dominators[current] = loop_result.first;
	  loop_misc[current] = loop_result.second;
	}

	LLVM_DEBUG(
	  dbgs() << "\nNext: " << current->getName() <<"\n";
	  dbgs() << "FCFG pred: [";
	  for(auto pred: fcfg_predecessors(current, start_mbb, LI)){
        LLVM_DEBUG(dbgs() << pred->getName() <<", ");
	  }
	  dbgs() << "]\n"
	);

	Optional<std::set<const MachineBasicBlock*>> pred_doms_intersect;
    for(auto pred: fcfg_predecessors(current, start_mbb, LI)){
      assert(dominators.count(pred) && "Predecessor hasn't been handled");

      std::set<const MachineBasicBlock*> pred_doms = dominators[pred];
      if(is_fcfg_header(pred)) {
        assert(loop_misc.count(pred) && "Predecessor hasn't been handled");

        if(constantBounds(pred)) {
          pred_doms.insert(loop_misc[pred].begin(), loop_misc[pred].end());
        }
      }

      if(pred_doms_intersect) {
    	  pred_doms_intersect = get_intersection(*pred_doms_intersect, pred_doms);
      } else {
    	  pred_doms_intersect = pred_doms;
      }
    }
    if(pred_doms_intersect){
      dominators[current] = *pred_doms_intersect;
    }

    // Self dominant if const header (or not header)
    if(!is_fcfg_header(current) || constantBounds(current)) {
      dominators[current].insert(current);
    }
  }

  // Extract dominators of block within loops into result
  for(auto loop_doms: loop_dominators){
    auto header = loop_doms.first;
    for(auto block_doms: loop_doms.second) {
      auto block = block_doms.first;
      dominators[block] = dominators[header];
      if(block != header && constantBounds(header)) {
        dominators[block].insert(block_doms.second.begin(), block_doms.second.end());
      }
    }
  }

  auto loop = LI->getLoopFor(start_mbb);
  std::set<const MachineBasicBlock*> misc;
  if(loop){
	auto extract_if_header = [&](auto mbb){
	  auto mbb_loop = LI->getLoopFor(mbb);
	  if(mbb_loop != loop) {
		auto mbb_outer_header = outermost_inner_loop_header(mbb_loop->getHeader(), start_mbb, LI);
        assert(loop_misc.count(mbb_outer_header));
        auto header_misc = dominators[mbb_outer_header];
        if(constantBounds(mbb_outer_header)) {
            header_misc.insert(loop_misc[mbb_outer_header].begin(), loop_misc[mbb_outer_header].end());
        }
        return header_misc;
	  } else {
		return dominators[mbb];
	  }
	};


    // Find blocks that dominate all latches
    SmallVector<MachineBasicBlock*> latches;
    loop->getLoopLatches(latches);
    assert(latches.size() > 0 && "Must have at least 1 latch");
    auto latch_doms = extract_if_header(latches[0]);
    for(auto latch: latches) {
      auto latch_dom_set = extract_if_header(latch);
      latch_doms=get_intersection(latch_doms, latch_dom_set);
    }
    LLVM_DEBUG(dbgs() << "Latch doms: "; dump_mbb_list(dbgs(), latch_doms) << "\n");

    // Find blocks that dominate all exits
    SmallVector<MachineBasicBlock*> exits;
    loop->getExitingBlocks(exits);
    assert(exits.size() > 0 && "Must have at least 1 exit");
    auto exit_doms = extract_if_header(exits[0]);
    for(auto exit: exits) {
      auto exit_dom_set = extract_if_header(exit);
      exit_doms=get_intersection(exit_doms, exit_dom_set);
    }

    // Find blocks that are strictly dominated by all exits
    std::set<const MachineBasicBlock*> exit_set(exits.begin(), exits.end());
    std::set<const MachineBasicBlock*> doms_by_exits;

    for(auto dom: dominators) {
	  auto block = dom.first;
      if(!exit_set.count(block) &&
        // All exits dominate the block
        std::all_of(exit_set.begin(), exit_set.end(),
          [&](auto exit){ return exit != block && extract_if_header(block).count(exit);})
      ){
        exit_doms.insert(block);
      }
    }

    // Get all that adhere to both exits and latches
    misc = get_intersection(latch_doms, exit_doms);
  }

  LLVM_DEBUG(
    dbgs() << "\nConstant-Loop Dominator Analysis Results: " << start_mbb->getName() << "\n";
    for(auto end: dominators){
      dbgs() << end.first->getName() << ": ";
      dump_mbb_list(dbgs(), end.second) << "\n";
    }
    dbgs() << "misc: [";
    for(auto tmp: misc){
      dbgs() << tmp->getName() << ", ";
    }
    dbgs() << "]\n";
  );

  return std::make_pair(dominators, misc);
}

/// Performs a constant-loop dominator analysis, where the bounds of loops are taken into account.
/// Definition:
///   A node 'x' constant-loop dominates a node 'y' (y cl-dom x) if every path from the entry to
///   'y' visits 'x' a fixed number of non-zero times.
///
/// Returns the cl-dominators of each block (i.e. x -> y, means y cl-dom y).
///
/// The entry block of the function is assumed to not be the header of a loop.
///
/// Inputs:
/// * start_mbb: The entry block to start analysis from
/// * LI: MachineLoopInfo of the block's function
/// * constantBounds: function that given a loop header block returns whether the loop
///                   is constant (lower and upper bound are equal)
/// * end_doms_only: Whether resulting map should only contain the end blocks of the
///                  given function
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
	auto dominators = constantLoopDominatorsAnalysisImpl(start_mbb, LI, constantBounds).first;

	if(end_doms_only) {
		assert(start_mbb->pred_size() == 0 && "End dominators only should only be used on function entry");
		LLVM_DEBUG(dbgs() << "End dominators only\n");
		std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>> end_doms;
		for(auto entry: dominators){
			auto block = entry.first;
			if(block->succ_size() == 0) {
				end_doms[block] = dominators[block];
			}
		}
		dominators = end_doms;
	}
	return dominators;
}

}
#endif /* TARGET_PATMOS_SINGLEPATH_CONSTANTLOOPDOMINATORANALYSIS_H_ */
