
#include "scheduler.hpp"

// PROGRAM_MODEL
#include "execution_io.hpp"
#include "instruction_io.hpp"
#include "visible_instruction_io.hpp"

// UTILS
#include "container_io.hpp"
#include "debug.hpp"
#include "error.hpp"
#include "utils_io.hpp"

// STL
#include <exception>

namespace scheduler
{
   //-------------------------------------------------------------------------------------

   class unregistered_thread : public std::exception {};

   //-------------------------------------------------------------------------------------

   Scheduler::Scheduler()
   : mLocVars(std::make_unique<LocalVars>())
   , mPool()
   , mControl()
   , mThreads()
   , mNrRegistered(0)
   , mRegMutex()
   , mRegCond()
   , mStatus(Execution::Status::RUNNING)
   , mStatusMutex()
   , mSettings(SchedulerSettings::read_from_file("schedules/settings.txt"))
   , mSelector(selector_factory(mSettings.strategy_tag()))
   , mThread([this] { return run(); })
   {
      DEBUGNL("Starting Scheduler");
      DEBUGNL("schedule:\t" << mLocVars->schedule());
   }

   //-------------------------------------------------------------------------------------

   Scheduler::~Scheduler()
   {
      DEBUGNL("~Scheduler");
      join();
   }

   //-------------------------------------------------------------------------------------

   int Scheduler::spawn_thread(pthread_t* pid,
                               const pthread_attr_t* attr,
                               void* (*start_routine)(void*),
                               void* args)
   {
      std::lock_guard<std::mutex> guard(mRegMutex);
      // Create the thread
      int ret = pthread_create(pid, attr, start_routine, args);
      // Register the thread
      const Thread::tid_t tid = mNrRegistered;
      mThreads.insert(TidMap::value_type(*pid, tid));
      mPool.register_thread(tid);
      mControl.register_thread(tid);
      DEBUGFNL(thread_str(tid), "spawn_thread", pid, "");
      ++mNrRegistered;
      // cond SIGNAL mRegCond
      mRegCond.notify_all();
      return ret;
   }

   //-------------------------------------------------------------------------------------

   void Scheduler::wait_registered()
   {
      std::unique_lock<std::mutex> ul(mRegMutex);
      // cond WAIT mRegCond
      mRegCond.wait(ul, [this] ()
      {
         DEBUGFNL(thread_str(pthread_self()), "wait_registered", "", "");
         return mThreads.find(pthread_self()) != mThreads.end();
      });
   }

   //-------------------------------------------------------------------------------------

   void Scheduler::post_task(const int op, const Object& obj, bool is_atomic,
                             const std::string& file_name, unsigned int line_number)
   {
      if (runs_controlled())
      {
         try
         {
            const Thread::tid_t tid = find_tid(pthread_self());
            Instruction instr(tid, static_cast<Object::Op>(op), obj, is_atomic);
            instr.add_meta_data({ file_name, line_number });
            DEBUGF(thread_str(tid), "post_task", instr, "\n");
            mPool.post(tid, instr);
            DEBUGFNL(thread_str(tid), "wait_for_turn", "", "");
            mControl.wait_for_turn(tid);
         }
         catch(const unregistered_thread&)
         {
            DEBUGFNL("[unregistered_thread]", "post_task", "", "");
         }
      }
   }

   //-------------------------------------------------------------------------------------

   void Scheduler::yield()
   {
      if (runs_controlled())
      {
         try
         {
            const Thread::tid_t tid = find_tid(pthread_self());
            DEBUGFNL(thread_str(tid), "yield", "", "");
            mPool.yield(tid);
         }
         catch(const unregistered_thread&)
         {
            DEBUGFNL("[unregistered_thread]", "yield", "", "");
         }
      }
   }

   //-------------------------------------------------------------------------------------

   void Scheduler::finish()
   {
      if (runs_controlled())
      {
         const Thread::tid_t tid = find_tid(pthread_self());
         DEBUGFNL(thread_str(tid), "finish", "", "");
         mPool.set_status_protected(tid, Thread::Status::FINISHED);
      }
   }

   //-------------------------------------------------------------------------------------

