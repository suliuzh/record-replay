
#pragma once

// STL
#include <string>

//--------------------------------------------------------------------------------------90
/// @file object.hpp
/// @author Susanne van den Elsen
/// @date 2015
//----------------------------------------------------------------------------------------

namespace program_model
{
   //-------------------------------------------------------------------------------------
   
   class Object
   {
   public:
      
      //----------------------------------------------------------------------------------
      
      enum class Op { READ = 0, WRITE = 1, LOCK = 2, UNLOCK = 3 };
      
      //----------------------------------------------------------------------------------
        
      /// @brief Constructor.
      /// @note Default arguments construct dummy Object.
      
      explicit Object(std::string gvar="", const int index=0);
      
      //----------------------------------------------------------------------------------
        
      bool operator==(const Object&) const;
        
      //----------------------------------------------------------------------------------
      
      /// @brief Getter.
      
      const std::string& var() const;
      
      //----------------------------------------------------------------------------------
      
      /// @brief Getter.
      
      int index() const;
      
      //----------------------------------------------------------------------------------
        
   private:
        
      //----------------------------------------------------------------------------------
      
      std::string mVar;
      int mIndex;
      
      //----------------------------------------------------------------------------------
        
      friend std::istream& operator>>(std::istream&, Object&);
      
      //----------------------------------------------------------------------------------
        
   }; // end class Object
   
   //-------------------------------------------------------------------------------------
	
	Object llvm_object(const char* gvar, const int index=0);
   
   //-------------------------------------------------------------------------------------
	
} // end namespace program_model
