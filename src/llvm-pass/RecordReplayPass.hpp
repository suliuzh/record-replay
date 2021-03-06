#pragma once

#include "VisibleInstructionPass.hpp"
#include "functions.hpp"
#include "llvm_visible_instruction.hpp"

#include <llvm/IR/InstIterator.h>

//--------------------------------------------------------------------------------------------------
/// @file RecordReplayPass.hpp
/// @author Susanne van den Elsen
/// @date 2015-2017
//--------------------------------------------------------------------------------------------------


namespace concurrency_passes {

/// @brief A VisibleInstructionPass that instruments an LLVM IR module of an input
/// program with a Scheduler object and wraps visible instructions in handles that
/// allow the Scheduler to record and/or replay the program under a given thread
/// interleaving.

class LightWeightPass : public VisibleInstructionPass
{
public:
   static char ID;

   /// @brief Default constructor.

   LightWeightPass();

   void onStartOfPass(llvm::Module& module) override;
   void instrumentFunction(llvm::Module& module, llvm::Function& function) override;
   void runOnVisibleInstruction(llvm::Module& module, llvm::Function& function,
                                llvm::inst_iterator inst_it,
                                const visible_instruction_t& visible_instruction) override;
   void onEndOfPass(llvm::Module& module) override;

private:
   bool isBlackListed(const llvm::Function& function) const override;

   Functions mFunctions;

}; // end class LightWeightPass

} // end namespace concurrency_passes
