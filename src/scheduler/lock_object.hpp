#pragma once

#include <thread.hpp>
#include <visible_instruction.hpp>

#include <boost/optional.hpp>

#include <array>
#include <unordered_map>

//--------------------------------------------------------------------------------------------------
/// @file lock_object.hpp
/// @author Susanne van den Elsen
/// @date 2017
//--------------------------------------------------------------------------------------------------


namespace scheduler {

class lock_object
{
public:
   using thread_t = program_model::Thread;
   using object_t = program_model::Object;
   using instruction_t = program_model::lock_instruction;
   using waitset_t = std::unordered_map<thread_t::tid_t, instruction_t>;
   using state_t = std::array<waitset_t, 3>;

   explicit lock_object(const object_t& object);

   /// @returns whether the lock operation is currently enabled
   bool request(const instruction_t& instruction);

   /// @returns whether the lock operation was successfull
   bool perform(const instruction_t& instruction);

   program_model::Tids waiting() const;

private:
   object_t m_object;
   state_t m_waiting;
   boost::optional<thread_t::tid_t> m_holder;

   friend std::ostream& operator<<(std::ostream&, const lock_object&);

}; // end class lock_object

//--------------------------------------------------------------------------------------------------

std::ostream& operator<<(std::ostream&, const lock_object&);

//--------------------------------------------------------------------------------------------------

} // end namespace scheduler
