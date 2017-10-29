#pragma once

#include "concurrency_error.hpp"

#include <thread.hpp>
#include <visible_instruction.hpp>

#include <array>
#include <unordered_map>
#include <vector>

//--------------------------------------------------------------------------------------------------
/// @file memory_object.hpp
/// @author Susanne van den Elsen
/// @date 2015-2017
//--------------------------------------------------------------------------------------------------


namespace scheduler {

class memory_object
{
public:
   using thread_t = program_model::Thread;
   using object_t = program_model::Object;
   using instruction_t = program_model::memory_instruction;
   using waitset_t = std::unordered_map<thread_t::tid_t, instruction_t>;

   /// @brief Constructor.

   explicit memory_object(const object_t& object);

   bool request(const instruction_t& instr);
   void perform(const instruction_t& instruction);

   waitset_t::const_iterator begin(std::size_t index) const;
   waitset_t::const_iterator end(std::size_t index) const;

private:
   object_t m_object;
   std::array<waitset_t, 2> m_waiting;

   friend std::ostream& operator<<(std::ostream&, const memory_object&);

}; // end class object

//--------------------------------------------------------------------------------------------------

std::ostream& operator<<(std::ostream&, const memory_object&);

/// @brief Returns the set of instructions posted for the given object that form
/// a data race with the given instruction.

struct get_data_races : public boost::static_visitor<std::vector<data_race_t>>
{
   get_data_races(const memory_object& object);

   std::vector<data_race_t> operator()(const program_model::memory_instruction& instruction);

   // Default case
   template <typename T>
   std::vector<data_race_t> operator()(const T&) const
   {
      return {};
   }

   const memory_object& m_object;

}; // end struct get_data_races

//--------------------------------------------------------------------------------------------------

} // end namespace scheduler
