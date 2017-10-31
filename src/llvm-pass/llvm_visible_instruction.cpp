
#include "llvm_visible_instruction.hpp"

#include "functions.hpp"
#include "instrumentation_utils.hpp"

#include "visible_instruction_io.hpp"

#include "color_output.hpp"

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>

#include <assert.h>


namespace concurrency_passes {

//--------------------------------------------------------------------------------------------------

wrap::wrap(llvm::Module& module, Functions& functions, llvm::inst_iterator& instruction_it)
: m_module(module)
, m_functions(functions)
, m_instruction_it(instruction_it)
{
}

//--------------------------------------------------------------------------------------------------

bool is_blacklisted(const program_model::meta_data_t& meta_data)
{
   const static std::array<std::string, 5> std_lib = {{ 
      "include/c++/v1/memory", 
      "include/c++/v1/thread",
      "include/c++/v1/tuple",
      "include/c++/v1/type_traits",
      "include/c++/v1/__functional_base"
   }};
   bool blacklisted = std::any_of(std_lib.begin(), std_lib.end(), [&meta_data](const auto& file_name) { return meta_data.file_name.find(file_name) != std::string::npos; });
   return blacklisted;
}

void wrap::operator()(const memory_instruction& instruction)
{
   if (is_blacklisted(instruction.meta_data()))
      return;
   auto arguments = construct_arguments(instruction);
   llvm::CallInst::Create(m_functions.Wrapper_post_memory_instruction(), arguments, "",
                          &*m_instruction_it);
}

//--------------------------------------------------------------------------------------------------

void wrap::operator()(const lock_instruction& instruction)
{
   if (is_blacklisted(instruction.meta_data()))
      return;
   auto arguments = construct_arguments(instruction);
   llvm::CallInst::Create(m_functions.Wrapper_post_lock_instruction(), arguments, "",
                          &*m_instruction_it);
}

//--------------------------------------------------------------------------------------------------

void wrap::operator()(const thread_management_instruction& instruction)
{
   if (is_blacklisted(instruction.meta_data()))
      return;
   assert(llvm::isa<llvm::CallInst>(&*m_instruction_it) ||
          llvm::isa<llvm::InvokeInst>(&*m_instruction_it));

   switch (instruction.operation())
   {
      case program_model::thread_management_operation::Spawn:
      {
         const auto arguments = construct_arguments(instruction);
         auto* tid = llvm::CallInst::Create(m_functions.Wrapper_post_spawn_instruction(), arguments,
                                            "", &*m_instruction_it);
         ++m_instruction_it;
         llvm::CallInst::Create(m_functions.Wrapper_register_thread(),
                                arguments_t{instruction.operand(), tid}, "", &*m_instruction_it);
         --m_instruction_it;
         break;
      }
      case program_model::thread_management_operation::Join:
      {
         const auto arguments = construct_arguments(instruction);
         llvm::Function* wrapper;
         if (instruction.operand()->getType() == m_functions.Type_pthread_t())
         {
            wrapper = m_functions.Wrapper_post_pthread_join_instruction();
         }
         else if (instruction.operand()->getType() == m_functions.Type_stdthread()->getPointerTo())
         {
            wrapper = m_functions.Wrapper_post_stdthread_join_instruction();
         }
         else
         {
            throw std::invalid_argument("join instruction has invalid operand type");
         }
         llvm::CallInst::Create(wrapper, arguments, "", &*m_instruction_it);
         break;
      }
      default:
         break;
   }
}

//--------------------------------------------------------------------------------------------------

auto wrap::construct_arguments(const memory_instruction& instruction) -> arguments_t
{
   using namespace llvm;
   Value* arg_operation = ConstantInt::get(
      m_module.getContext(), APInt(32, static_cast<int>(instruction.operation()), false));
   Value* arg_operand = construct_operand(instruction.operand());
   Value* arg_operand_name = construct_operand_name(instruction.operand());
   Value* arg_is_atomic =
      ConstantInt::get(m_module.getContext(), APInt(8, (instruction.is_atomic() ? 1 : 0), false));
   Value* arg_file_name = construct_file_name(instruction.meta_data().file_name);
   Value* arg_line_number = construct_line_number(instruction.meta_data().line_number);
   return {arg_operation, arg_operand,   arg_operand_name,
           arg_is_atomic, arg_file_name, arg_line_number};
}

//--------------------------------------------------------------------------------------------------

auto wrap::construct_arguments(const lock_instruction& instruction) -> arguments_t
{
   using namespace llvm;
   Value* arg_operation = ConstantInt::get(
      m_module.getContext(), APInt(32, static_cast<int>(instruction.operation()), false));
   Value* arg_operand = construct_operand(instruction.operand());
   Value* arg_operand_name = construct_operand_name(instruction.operand());
   Value* arg_file_name = construct_file_name(instruction.meta_data().file_name);
   Value* arg_line_number = construct_line_number(instruction.meta_data().line_number);
   return {arg_operation, arg_operand, arg_operand_name, arg_file_name, arg_line_number};
}

//--------------------------------------------------------------------------------------------------

auto wrap::construct_arguments(const thread_management_instruction& instruction) -> arguments_t
{
   using namespace llvm;
   Value* arg_file_name = construct_file_name(instruction.meta_data().file_name);
   Value* arg_line_number = construct_line_number(instruction.meta_data().line_number);
   return {instruction.operand(), arg_file_name, arg_line_number};
}

//--------------------------------------------------------------------------------------------------

llvm::Value* wrap::construct_operand(const memory_location_t& operand)
{
   llvm::IRBuilder<> builder(&*m_instruction_it);
   return builder.CreatePointerCast(operand, builder.getInt8PtrTy());
}

//--------------------------------------------------------------------------------------------------

llvm::Value* wrap::construct_operand_name(llvm::Value* name,
                                          const std::vector<llvm::Value*>& indices_data,
                                          llvm::Instruction& before)
{
   llvm::IRBuilder<> builder(&before);
   auto* indices = instrumentation_utils::create_c_int64_array(indices_data, before, m_module);
   auto* size = llvm::ConstantInt::get(builder.getInt64Ty(), indices_data.size(), false);
   return builder.CreateCall(m_functions.Helper_create_operand_name(), {name, indices, size}, "");
}

//--------------------------------------------------------------------------------------------------

llvm::Value* wrap::construct_operand_name(const memory_location_t& operand)
{
   llvm::IRBuilder<> builder(&*m_instruction_it);

   if (const auto* gep = llvm::dyn_cast<llvm::GetElementPtrInst>(operand))
   {
      llvm::Value* name;
      if (const auto* gep2 = llvm::dyn_cast<llvm::ConstantExpr>(gep->getPointerOperand()))
      {
         name = instrumentation_utils::get_or_create_global_string_ptr(
            m_module, *m_instruction_it, "_mem_loc_name_" + gep2->getOperand(0)->getName().str(),
            gep2->getOperand(0)->getName().str());
      }
      else
      {
         name = instrumentation_utils::get_or_create_global_string_ptr(
            m_module, *m_instruction_it,
            "_mem_loc_name_" + gep->getPointerOperand()->getName().str(),
            gep->getPointerOperand()->getName().str());
      }

      std::vector<llvm::Value*> indices;
      for (auto index = gep->idx_begin(); index != gep->idx_end(); ++index)
      {
         indices.push_back(index->get());
      }
      return construct_operand_name(name, indices, *m_instruction_it);
   }

   else if (auto* const_expr = llvm::dyn_cast<llvm::ConstantExpr>(operand))
   {
      if (const_expr->isGEPWithNoNotionalOverIndexing())
      {
         auto name = instrumentation_utils::get_or_create_global_string_ptr(
            m_module, *m_instruction_it,
            "_mem_loc_name_" + const_expr->getOperand(0)->getName().str(),
            const_expr->getOperand(0)->getName().str());
         std::vector<llvm::Value*> indices{const_expr->getOperand(1), const_expr->getOperand(2)};
         return construct_operand_name(name, indices, *m_instruction_it);
      }
   }
   std::string global_name = "_mem_loc_name_" + operand->getName().str();
   return instrumentation_utils::get_or_create_global_string_ptr(
      m_module, *m_instruction_it, global_name, operand->getName().str());
}

//--------------------------------------------------------------------------------------------------

llvm::Value* wrap::construct_file_name(const std::string& file_name)
{
   std::string global_name = "_recrep_file_name_" + file_name;
   return instrumentation_utils::get_or_create_global_string_ptr(m_module, *m_instruction_it,
                                                                 global_name, file_name);
}

//--------------------------------------------------------------------------------------------------

llvm::Value* wrap::construct_line_number(unsigned int line_number)
{
   return llvm::ConstantInt::get(m_module.getContext(), llvm::APInt(32, line_number, false));
}

//--------------------------------------------------------------------------------------------------


namespace llvm_visible_instruction {

//--------------------------------------------------------------------------------------------------

bool is_thread_local(const llvm::Value& operand)
{
   const llvm::Value* address_start = operand.stripInBoundsOffsets();
   if (llvm::isa<llvm::GlobalVariable>(address_start))
   {
      return false;
   }
   return false;
}

//--------------------------------------------------------------------------------------------------

boost::optional<program_model::meta_data_t> get_meta_data(llvm::Instruction& instruction)
{
   if (llvm::DILocation* location = instruction.getDebugLoc())
   {
      return boost::make_optional<program_model::meta_data_t>(
         {location->getFilename().str(), location->getLine()});
   }
   return boost::optional<program_model::meta_data_t>();
}

//--------------------------------------------------------------------------------------------------

template <typename instruction_t, typename... args_t>
creator::return_type create(llvm::Instruction& instruction,
                            const typename instruction_t::operation_t& operation,
                            llvm::Value* operand, args_t&&... args)
{
   if (!is_thread_local(*operand))
   {
      instruction_t visible_instruction(nullptr, operation, operand, std::forward<args_t>(args)...);
      auto meta_data = get_meta_data(instruction);
      if (meta_data)
      {
         visible_instruction.add_meta_data(*meta_data);
      }
      return creator::return_type(visible_instruction);
   }
   return creator::return_type();
}

//--------------------------------------------------------------------------------------------------
// creator
//--------------------------------------------------------------------------------------------------

using memory_operation = program_model::memory_operation;
using lock_operation = program_model::lock_operation;

//--------------------------------------------------------------------------------------------------

auto creator::visitLoadInst(llvm::LoadInst& instr) -> return_type
{
   return create<memory_instruction>(instr, memory_operation::Load, instr.getPointerOperand(),
                                     instr.isAtomic());
}

//--------------------------------------------------------------------------------------------------

auto creator::visitStoreInst(llvm::StoreInst& instr) -> return_type
{
   return create<memory_instruction>(instr, memory_operation::Store, instr.getPointerOperand(),
                                     instr.isAtomic());
}

//--------------------------------------------------------------------------------------------------

auto creator::visitAtomicRMWInst(llvm::AtomicRMWInst& instr) -> return_type
{
   assert(instr.isAtomic());
   return create<memory_instruction>(instr, memory_operation::ReadModifyWrite,
                                     instr.getPointerOperand(), true);
}

//--------------------------------------------------------------------------------------------------

auto creator::visitCallInst(llvm::CallInst& instr) -> return_type
{
   return handle_call_and_invoke_instr(instr, instr.getCalledFunction(), instr.arg_operands());
}

//--------------------------------------------------------------------------------------------------

auto creator::visitInvokeInst(llvm::InvokeInst& instr) -> return_type
{
   return handle_call_and_invoke_instr(instr, instr.getCalledFunction(), instr.arg_operands());
}

//--------------------------------------------------------------------------------------------------

auto creator::visitInstruction(llvm::Instruction& instr) -> return_type
{
   return return_type();
}

//--------------------------------------------------------------------------------------------------

auto creator::handle_call_and_invoke_instr(
   llvm::Instruction& instr, const llvm::Function* callee,
   const llvm::iterator_range<llvm::User::const_op_iterator>& arg_operands) -> return_type
{
   // Direct function invocation
   if (callee)
   {
      using namespace program_model;
      if (callee->getName() == "pthread_create")
      {
         return create<thread_management_instruction>(instr, thread_management_operation::Spawn,
                                                      *arg_operands.begin());
      }
      else if (callee->getName() == "\01_pthread_join" ||
               callee->getName() == "_ZNSt3__16thread4joinEv")
      {
         return create<thread_management_instruction>(instr, thread_management_operation::Join,
                                                      *arg_operands.begin());
      }
      else if (callee->getName() == "pthread_mutex_lock")
      {
         return create<lock_instruction>(instr, lock_operation::Lock, *arg_operands.begin());
      }
      else if (callee->getName() == "pthread_mutex_unlock")
      {
         return create<lock_instruction>(instr, lock_operation::Unlock, *arg_operands.begin());
      }
      else if (callee->getName() == "pthread_mutex_trylock")
      {
         return create<lock_instruction>(instr, lock_operation::Trylock, *arg_operands.begin());
      }
   }
   /// @todo Case of indirect function invokation
   /// @todo Include pthread_exit as visible instruction?

   return return_type();
}

//--------------------------------------------------------------------------------------------------

} // end namespace llvm_visible_instruction
} // end namespace concurrency_passes
