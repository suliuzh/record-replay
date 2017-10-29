
#include "lock_object.hpp"

#include <visible_instruction_io.hpp>

#include <debug.hpp>

#include <algorithm>
#include <assert.h>


namespace scheduler {

//--------------------------------------------------------------------------------------------------

lock_object::lock_object(const object_t& object)
: m_object(object)
, m_waiting{{{}, {}, {}}}
, m_holder(boost::none)
{
}

//--------------------------------------------------------------------------------------------------

bool lock_object::request(const instruction_t& instruction)
{
   using namespace program_model;
   DEBUGF_SYNC("lock_object", "request", instruction, "\n");

   // requesting thread has no instruction waiting on this object yet
   assert(std::all_of(m_waiting.begin(), m_waiting.end(), [&instruction](const auto& waitset) {
      return waitset.find(instruction.tid()) == waitset.end();
   }));
   // if thread requests an unlock, the thread is the holder of this lock
   assert(instruction.operation() != lock_operation::Unlock ||
          (m_holder != boost::none && *m_holder == instruction.tid()));

   // insert instruction into respective waitset
   const auto index = (static_cast<int>(instruction.operation()) % 3);
   auto& waitset = m_waiting[index];
   m_waiting[index].insert({instruction.tid(), instruction});
   DEBUG_SYNC(*this << "\n");

   // determine whether the instruction is currently enabled
   return instruction.operation() != lock_operation::Lock || m_holder == boost::none;
}

//--------------------------------------------------------------------------------------------------

bool lock_object::perform(const instruction_t& instruction)
{
   using namespace program_model;
   DEBUGF_SYNC("lock_object", "perform", instruction, "\n");

   // remove instruction from respective waitset
   const auto index = static_cast<size_t>(instruction.operation()) % 3;
   const auto result = m_waiting[index].erase(instruction.tid());
   assert(result == 1);

   bool success = true;

   // update m_holder and success
   if (instruction.operation() == lock_operation::Lock)
   {
      assert(!m_holder);
      m_holder = instruction.tid();
   }
   else if (instruction.operation() == lock_operation::Trylock)
   {
      if (!m_holder)
         m_holder = instruction.tid();
      else
         success = false;
   }
   else
   {
      assert(m_holder && *m_holder == instruction.tid());
      m_holder = boost::none;
   }

   DEBUG_SYNC(*this << "\n");
   return success;
}

//--------------------------------------------------------------------------------------------------

program_model::Tids lock_object::waiting() const
{
   program_model::Tids waiting;
   std::transform(m_waiting[0].begin(), m_waiting[0].end(), std::inserter(waiting, waiting.end()),
                  [](const auto& item) { return item.first; });
   std::transform(m_waiting[1].begin(), m_waiting[1].end(), std::inserter(waiting, waiting.end()),
                  [](const auto& item) { return item.first; });
   return waiting;
}

//--------------------------------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const lock_object& object)
{
   os << "lock_object {\n\taddress=" << object.m_object
      << "\n\tm_holder=" << (object.m_holder ? std::to_string(*object.m_holder) : "none");
   std::for_each(object.m_waiting.begin(), object.m_waiting.end(), [&os](const auto& waitset) {
      os << "\n\t{";
      std::for_each(waitset.begin(), waitset.end(),
                    [&os](const auto& item) { os << item.second << ", "; });
      os << "}";
   });
   return os;
}

//--------------------------------------------------------------------------------------------------

} // end namespace scheduler
