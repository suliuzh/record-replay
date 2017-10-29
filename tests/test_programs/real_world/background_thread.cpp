
//--------------------------------------------------------------------------------------------------
/// @file background_thread.cpp
/// @author Susanne van den Elsen
/// @date 2017
//--------------------------------------------------------------------------------------------------

#include <pthread.h>

#include <chrono>
#include <thread>

//--------------------------------------------------------------------------------------------------

class background_thread_manager
{
public:
   background_thread_manager() { pthread_mutex_init(&m_mutex, nullptr); }

   ~background_thread_manager() { cancel_thread(); }

   void cancel_thread()
   {
      if (m_background_thread.joinable())
         m_background_thread.join();
   }

   void run_thread()
   {
      if (pthread_mutex_trylock(&m_mutex) != 0)
         return;

      cancel_thread();

      m_background_thread = std::thread([this] {
         pthread_mutex_lock(&m_mutex);
         // perform task
         pthread_mutex_unlock(&m_mutex);
      });

      pthread_mutex_unlock(&m_mutex);
   }

private:
   std::thread m_background_thread;
   pthread_mutex_t m_mutex;
};

//--------------------------------------------------------------------------------------------------


int main()
{
   background_thread_manager m;
   m.run_thread();
   std::this_thread::sleep_for(std::chrono::microseconds(40));
   m.run_thread();

   return 0;
}

//--------------------------------------------------------------------------------------------------
