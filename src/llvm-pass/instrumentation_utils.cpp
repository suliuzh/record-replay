
#include "instrumentation_utils.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Module.h>


namespace instrumentation_utils {

//--------------------------------------------------------------------------------------------------

llvm::Value* get_or_create_global_string_ptr(llvm::Module& module, llvm::Instruction& before,
                                             const std::string& variable_name,
                                             const std::string& str)
{
   llvm::IRBuilder<> builder(&before);
   llvm::GlobalVariable* global_variable = module.getGlobalVariable(variable_name, true);
   if (global_variable)
   {
      llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(module.getContext()), 0);
      return builder.CreateInBoundsGEP(global_variable->getValueType(), global_variable,
                                       {zero, zero}, variable_name);
   }
   return builder.CreateGlobalStringPtr(str, variable_name);
}

//--------------------------------------------------------------------------------------------------

namespace detail {
void fill_c_int64_array(llvm::Value* array, const std::vector<llvm::Value*>& data,
                        llvm::Instruction& before, llvm::ConstantInt* zero)
{
   llvm::IRBuilder<> builder(&before);
   unsigned int i = 0;
   for (auto* value : data)
   {
      auto* index = llvm::ConstantInt::get(builder.getInt32Ty(), i, false);
      auto* index_ptr = builder.CreateGEP(array, {zero, index}, "");

      if (value->getType() == builder.getInt64Ty())
      {
         auto* store = builder.CreateStore(value, index_ptr);
      }
      else
      {
         auto* cast = builder.CreateIntCast(value, builder.getInt64Ty(), true);
         auto* store = builder.CreateStore(cast, index_ptr);
      }
      ++i;
   }
}
} // end namespace detail

//--------------------------------------------------------------------------------------------------

llvm::Value* create_c_int64_array(const std::vector<llvm::Value*>& data, llvm::Instruction& before,
                                  llvm::Module& module)
{
   llvm::IRBuilder<> builder(&before);
   auto* array_type = llvm::ArrayType::get(builder.getInt64Ty(), data.size());
   auto* array = new llvm::AllocaInst(array_type, "", &before);

   llvm::ConstantInt* zero =
      llvm::ConstantInt::get(module.getContext(), llvm::APInt(64, llvm::StringRef("0"), 10));

   detail::fill_c_int64_array(array, data, before, zero);
   return builder.CreateGEP(array, {zero, zero}, "");
}

//--------------------------------------------------------------------------------------------------

llvm::CallInst* add_call_begin(llvm::Function* F, llvm::Function* callee,
                               const llvm::ArrayRef<llvm::Value*>& args,
                               const std::string& call_name)
{
   llvm::IRBuilder<> builder(&*(llvm::inst_begin(F)));
   return builder.CreateCall(callee, args, call_name);
}

//--------------------------------------------------------------------------------------------------

llvm::CallInst* add_call_end(llvm::Function* F, llvm::Function* callee,
                             const llvm::ArrayRef<llvm::Value*>& args, const std::string& call_name)
{
   llvm::IRBuilder<> builder(&*(--((--(F->end()))->end())));
   return builder.CreateCall(callee, args, call_name);
}

//--------------------------------------------------------------------------------------------------

} // end namespace instrumentation_utils
