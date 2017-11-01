
#include "RecordReplayPass.hpp"

#include "instrumentation_utils.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

#include <boost/range/adaptor/filtered.hpp>


namespace concurrency_passes {

//--------------------------------------------------------------------------------------------------

char LightWeightPass::ID = 0;

//--------------------------------------------------------------------------------------------------

LightWeightPass::LightWeightPass()
: VisibleInstructionPass(ID)
{
}

//--------------------------------------------------------------------------------------------------

void LightWeightPass::onStartOfPass(llvm::Module& module)
{
   mFunctions.initialize(module);
}

//--------------------------------------------------------------------------------------------------

void LightWeightPass::instrumentFunction(llvm::Module& module, llvm::Function& function)
{
   if (!function.isDeclaration())
   {
      auto* function_name = instrumentation_utils::get_or_create_global_string_ptr(
         module, *inst_begin(function), "_recrep_function_name_" + function.getName().str(),
         function.getName());
      // function entry
      instrumentation_utils::add_call_begin(&function, mFunctions.Wrapper_enter_function(),
                                            {function_name});

      // function exit
      for (auto inst_it = inst_begin(&function); inst_it != inst_end(&function); ++inst_it)
      {
         if (llvm::isa<llvm::ReturnInst>(&*inst_it) || llvm::isa<llvm::ResumeInst>(&*inst_it))
         {
            llvm::IRBuilder<> builder(&*inst_it);
            builder.CreateCall(mFunctions.Wrapper_exit_function(), {function_name}, "");
         }
         else if (const auto* call = llvm::dyn_cast<llvm::CallInst>(&*inst_it))
         {
            if (call->getCalledFunction()->getName() == "pthread_exit")
            {
               llvm::IRBuilder<> builder(&*inst_it);
               builder.CreateCall(mFunctions.Wrapper_exit_function(), {function_name}, "");
            }
         }
      }
   }
}

//--------------------------------------------------------------------------------------------------

void LightWeightPass::runOnVisibleInstruction(llvm::Module& module, llvm::Function& function,
                                              llvm::inst_iterator inst_it,
                                              const visible_instruction_t& visible_instruction)
{
   auto wrapper = concurrency_passes::wrap(module, mFunctions, inst_it);
   visible_instruction.apply_visitor(wrapper);
   ++m_nr_instrumented;
}

//--------------------------------------------------------------------------------------------------

bool LightWeightPass::isBlackListed(const llvm::Function& function) const
{
   return mFunctions.blacklisted(&function);
}

//--------------------------------------------------------------------------------------------------

void LightWeightPass::onEndOfPass(llvm::Module& module)
{
   if (auto* main = module.getFunction("main"))
   {
      instrumentation_utils::add_call_begin(main, mFunctions.Wrapper_register_main_thread(), {});
   }
   
   // Instrument assertion failures
   const auto get_all_callers = [](llvm::Function& function)
   {
       std::vector<llvm::Instruction*> callers;
       for (auto* user : function.users())
       {
           if (llvm::isa<llvm::CallInst>(user) || llvm::isa<llvm::InvokeInst>(user))
           {
               callers.push_back(llvm::dyn_cast<llvm::Instruction>(user));
           }
       }
       return callers;
   };
   
   if (auto* assert_rtn = module.getFunction("__assert_rtn"))
   {
       const auto callers = get_all_callers(*assert_rtn);
       for (auto* caller : callers)
       {
           std::vector<llvm::Value*> args;
           if (auto* call = llvm::dyn_cast<llvm::CallInst>(caller))
           {
               args = { call->getArgOperand(0), call->getArgOperand(1), call->getArgOperand(2), call->getArgOperand(3) };
           }
           else if (auto* invoke = llvm::dyn_cast<llvm::InvokeInst>(caller))
           {
               args = { invoke->getArgOperand(0), invoke->getArgOperand(1), invoke->getArgOperand(2), invoke->getArgOperand(3) };
           }
           llvm::IRBuilder<> builder(caller);
           builder.CreateCall(mFunctions.Wrapper_notify_assertion_failure(), args, "");
       }
   }
}

//--------------------------------------------------------------------------------------------------

} // end namespace concurrency_passes


static llvm::RegisterPass<concurrency_passes::LightWeightPass> X(
   "instrument-record-replay-lw", "concurrency_passes::LightWeightPass", false, false);

//--------------------------------------------------------------------------------------------------
