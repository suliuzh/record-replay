
#include "object.hpp"


namespace program_model {

//--------------------------------------------------------------------------------------------------

Object::Object(ptr_t address, const name_t& name)
: m_address(address)
, m_name(name)
{
}

//--------------------------------------------------------------------------------------------------

bool Object::operator==(const Object& other) const
{
   return m_address == other.m_address;
}

//--------------------------------------------------------------------------------------------------

auto Object::address() const -> ptr_t
{
   return m_address;
}

//--------------------------------------------------------------------------------------------------

} // end namespace program_model
