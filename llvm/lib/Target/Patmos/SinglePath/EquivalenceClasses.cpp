
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
		auto predecessors = getAllClassPredecessors();
		dbgs() << "Equivalence Class predecessors:\n";
		for(auto entry: predecessors) {
			dbgs() << entry.first << ": ";
			for(auto parent: entry.second) {
				dbgs() << parent << ", ";
			}
			dbgs() << "\n";
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

std::map<unsigned, std::set<unsigned>> EquivalenceClasses::getAllClassPredecessors() const {
	std::map<unsigned, std::set<unsigned>> predecessors;

	for(auto entry: classes){
		auto class_nr = entry.first;
		auto &class_deps = entry.second.first;

		for(auto dep: class_deps) {
			auto dep_class_nr = getClassFor(dep.first? *dep.first : dep.second).number;
			if(dep_class_nr != class_nr) {
				predecessors[class_nr].insert(dep_class_nr);
			}
		}
	}

	return predecessors;
}

std::set<unsigned> EquivalenceClasses::getAllPredecessors(
		unsigned class_nr,
		std::map<unsigned, std::set<unsigned>> &parent_tree
) {
	std::set<unsigned> result;

	getAllPredecessors(class_nr, parent_tree, result);

	return result;
}

void EquivalenceClasses::getAllPredecessors(
		unsigned class_nr,
		std::map<unsigned, std::set<unsigned>> &parent_tree,
		std::set<unsigned> &result
) {
	if(parent_tree.count(class_nr)) {
		for(auto parent: parent_tree[class_nr]){
			/// Loops in the program may add loops to the predecessors
			if(!result.count(parent)){
				getAllPredecessors(parent, parent_tree, result);
			}
		}
	}
}

static MDNode* unsigned_md(unsigned x, LLVMContext &C) {
	return MDNode::get(C, ConstantAsMetadata::get(ConstantInt::get(C, llvm::APInt(64, x, false))));
}

void EquivalenceClasses::exportClassPredecessorsToModule(MachineFunction &MF) {
	auto &F= MF.getFunction();
	LLVMContext &C = F.getContext();
	std::vector<Metadata*> mds;
	std::map<unsigned, MDNode*> result;

	for(auto entry: getAllClassPredecessors()) {
		std::vector<Metadata*> parents_md_list;
		for(auto parent: entry.second) {
			parents_md_list.push_back(unsigned_md(parent, C));
		}
		MDNode* parents_md = MDTuple::get(C, ArrayRef<Metadata*>(parents_md_list));
		std::vector<Metadata*> entry_md_list;
		auto class_md = unsigned_md(entry.first, C);
		result[entry.first] = class_md;
		entry_md_list.push_back(class_md);
		entry_md_list.push_back(parents_md);
		mds.push_back(MDTuple::get(C, entry_md_list));
	}
	F.setMetadata("llvm.patmos.equivalence.class.tree", MDTuple::get(C, mds));
}

std::map<unsigned, std::set<unsigned>> EquivalenceClasses::importClassPredecessorsFromModule(const MachineFunction &MF) {
	std::map<unsigned, std::set<unsigned>> result;

	auto &F= MF.getFunction();
	LLVMContext &C = F.getContext();
	assert(F.getMetadata("llvm.patmos.equivalence.class.tree") && "Equivalence class tree not exported to metadata");

	auto class_tree_md = dyn_cast<MDTuple>(F.getMetadata("llvm.patmos.equivalence.class.tree"));

	for(auto i = 0; i<class_tree_md->getNumOperands(); i++) {
		auto class_tree_md_entry = dyn_cast<MDTuple>(class_tree_md->getOperand(i));
		assert(class_tree_md_entry->getNumOperands() == 2);

		auto get_val = [&](auto &md){
			return cast<ConstantInt>(dyn_cast<ConstantAsMetadata>(dyn_cast<MDNode>(md)->getOperand(0))->getValue())->getSExtValue();
		};

		auto class_nr = get_val(class_tree_md_entry->getOperand(0));
		auto parents_md = dyn_cast<MDTuple>(class_tree_md_entry->getOperand(1));
		std::set<unsigned> parents;
		for(auto j = 0; j<parents_md->getNumOperands(); j++) {
			parents.insert(get_val(parents_md->getOperand(j)));
		}
		result[class_nr] = parents;
	}

	return result;
}

void EquivalenceClasses::addClassMetaData(MachineInstr* MI, unsigned class_nr) {
	auto MF = MI->getParent()->getParent();
	auto &C = MF->getFunction().getContext();

	MI->addOperand(*MF, MachineOperand::CreateMetadata(MDNode::get(C, MDString::get(C, "patmos.eq.class"))));
	MI->addOperand(*MF, MachineOperand::CreateMetadata(unsigned_md(class_nr, C)));
}

Optional<unsigned> EquivalenceClasses::getEqClassNr(const MachineInstr* MI) {
	Optional<unsigned> idx;
	for(int i = 0; i<MI->getNumOperands(); i++){
		if(MI->getOperand(i).isMetadata()){
			auto &md = MI->getOperand(i).getMetadata()->getOperand(0);
			if(auto string = dyn_cast<MDString>(md)) {
				if(string->getString().equals("patmos.eq.class")) {
					idx = i+1;
					break;
				}
			}
		}
	}
	if(idx) {
		assert(MI->getOperand(*idx).isMetadata());
		return cast<ConstantInt>(dyn_cast<ConstantAsMetadata>(dyn_cast<MDNode>(MI->getOperand(*idx).getMetadata())->getOperand(0))->getValue())->getSExtValue();
	} else {
		return None;
	}
}

bool EquivalenceClasses::dependentClasses(
		const MachineInstr* instr1,const MachineInstr* instr2,
		std::map<unsigned, std::set<unsigned>> &parent_tree
){
	auto class1 = EquivalenceClasses::getEqClassNr(instr1);
	auto class2 = EquivalenceClasses::getEqClassNr(instr2);
	if(class1 && class2) {
	  auto preds1 = EquivalenceClasses::getAllPredecessors(*class1, parent_tree);
	  auto preds2 = EquivalenceClasses::getAllPredecessors(*class2, parent_tree);

	  return !(*class1 != *class2 && !preds1.count(*class2) && !preds2.count(*class1));
	} else {
	  // Unpredicated instructions are always enabled and so may be enabled with all others
	  return true;
	}
}