   void Scheduler::join()
   {
      if (mThread.joinable())
      {
         mThread.join();
         DEBUGNL("mTread.joined");
      }
      else
      {
         DEBUGNL("!mThread.joinable()");
      }
   }

   //-------------------------------------------------------------------------------------
   // SCHEDULER INTERNAL
   //-------------------------------------------------------------------------------------

   Thread::tid_t Scheduler::find_tid(const pthread_t& pid)
   {
      std::lock_guard<std::mutex> guard(mRegMutex);
      auto it = mThreads.find(pid);
      if (it != mThreads.end())
      {
         return it->second;
      }
      throw unregistered_thread();
   }

   //-------------------------------------------------------------------------------------

   bool Scheduler::runs_controlled()
   {
      const Execution::Status s = status();
      return
         s != Execution::Status::BLOCKED &&
         s != Execution::Status::ERROR;
   }

   //-------------------------------------------------------------------------------------

   Execution::Status Scheduler::status()
   {
      std::lock_guard<std::mutex> guard(mStatusMutex);
      return mStatus;
   }

   //-------------------------------------------------------------------------------------

   void Scheduler::set_status(const Execution::Status &s)
   {
      std::lock_guard<std::mutex> guard(mStatusMutex);
      mStatus = s;
   }

   //-------------------------------------------------------------------------------------

   std::string Scheduler::thread_str(const Thread::tid_t& tid)
   {
      std::stringstream ss("");
      ss << "[thread" << tid << "]\t\t";
      return ss.str();
   }

   //-------------------------------------------------------------------------------------

   std::string Scheduler::thread_str(const pthread_t& pid)
   {
      std::stringstream ss("");
      ss << "[thread" << pid << "]\t\t";
      return ss.str();
   }

   //-------------------------------------------------------------------------------------

   // THREAD

   // #todo Look at mutex protection in updating E (i.e. retreiving current_task),
   // and in selecting the next thread. At this point, no other thread should operate
   // on mPool, but this is not checked/enforced.
   void Scheduler::run()
   {
      mControl.set_owner(mThread.get_id()); // mThread.get_id() == std::this_thread::get_id()
      wait_all_registered();
      mPool.wait_enabled_collected();

      Execution E(mLocVars->nr_threads(), mPool.program_state());

      while (status() == Execution::Status::RUNNING)
      {
         DEBUGNL("---------- [round" << mLocVars->task_nr() << "]");
         if (mLocVars->task_nr() > 0)
         {
            E.push_back(*mPool.current_task(), mPool.program_state());
         }
         auto selection = mSelector->select(mPool,
                                            mLocVars->schedule(),
                                            mLocVars->task_nr());
         if (selection.first == Execution::Status::RUNNING)
         {
            /// @pre
            assert(selection.second >= 0);
            if (!schedule_thread(selection.second))
            {
               report_error("selection error");
               break;
            }
         }
         else
         {
            set_status(selection.first);
            break;
         }
         mPool.wait_enabled_collected();
      }
      close(E);
   }

   //-------------------------------------------------------------------------------------

   // THREAD (PRIVATE)

   void Scheduler::wait_all_registered()
   {
      std::unique_lock<std::mutex> ul(mRegMutex);
      // cond WAIT mRegCond
      mRegCond.wait(ul, [this] () {
            DEBUGFNL("Scheduler", "wait_all_registered", "", "");
            return mNrRegistered == mLocVars->nr_threads(); });
   }

   //-------------------------------------------------------------------------------------

   /// Sets tid as the current thread in TaskPool, removing and obtaining the current
   /// posted task by Thread tid from the TaskPool. Removing the task is to guarantee that
   /// the Scheduler thread blocks on mPool.all_enabled_collected, because the thread only
   /// updates its status when it yields in task_done. Calls Control to grant Thread tid
   /// execution right.

   bool Scheduler::schedule_thread(const Thread::tid_t& tid)
   {
      if (mPool.status_protected(tid) == Thread::Status::ENABLED)
      {
         const Instruction task = mPool.set_current(tid);
         DEBUGFNL("Scheduler", "schedule_thread", tid, "next task = " << task);
         mControl.grant_execution_right(tid);
         mLocVars->increase_task_nr();
         return true;
      }
      return false;
   }

