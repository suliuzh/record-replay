
#include "memory_object.hpp"

#include <visible_instruction_io.hpp>

#include <debug.hpp>
#include <utils_io.hpp>

#include <algorithm>
#include <assert.h>
#include <exception>


namespace scheduler {

//--------------------------------------------------------------------------------------------------

std::vector<data_race_t> get_data_races(const memory_object& object,
   const program_model::memory_instruction& instruction)
{
   using namespace program_model;
   std::vector<data_race_t> data_races;
   auto convert_to_race = [&instruction](const auto& entry) {
      return data_race_t{entry.second, instruction};
   };
   const auto operation = instruction.operation();
   if (operation == memory_operation::Store || operation == memory_operation::ReadModifyWrite)
   {
      std::transform(object.begin(0), object.end(0), std::back_inserter(data_races),
                     convert_to_race);
   }
   std::transform(object.begin(1), object.end(1), std::back_inserter(data_races),
                  convert_to_race);
   // filter the ones with two atomic operations out
   data_races.erase(std::remove_if(data_races.begin(), data_races.end(),
                                   [](const auto& data_race) {
                                      return data_race.first.is_atomic() &&
                                             data_race.second.is_atomic();
                                   }),
                    data_races.end());
   return data_races;
}

//--------------------------------------------------------------------------------------------------


memory_object::memory_object(const object_t& object)
: m_object(object)
, m_waiting{{{}, {}}}
{
}

//--------------------------------------------------------------------------------------------------

bool memory_object::request(const instruction_t& instruction)
{
   using namespace program_model;
   DEBUGF_SYNC("memory_object", "request", instruction, "\n");

   // requesting thread has no instruction waiting on this object yet
   assert(std::all_of(m_waiting.begin(), m_waiting.end(), [&instruction](const auto& waitset) {
      return waitset.find(instruction.tid()) == waitset.end();
   }));

   // insert instruction into respective waitset
   const auto index = (static_cast<int>(instruction.operation()) % 2);
   auto& waitset = m_waiting[index];
   m_waiting[index].insert({instruction.tid(), instruction});
   DEBUG_SYNC(*this << "\n");

   return true;
}

//--------------------------------------------------------------------------------------------------

void memory_object::perform(const instruction_t& instruction)
{
   DEBUGF_SYNC("memory_object", "perform", instruction, "\n");

   // remove instruction from respective waitset
   const auto index = static_cast<size_t>(instruction.operation()) % 2;
   const auto result = m_waiting[index].erase(instruction.tid());
   assert(result == 1);
}

//--------------------------------------------------------------------------------------------------

auto memory_object::begin(std::size_t index) const -> waitset_t::const_iterator
{
   return m_waiting[index].begin();
}

//--------------------------------------------------------------------------------------------------

auto memory_object::end(std::size_t index) const -> waitset_t::const_iterator
{
   return m_waiting[index].end();
}

//--------------------------------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const memory_object& object)
{
   os << "memory_object {\n\taddress=" << object.m_object;
   std::for_each(object.m_waiting.begin(), object.m_waiting.end(), [&os](const auto& waitset) {
      os << "\n\t{";
      std::for_each(waitset.begin(), waitset.end(),
                    [&os](const auto& item) { os << item.second << ", "; });
      os << "}";
   });
   os << "\n}";
   return os;
}

//--------------------------------------------------------------------------------------------------

} // end namespace scheduler
