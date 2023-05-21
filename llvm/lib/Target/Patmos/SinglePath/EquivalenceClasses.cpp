
#include "EquivalenceClasses.h"
#include "Patmos.h"
#include "SinglePath/PatmosSPReduce.h"
#include "SinglePath/ConstantLoopDominatorAnalysis.h" // for "get_intersection"
#include "SinglePath/FCFGPostDom.h"
#include "TargetInfo/PatmosTargetInfo.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/IR/Dominators.h"

#include <deque>

using namespace llvm;

#define DEBUG_TYPE "patmos-singlepath"

STATISTIC( NrEquivalenceClasses, "Number of equivalence classes in single-path code");

char EquivalenceClasses::ID = 0;

FunctionPass *llvm::createEquivalenceClassesPass() {
  return new EquivalenceClasses;
}

bool EquivalenceClasses::runOnMachineFunction(MachineFunction &MF) {
	if(!PatmosSinglePathInfo::isEnabled(MF)) {
		return false;
	}

	LLVM_DEBUG(
		dbgs() << "\n\n[Single-Path] Equivalence Classes '" << MF.getFunction().getName() << "'\n" ;
		MF.dump();
	);

	auto &LI = getAnalysis<MachineLoopInfo>();

	FCFGPostDom doms(MF, LI);
	LLVM_DEBUG(doms.print(dbgs()));

	std::map<
		// X
		MachineBasicBlock*,
		// Set of {Y->Z} control dependencies
		std::set<std::pair<Optional<MachineBasicBlock*>,MachineBasicBlock*>>
	>  deps;
	doms.get_control_dependencies(deps);

	LLVM_DEBUG(
		dbgs() << "New Control Dependencies:\n";
		for(auto entry: deps){
			dbgs() << "bb." << entry.first->getNumber() << ": ";
			for(auto edge: entry.second){
				dbgs() << "(";
				if(edge.first) {
					dbgs() << "bb." << (*edge.first)->getNumber() ;
				}
				dbgs() << " -> bb." << edge.second->getNumber()  << "), ";
			}
			dbgs() << "\n";
		}
	);

	// Find equivalence classes
	// Blocks are in the same equivalence class when they have the same control dependencies
	unsigned next_class_number = 0;
	classes.clear();

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
				dbgs() << "(";
				if(edge.first) {
					dbgs() << "bb." << (*edge.first)->getNumber() ;
				}
				dbgs() << " -> bb." << edge.second->getNumber()  << "), ";
			}
			dbgs() << "}\n\tBlocks:{";
			for(auto block: entry.second.second){
				dbgs() << "bb." << block->getNumber()  << ", ";
			}
			dbgs() << "}\n";
		}
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







