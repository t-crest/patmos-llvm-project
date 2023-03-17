//===---- VirtualizePredicates.h - Reduce the CFG for Single-Path code ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//
#ifndef TARGET_PATMOS_SINGLEPATH_VIRTUALIZEPREDICATES_H_
#define TARGET_PATMOS_SINGLEPATH_VIRTUALIZEPREDICATES_H_

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "PatmosSubtarget.h"
#include "PatmosSinglePathInfo.h"
#include "PatmosMachineFunctionInfo.h"

namespace llvm {

	class VirtualizePredicates : public MachineFunctionPass {
	private:
	public:
		static char ID;

		VirtualizePredicates(const PatmosTargetMachine &tm) :
			MachineFunctionPass(ID)
		{}

		StringRef getPassName() const override {
			return "Patmos Single-Path Virtualize Predicates";
		}

		void getAnalysisUsage(AnalysisUsage &AU) const override {
			MachineFunctionPass::getAnalysisUsage(AU);
		}

		bool runOnMachineFunction(MachineFunction &MF) override;
	};
}

#endif /* TARGET_PATMOS_SINGLEPATH_VIRTUALIZEPREDICATES_H_ */
