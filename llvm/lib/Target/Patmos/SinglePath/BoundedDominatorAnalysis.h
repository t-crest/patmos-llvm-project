#ifndef TARGET_PATMOS_SINGLEPATH_BOUNDEDDOMINATORANALYSIS_H_
#define TARGET_PATMOS_SINGLEPATH_BOUNDEDDOMINATORANALYSIS_H_

#include "llvm/CodeGen/MachineBasicBlock.h"
#include <set>
#include <map>
#include <queue>

#define DEBUG_TYPE "patmos-singlepath"

// Uncomment to always print debug info
//#define LLVM_DEBUG(...) __VA_ARGS__

namespace llvm {

/// Performs a modified dominator analysis, where the bounds of loops are taken into account.
/// A loop dominates its successors only if it has constant bounds, i.e., its minum and maximum
/// iteration bounds are the same.
///
/// Returns the dominators of the end blocks of the loop identified by the given header
/// (or the entry block of a function)
///
///
template<
  typename MachineBasicBlock,
  typename MachineLoopInfo
>
std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>>
boundedDominatorsAnalysis(
    const MachineBasicBlock *start_mbb,
    const MachineLoopInfo *LI,
    bool (*constantBounds)(const MachineBasicBlock *mbb)
) {
  LLVM_DEBUG(dbgs() << "\nStarting Bounded Dominator Analysis: " << start_mbb->getName() << "\n");
  std::set<const MachineBasicBlock*> all_mbbs;
  std::set<const MachineBasicBlock*> end_mbbs;
  std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>> dominators;
  std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>> loop_dominators;

  // Start by exploring the graph
  std::queue<const MachineBasicBlock *> queue;

  queue.push(start_mbb);
  while(!queue.empty()) {
      auto *current = queue.front();
      queue.pop();
      if(all_mbbs.count(current)) {
        LLVM_DEBUG(dbgs() << "\nAlready explored: " << current->getName() <<"\n");
        continue;
      }
      all_mbbs.insert(current);

      LLVM_DEBUG(dbgs() << "\nExplore next: " << current->getName() <<"\n");

      if(current->succ_size() == 0){
        // The end block
        end_mbbs.insert(current);
        LLVM_DEBUG(dbgs() << "End block.\n");
      } else if(current != start_mbb && LI->isLoopHeader(current)) {
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
              queue.push(exit.second);
              LLVM_DEBUG(dbgs() << "Found exit to current loop: " << exit.second->getName() <<"\n");
          } else {
            LLVM_DEBUG(dbgs() << "Exit to outer loop: " << exit.second->getName() <<"\n");
            LLVM_DEBUG(dbgs() << "Exit to outer loop: " << exit.first->getName() <<"\n");

            // This exit exits the current loop too, so treat is as an end
            assert(LI->getLoopFor(start_mbb) && "Didn't find loop");
            assert(LI->getLoopFor(start_mbb)->isLoopExiting(exit.first) && "Doesn't exit the current loop");
            end_mbbs.insert(exit.second);
          }
        }

        // Now run the analysis in the header's loop
        auto loop_end_dominators = boundedDominatorsAnalysis(current, LI, constantBounds);
        assert(std::all_of(loop_end_dominators.begin(), loop_end_dominators.end(), [&](auto end_doms){
          return end_doms.second.count(current);
        }) && "Header doesn't dominate all ends");

        if(loop_end_dominators.size() == 0) {
          // loop not singlepath. return nothing
          return std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>>();
        }

        // Intersect all the loop's end dominators to find all the blocks
        // that dominate any end of the loop
        auto shared_doms = loop_end_dominators.begin()->second;
        std::for_each(std::next(loop_end_dominators.begin()), loop_end_dominators.end(), [&](auto end_doms){
          std::set<const MachineBasicBlock*> new_shared_doms;
          std::set_intersection(
            shared_doms.begin(), shared_doms.end(),
            end_doms.second.begin(), end_doms.second.end(),
            std::inserter(new_shared_doms, new_shared_doms.begin())
          );
          shared_doms = new_shared_doms;
        });
        assert(!loop_dominators.count(current) && "Loop already analyzed");
        loop_dominators[current] = shared_doms;
      } else {
        std::for_each(current->succ_begin(), current->succ_end(), [&](auto succ){
          if( LI->getLoopDepth(current) > 0 &&
              //Exit edge
              !(LI->getLoopFor(current)->contains(succ))
          ) {
            LLVM_DEBUG(dbgs() << "End Block\n");
            end_mbbs.insert(current);
          } else if(
              LI->getLoopDepth(current) == 0 ||
              // Not latch
              LI->getLoopFor(current)->getHeader() != succ
          ) {
              LLVM_DEBUG(dbgs() << "Enqueue: " << succ->getName() <<"\n");
              queue.push(succ);
          }
        });
      }
  }

  std::set<const MachineBasicBlock*> all_possible(all_mbbs);
  for(auto loop: loop_dominators) {
    all_possible.insert(loop.second.begin(), loop.second.end());
  }
  for(auto mbb: all_mbbs) {
    dominators[mbb] = all_possible;
  }
  dominators[start_mbb] = {start_mbb};

  bool changed = true;
  while(changed) {
    changed = false;
    for(auto mbb: all_mbbs) {
      LLVM_DEBUG(dbgs() << "\nPruning dominators for: " << mbb->getName() <<"\n");
      std::set<const MachineBasicBlock*> shared_doms(dominators[mbb]);

      std::for_each(mbb->pred_begin(), mbb->pred_end(), [&](auto pred){
        // Ignore latches
        if(!(LI->isLoopHeader(mbb) && LI->getLoopFor(mbb)->contains(pred))){
          std::set<const MachineBasicBlock*> pred_doms;
          if( // Exit from loop
              LI->getLoopDepth(pred)>LI->getLoopDepth(mbb) ||
              // Exit from loop directly to the header of next loop
              (LI->getLoopDepth(pred) == LI->getLoopDepth(mbb) &&
              LI->getLoopFor(pred) != LI->getLoopFor(mbb) &&
              mbb != start_mbb
              )
          ){
            auto *loop = LI->getLoopFor(pred);
            while(
                loop->getParentLoop() != LI->getLoopFor(mbb) &&
                loop->getLoopDepth() != LI->getLoopDepth(mbb)
            ){
              auto *new_loop = loop->getParentLoop();
              assert(new_loop && "Reached the root");
              loop= new_loop;
            }
            pred = loop->getHeader();
            assert(loop_dominators.count(pred) && "Analysis not run for loop");
            if(constantBounds(pred)) {
              LLVM_DEBUG(dbgs() << "Adding dominators of loop '" << pred->getName() <<"'\n");
              pred_doms.insert(loop_dominators[pred].begin(), loop_dominators[pred].end());
              shared_doms.insert(loop_dominators[pred].begin(), loop_dominators[pred].end());
            } else {
              LLVM_DEBUG(dbgs() << "Not adding dominators of loop '" << pred->getName() <<"'\n");
            }
          }
          pred_doms.insert(dominators[pred].begin(), dominators[pred].end());

          std::set<const MachineBasicBlock*> new_shared_doms;
          std::set_intersection(
            shared_doms.begin(), shared_doms.end(),
            pred_doms.begin(), pred_doms.end(),
            std::inserter(new_shared_doms, new_shared_doms.begin())
          );
          if(( // Exit from loop
               LI->getLoopDepth(pred)>LI->getLoopDepth(mbb) ||
               // Exit from loop directly to the header of next loop
               (LI->getLoopDepth(pred) == LI->getLoopDepth(mbb) &&
               LI->getLoopFor(pred) != LI->getLoopFor(mbb) //&&
               //mbb != start_mbb
               )
             ) &&
             !constantBounds(pred)
          ) {
            assert(LI->isLoopHeader(pred) && "Not header");
            LLVM_DEBUG(dbgs() << "Removing loop header from dominators '" << pred->getName() <<"'\n");
            new_shared_doms.erase(pred);
          }
          shared_doms = new_shared_doms;
        }
      });

      shared_doms.insert(mbb);

      changed |= shared_doms != dominators[mbb];

      LLVM_DEBUG(
        dbgs() << "Old dominators: ";
        for(auto dom: dominators[mbb]) {
          dbgs() << dom->getName() << ", ";
        }
        dbgs() << "\n";
      );
      LLVM_DEBUG(
        dbgs() << "New dominators: ";
        for(auto dom: shared_doms) {
          dbgs() << dom->getName() << ", ";
        }
        dbgs() << "\n";
      );

      dominators[mbb] = shared_doms;
    }
  }

  std::set<const MachineBasicBlock*> latch_doms;
  if(LI->getLoopDepth(start_mbb) > 0) {
    LLVM_DEBUG(dbgs() << "\nAdding latch dominators\n");
    auto loop = LI->getLoopFor(start_mbb);

    SmallVector<MachineBasicBlock*> loop_latches;
    loop->getLoopLatches(loop_latches);

    auto first_latch = true;
    for(auto latch: loop_latches){
      std::set<const MachineBasicBlock*> new_doms;
      if(LI->getLoopDepth(latch)>LI->getLoopDepth(start_mbb)){
        // Exit from loop
        auto *header = LI->getLoopFor(latch)->getHeader();
        while(LI->getLoopDepth(header) != (LI->getLoopDepth(start_mbb) + 1)){
          auto *new_header = LI->getLoopFor(header)->getParentLoop()->getHeader();
          assert(LI->getLoopDepth(header) == (LI->getLoopDepth(new_header)-1)
              && "Didn't find the header of outer loop");
          header= new_header;
        }
        latch = header;
        assert(loop_dominators.count(latch) && "Analysis not run for loop");
        if(constantBounds(latch)) {
          LLVM_DEBUG(dbgs() << "Adding dominators of loop '" << latch->getName() <<"'\n");
          new_doms.insert(loop_dominators[latch].begin(), loop_dominators[latch].end());
          latch_doms.insert(loop_dominators[latch].begin(), loop_dominators[latch].end());
        } else {
          LLVM_DEBUG(dbgs() << "Not adding dominators of loop '" << latch->getName() <<"'\n");
        }
      }
      if(first_latch){
        first_latch = false;
        new_doms.insert(dominators[latch].begin(),dominators[latch].end());
      } else {
        std::set_intersection(
          latch_doms.begin(), latch_doms.end(),
          dominators[latch].begin(), dominators[latch].end(),
          std::inserter(new_doms, new_doms.begin())
        );
      }
      if(
          LI->getLoopDepth(latch)>LI->getLoopDepth(start_mbb) &&
          !constantBounds(latch)
      ) {
        assert(LI->isLoopHeader(latch) && "Not header");
        LLVM_DEBUG(dbgs() << "Removing loop header from dominators '" << latch->getName() <<"'\n");
        new_doms.erase(latch);
      }
      latch_doms = new_doms;
    }
    LLVM_DEBUG(
      dbgs() << "Latch dominators: ";
      for(auto dom: latch_doms) {
        dbgs() << dom->getName() << ", ";
      }
      dbgs() << "\n";
    );
  }

  std::map<const MachineBasicBlock*, std::set<const MachineBasicBlock*>> end_doms;
  for(auto end: end_mbbs){
    end_doms[end] = dominators[end];
    end_doms[end].insert(latch_doms.begin(), latch_doms.end());
  }

  LLVM_DEBUG(
    dbgs() << "\nBounded Dominator Analysis Results: " << start_mbb->getName() << "\n";
    for(auto end: end_doms){
      dbgs() << end.first->getName() << ": ";
      for(auto dom: end.second) {
        dbgs() << dom->getName() << ", ";
      }
      dbgs() << "\n";
    }
  );

  return end_doms;
}

}
#endif /* TARGET_PATMOS_SINGLEPATH_BOUNDEDDOMINATORANALYSIS_H_ */
