//===---- EquivalenceClasses.h - Reduce the CFG for Single-Path code ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#ifndef TARGET_PATMOS_SINGLEPATH_EQUIVALENCECLASSES_H_
#define TARGET_PATMOS_SINGLEPATH_EQUIVALENCECLASSES_H_

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"

#include <set>

namespace llvm {

	// Represents an equivalence class
	//
	// These aren't unique and therefore pointers to EqClass shouldn't be used to
	// distinguish between classes.
	struct EqClass {
		/// The unique number assigned to this class
		unsigned number;

		/// The edges the block is control dependent on
		std::set<std::pair<Optional<MachineBasicBlock*>,MachineBasicBlock*>> dependencies;

		// The blocks within the class
		std::set<MachineBasicBlock*> members;
	};

	class EquivalenceClasses : public MachineFunctionPass {
	private:
		std::map<
			// Unique number of the class
			unsigned,
			std::pair<
				// The control dependencies of the class.
				// If the source is 'None' it is a dependency on the entry to the loop.
				// (the target is then the header)
				std::set<std::pair<Optional<MachineBasicBlock*>,MachineBasicBlock*>>,
				// The blocks in the class
				std::set<MachineBasicBlock*>
			>
		> classes;

	public:
		static char ID;

		EquivalenceClasses() :
			MachineFunctionPass(ID)
		{}

		StringRef getPassName() const override {
			return "Patmos Single-Path Equivalence Classes";
		}

		void getAnalysisUsage(AnalysisUsage &AU) const override {
			AU.addRequired<MachineLoopInfo>();
			AU.addPreserved<MachineLoopInfo>();
			MachineFunctionPass::getAnalysisUsage(AU);
		}

		bool runOnMachineFunction(MachineFunction &MF) override;

		std::vector<EqClass> getAllClasses() const;

		EqClass getClassFor(MachineBasicBlock*mbb) const;

		// Returns for each equivalence class, which classes are the source
		// of its dependence edges
		std::map<unsigned, std::set<unsigned>> getClassParents() const;

		// Returns for the given equivalence class, which classes are the source
		// of its dependence edges, recursively
		static std::set<unsigned> getAllParents(unsigned class_nr, std::map<unsigned, std::set<unsigned>> &parent_tree);

		// Exports the equivalence class "parent" relations given as metadata connected to the given function
		static void exportClassTreeToModule(std::map<unsigned, std::set<unsigned>> &parent_tree, MachineFunction &MF);

		// Imports the metadata representing the equivalence class relation tree connected to the given function
		static std::map<unsigned, std::set<unsigned>> importClassTreeFromModule(const MachineFunction &MF);

		// Adds the given class number, as a metadata operand, to the given instruction,
		// signifying what equivalence class the instruction is predicated by)
		static void addClassMD(MachineInstr* MI, unsigned class_nr);

		// Extracts the metadata operand signifying what equivalence class the instruction is predicated by
		static Optional<unsigned> getEqClassNr(const MachineInstr* MI);
	};
}

#endif /* TARGET_PATMOS_SINGLEPATH_EQUIVALENCECLASSES_H_ */
