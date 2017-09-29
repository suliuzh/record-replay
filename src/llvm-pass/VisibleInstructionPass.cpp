
#include "VisibleInstructionPass.hpp"

#include <llvm/Analysis/AliasSetTracker.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>


namespace concurrency_passes {

//--------------------------------------------------------------------------------------------------

VisibleInstructionPass::VisibleInstructionPass(char& ID)
: llvm::ModulePass(ID)
, m_nr_visible_instructions(0)
, m_nr_instrumented(0)
{
}

//--------------------------------------------------------------------------------------------------

bool VisibleInstructionPass::runOnModule(llvm::Module& module)
{    
   try
   {
      onStartOfPass(module);
      auto& functions = module.getFunctionList();
      std::for_each(functions.begin(), functions.end(),
                    [this, &module](auto& function) { runOnFunction(module, function); });
      onEndOfPass(module);

      // print statistics
      llvm::errs() << "number of visible instructions:\t" << m_nr_visible_instructions << "\n";
      llvm::errs() << "number of instrumented instructions:\t" << m_nr_instrumented << "\n";
   }
   catch (const std::exception& e)
   {
      llvm::errs() << e.what() << "\n";
   }
   return false;
}

//--------------------------------------------------------------------------------------------------

void VisibleInstructionPass::getAnalysisUsage(llvm::AnalysisUsage& analysis_usage) const {
    analysis_usage.addRequired<llvm::AAResultsWrapperPass>();
}

bool VisibleInstructionPass::runOnFunction(llvm::Module& module, llvm::Function& function)
{
   using namespace llvm;

   if (!isBlackListed(function) && !function.isDeclaration())
   {
      instrumentFunction(module, function);
      llvm::AliasSetTracker alias_set_tracker{getAnalysis<llvm::AAResultsWrapperPass>(function).getAAResults()};
      llvm_visible_instruction::creator creator{alias_set_tracker};
      
      for (auto inst_it = inst_begin(function); inst_it != inst_end(function); ++inst_it)
      {
         auto& instruction = *inst_it;
         alias_set_tracker.add(&instruction);
         if (const auto visible_instruction = creator.visit(instruction))
         {
            runOnVisibleInstruction(module, function, inst_it, *visible_instruction);
            ++m_nr_visible_instructions;
         }
      }
      
      llvm::errs() << "AliasSet for " << function.getName() << ":\n";
      for (const auto& as : alias_set_tracker.getAliasSets())
      {
          as.dump();
      }
      llvm::errs() << "\n\n";
   }

   return false;
}

//--------------------------------------------------------------------------------------------------

bool VisibleInstructionPass::isBlackListed(const llvm::Function& function) const
{
   return false;
}

//--------------------------------------------------------------------------------------------------

} // end namespace concurrency_passes
