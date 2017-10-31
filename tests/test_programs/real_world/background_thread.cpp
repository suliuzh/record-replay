
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

   ~background_thread_manager() { join_thread(); }

   void run_thread()
   {
      if (pthread_mutex_trylock(&m_mutex) != 0)
         // ... notify client that background thread is already running ...
         return;

      join_thread();

      m_background_thread = std::thread([this] {
         pthread_mutex_lock(&m_mutex);
         // ... perform task ...
         pthread_mutex_unlock(&m_mutex);
      });

      pthread_mutex_unlock(&m_mutex);
   }

private:
   void join_thread()
   {
      if (m_background_thread.joinable())
         m_background_thread.join();
   }

   std::thread m_background_thread;
   pthread_mutex_t m_mutex;
};

//--------------------------------------------------------------------------------------------------


int main()
{
   background_thread_manager m;

   m.run_thread();
   
   std::thread thr(&background_thread_manager::run_thread, &m);
   thr.join();

   return 0;
}

//--------------------------------------------------------------------------------------------------
