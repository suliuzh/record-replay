
#include "object_io.hpp"

#include <iostream>


namespace program_model {

//--------------------------------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const Object& obj)
{
   os << std::hex << obj.m_address << std::dec << " \"" << obj.m_name << "\"";
   return os;
}

//--------------------------------------------------------------------------------------------------

std::istream& operator>>(std::istream& is, Object& object)
{
   std::uintptr_t ptr;
   is >> std::hex >> ptr >> std::dec >> object.m_name;
   object.m_address = (void*)ptr;
   return is;
}

//--------------------------------------------------------------------------------------------------

} // end namespace program_model