   //-------------------------------------------------------------------------------------

   /// @note As soon as internal Scheduler status is set to ERROR new threads are not
   /// waiting anymore, but we already waiting threads have to be "waken-up".

   void Scheduler::report_error(const std::string& what)
   {
      ERROR("Scheduler::report_error", what);
      set_status(Execution::Status::ERROR);
   }

   //-------------------------------------------------------------------------------------

   void Scheduler::close(Execution& E)
   {
      DEBUGFNL("Scheduler", "close", "", to_string(status()));
      if (!runs_controlled())
      {
         mControl.grant_execution_right_all();
      }
      // finish execution
      try
      {
         E.last().set_post(mPool.program_state());
      }
      catch(const std::out_of_range& e)
      {
         ERROR("Scheduler::close", e.what());
      }
      E.set_status(status());
      dump_execution(E);
      dump_data_races();
   }

   //-------------------------------------------------------------------------------------

   void Scheduler::dump_execution(const Execution& E) const
   {
      if (!E.empty())
      {
         std::ofstream record;
         record.open("record.txt");
         record << E;
         record.close();

         std::ofstream record_short;
         record_short.open("record_short.txt");
         record_short << to_short_string(E);
         record_short.close();
      }
   }
   
   //-------------------------------------------------------------------------------------
   
   void Scheduler::dump_data_races() const
   {
      std::ofstream ofs;
      ofs.open("data_races.txt", std::ofstream::app);
      for (const auto& data_race : mPool.data_races())
      {
         write_to_stream(ofs, data_race);
      }
      ofs << "\n>>>>>\n\n";
      ofs.close();
   }

   //-------------------------------------------------------------------------------------

   // Class Scheduler::LocalVars

   Scheduler::LocalVars::LocalVars()
   : mNrThreads(0)
   , mSchedule()
   , mTaskNr(0)
   {
      if (!utils::io::read_from_file("schedules/schedule.txt", mSchedule))
      {
         ERROR("Scheduler::LocalVars()", "reading schedules/schedule.txt");
         mSchedule = {};
      }
      if (!utils::io::read_from_file("schedules/threads.txt", mNrThreads))
      {
         ERROR("Scheduler::LocalVars()", "reading schedules/threads.txt");
         // #todo Handle such an error
      }
   }

   //-------------------------------------------------------------------------------------

   int Scheduler::LocalVars::nr_threads() const
   {
      return mNrThreads;
   }

   //-------------------------------------------------------------------------------------

   const schedule_t& Scheduler::LocalVars::schedule() const
   {
      return mSchedule;
   }

   //-------------------------------------------------------------------------------------

   int Scheduler::LocalVars::task_nr() const
   {
      return mTaskNr;
   }

   //-------------------------------------------------------------------------------------

   void Scheduler::LocalVars::increase_task_nr()
   {
      mTaskNr++;
   }

   //-------------------------------------------------------------------------------------

} // end namespace scheduler

//----------------------------------------------------------------------------------------

int wrapper_spawn_thread(pthread_t* pid,
                         const pthread_attr_t* attr,
                         void* (*start_routine)(void*),
                         void* args)
{
   return the_scheduler.spawn_thread(pid, nullptr, start_routine, args);
}

//----------------------------------------------------------------------------------------

void wrapper_wait_registered()
{
   the_scheduler.wait_registered();
}

//----------------------------------------------------------------------------------------

void wrapper_post_instruction(int operation, void* operand,
                              const char* file_name, unsigned int line_number)
{
   the_scheduler.post_task(operation, program_model::Object(operand), false,
                           file_name, line_number);
}

//----------------------------------------------------------------------------------------

void wrapper_post_memory_instruction(int operation, void* operand, bool is_atomic,
                                     const char* file_name, unsigned int line_number)
{
   the_scheduler.post_task(operation, program_model::Object(operand), is_atomic,
                           file_name, line_number);
}

//----------------------------------------------------------------------------------------

void wrapper_yield()
{
   the_scheduler.yield();
}

//----------------------------------------------------------------------------------------

void wrapper_finish()
{
   the_scheduler.finish();
}

//----------------------------------------------------------------------------------------
