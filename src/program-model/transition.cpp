
#include "transition.hpp"

namespace program_model
{
   //-------------------------------------------------------------------------------------
      
   Transition::Transition(const int index,
                          StatePtr pre,
                          const Instruction& instr,
                          StatePtr post)
   : mIndex(index)
   , mPre(pre)
   , mInstr(instr)
   , mPost(post)
   {
   }
      
   //-------------------------------------------------------------------------------------
   
   int Transition::index() const
   {
      return mIndex;
   }
      
   //-------------------------------------------------------------------------------------

   const Instruction& Transition::instr() const
   {
      return mInstr;
   }
   
   //-------------------------------------------------------------------------------------
   
   auto Transition::pre_ptr() -> StatePtr
   {
      return mPre;
   }
   
   //-------------------------------------------------------------------------------------
   
   const State& Transition::pre() const
   {
      return *mPre;
   }
   
   //-------------------------------------------------------------------------------------

   State& Transition::pre()
   {
      return *mPre;
   }
   
   //-------------------------------------------------------------------------------------
   
   auto Transition::post_ptr() -> StatePtr
   {
      return mPost;
   }
   
   //-------------------------------------------------------------------------------------

   const State& Transition::post() const
   {
      return *mPost;
   }
   
   //-------------------------------------------------------------------------------------

   State& Transition::post()
   {
      return *mPost;
   }
   
   //-------------------------------------------------------------------------------------
   
   void Transition::set_pre(const StatePtr& pre)
   {
      mPre = pre;
   }
   
   //-------------------------------------------------------------------------------------
   
   void Transition::set_post(const StatePtr& post)
   {
      mPost = post;
   }
   
   //-------------------------------------------------------------------------------------
   
} // end namespace program_model
