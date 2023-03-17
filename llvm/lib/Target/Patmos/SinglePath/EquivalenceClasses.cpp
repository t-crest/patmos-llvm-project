
#include "EquivalenceClasses.h"
#include "Patmos.h"
#include "SinglePath/PatmosSPReduce.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/IR/Dominators.h"

using namespace llvm;

#define DEBUG_TYPE "patmos-singlepath"

STATISTIC( NrEquivalenceClasses, "Number of equivalence classes in single-path code");

char EquivalenceClasses::ID = 0;

FunctionPass *llvm::createEquivalenceClassesPass() {
  return new EquivalenceClasses;
}

bool EquivalenceClasses::runOnMachineFunction(MachineFunction &MF) {

	LLVM_DEBUG(
		dbgs() << "\n\n[Single-Path] Equivalence Classes '" << MF.getFunction().getName() << "'\n" ;
		MF.dump();
	);

	PostDomTreeBase<MachineBasicBlock> PDT;
	PDT.recalculate(MF);
	LLVM_DEBUG(PDT.print(dbgs()));

	// Calculate control dependencies
	// X is control dependent on Y if X post-dominates Z but not Y (given the edge Y->Z).
	std::map<
		// X
		MachineBasicBlock*,
		// Set of {Y->Z} control dependencies
		std::set<Optional<std::pair<MachineBasicBlock*,MachineBasicBlock*>>>
	>  deps;

	std::for_each(MF.begin(), MF.end(), [&](auto &mbb){
		SmallVector<MachineBasicBlock*, 10> post_dominees;
		PDT.getDescendants(&mbb, post_dominees);

		// If mbb post-dominates a block, any of its predecessors that mbb does not dominate must therefore
		// be a control dependency of mbb
		for(auto dominee: post_dominees){
			if(dominee->pred_size() == 0) {
				// If you postdominate the entry, then you are control dependent on the entry
				deps[&mbb].insert(None);
			} else {
				std::for_each(dominee->pred_begin(), dominee->pred_end(), [&](auto pred){
					// We must use strict dominance, otherwise loops will get wrong dependencies
					// when exits blocks have a dependency on an edge outgoing from themselves
					if(pred == &mbb || !PDT.dominates(&mbb, pred)){
						deps[&mbb].insert(std::make_pair(pred, dominee));
					}
				});
			}
		}
	});

	LLVM_DEBUG(
		dbgs() << "Control Dependencies:\n";
		for(auto entry: deps){
			dbgs() << "bb." << entry.first->getNumber() << ": ";
			for(auto edge: entry.second){
				if(edge) {
					dbgs() << "(bb." << edge->first->getNumber()  << " -> bb." << edge->second->getNumber()  << "), ";
				} else {
					dbgs() << "(->bb.0), ";
				}
			}
			dbgs() << "\n";
		}
	);

	// Find equivalence classes
	// Blocks are in the same equivalence class when they have the same control dependencies
	unsigned next_class_number = 0;
	classes.clear(); // C++ is stupid, so this apparently is needed

	std::for_each(MF.begin(), MF.end(), [&](auto &mbb){
		// Check if class already exists
		auto found = std::find_if(classes.begin(), classes.end(), [&](auto entry){
			return deps[&mbb] == entry.second.first;
		});

		if(found != classes.end()) {
			found->second.second.insert(&mbb);
		} else {
			classes[next_class_number++] = std::make_pair(deps[&mbb], std::set<MachineBasicBlock*>({&mbb}));
		}
	});
	NrEquivalenceClasses += classes.size();

	LLVM_DEBUG(
		dbgs() << "Equivalence Classes:\n";
		for(auto entry: classes){
			dbgs() << "(" << entry.first << "):\n\tControl Dependencies: {";
			for(auto edge: entry.second.first){
				if(edge) {
					dbgs() << "(bb." << edge->first->getNumber()  << " -> bb." << edge->second->getNumber()  << "),";
				} else {
					dbgs() << "(->bb.0), ";
				}
			}
			dbgs() << "}\n\tBlocks:{";
			for(auto block: entry.second.second){
				dbgs() << "bb." << block->getNumber()  << ", ";
			}
			dbgs() << "}\n";
		}
	);

	// We test a hypothesis: Every class has a single "root" block which is post-dominated by all the others
	assert(
		std::all_of(classes.begin(), classes.end(), [&](auto entry){
			Optional<MachineBasicBlock*> root;
			for(auto block: entry.second.second){
				bool dominated_by_all = std::all_of(entry.second.second.begin(), entry.second.second.end(), [&](auto other){
					return PDT.dominates(other, block);
				});
				if(root && dominated_by_all) {
					return false; // Both root and block dominated by all others
				} else if(!root && dominated_by_all) {
					root = block;
				}
			}
			return root.hasValue();
		})
		&&
		"Did not find root in all classes"
	);

	return false;
}

std::vector<EqClass> EquivalenceClasses::getAllClasses() const {
	std::vector<EqClass> result;

	for(auto entry: classes){
		result.push_back( EqClass{
			entry.first, entry.second.first, entry.second.second
		});
	}

	return result;
}


EqClass EquivalenceClasses::getClassFor(MachineBasicBlock *mbb) const{
	auto found = std::find_if(classes.begin(), classes.end(), [&](auto entry){
		return entry.second.second.count(mbb);
	});

	assert(found != classes.end() && "No class for block");

	return EqClass{
		found->first, found->second.first, found->second.second
	};
}







