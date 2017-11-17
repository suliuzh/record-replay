
#include "task_pool.hpp"

#include "object_io.hpp"
#include "thread_io.hpp"
#include "visible_instruction_io.hpp"

#include "debug.hpp"
#include "utils_io.hpp"
#include <algorithm/zip_map_values.hpp>

#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>

#include <algorithm>
#include <assert.h>

namespace scheduler {

//--------------------------------------------------------------------------------------------------

void TaskPool::register_thread(const Thread::tid_t& tid)
{
   std::lock_guard<std::mutex> guard(mMutex);
   mThreads.insert(Threads::value_type(tid, Thread(tid)));
   const auto thread = mThreads.find(tid);
   std::lock_guard<std::mutex> lock(m_objects_mutex);
   m_thread_states.insert(thread_states_t::value_type(tid, thread_state(thread->second)));
   DEBUGF_SYNC("TaskPool", "register_thread", thread->second, "\n");
}

//--------------------------------------------------------------------------------------------------

void TaskPool::post(const Thread::tid_t& tid, const instruction_t& task)
{
   std::lock_guard<std::mutex> lock_mutex(mMutex);
   DEBUGF_SYNC("Taskpool", "post", tid, "\n");
   auto task_it(mTasks.find(tid));
   /// @pre mTasks.find(tid) == mTasks.end()
   assert(task_it == mTasks.end());
   mTasks.insert(Tasks::value_type(tid, task));
   update_object_post(tid, task);
   // cond SIGNAL mModified
   mModified.notify_one();
}

//--------------------------------------------------------------------------------------------------

void TaskPool::post_assertion_failure(const Thread::tid_t& tid, const assertion_failure& error)
{
   std::lock_guard<std::mutex> lock_mutex(mMutex);

   DEBUGF_SYNC("Taskpool", "post_assertion_failure", tid, "\n");

   auto task_it(mTasks.find(tid));
   assert(task_it == mTasks.end());

   mAssertionFailures.push_back(error);

   set_status(tid, program_model::Thread::Status::FINISHED);

   mModified.notify_one();
}

//--------------------------------------------------------------------------------------------------

void TaskPool::yield(const Thread::tid_t& tid)
{
   std::lock_guard<std::mutex> guard(mMutex);
   if (mCurrentTask && boost::apply_visitor(program_model::get_tid(), *mCurrentTask) == tid)
   {
      DEBUGF_SYNC("Taskpool", "yield",
                  boost::apply_visitor(program_model::instruction_to_short_string(), *mCurrentTask),
                  "\n");
      update_object_yield(*mCurrentTask);
   }
}

//--------------------------------------------------------------------------------------------------

void TaskPool::finish(const program_model::Thread::tid_t& tid)
{
   std::lock_guard<std::mutex> lock(m_objects_mutex);

   auto thread_state = m_thread_states.find(tid);
   if (thread_state == m_thread_states.end())
   {
      throw std::runtime_error("TaskPool::finish");
   }

   std::for_each(thread_state->second.begin(), thread_state->second.end(),
                 [this](const auto& join_request) {
                    set_status(join_request.first, program_model::Thread::Status::ENABLED);
                 });

   set_status_protected(tid, program_model::Thread::Status::FINISHED);

   // As soon as the thread is finished, potential waiters for a join are enabled and remain
   // enabled even after a potential join. Joining an unjoinable thread returns an error code.
}

//--------------------------------------------------------------------------------------------------

void TaskPool::wait_until_unfinished_threads_have_posted()
{
   std::unique_lock<std::mutex> lock(mMutex);
   mModified.wait(lock, [this] {
      DEBUGF_SYNC("TaskPool", "wait_until_unfinished_threads_have_posted", "", "\n");
      return std::all_of(mThreads.begin(), mThreads.end(), [this](const auto& thread) {
         return thread.second.status() == Thread::Status::FINISHED ||
                mTasks.find(thread.first) != mTasks.end();
      });
   });
   DEBUGF_SYNC("TaskPool", "all_unfinished_threads_have_posted", "", "\n");
}

//--------------------------------------------------------------------------------------------------

void TaskPool::wait_all_finished()
{
   std::unique_lock<std::mutex> ul(mMutex);
   // cond WAIT mModified
   mModified.wait(ul, [this] {
      DEBUGF_SYNC("TaskPool", "wait_all_finished", "", "\n");
      return all_finished();
   });
}

//--------------------------------------------------------------------------------------------------

instruction_t TaskPool::set_current(const Thread::tid_t& tid)
{
   std::lock_guard<std::mutex> guard(mMutex);
   auto it = mTasks.find(tid);
   /// @pre mTasks.find(tid) != mTasks.end()
   assert(it != mTasks.end());
   mCurrentTask = std::shared_ptr<instruction_t>(new instruction_t(it->second));
   mTasks.erase(it); // noexcept
   return *mCurrentTask;
}

//--------------------------------------------------------------------------------------------------

bool TaskPool::has_next(const Thread::tid_t& tid) const
{
   return mTasks.find(tid) != mTasks.end();
}

//--------------------------------------------------------------------------------------------------

TaskPool::Tasks::const_iterator TaskPool::task(const Thread::tid_t& tid) const
{
   return mTasks.find(tid);
}

//--------------------------------------------------------------------------------------------------

TaskPool::Tasks::const_iterator TaskPool::tasks_cbegin() const
{
   return mTasks.cbegin();
}

//--------------------------------------------------------------------------------------------------

TaskPool::Tasks::const_iterator TaskPool::tasks_cend() const
{
   return mTasks.cend();
}

//--------------------------------------------------------------------------------------------------

std::shared_ptr<const instruction_t> TaskPool::current_task() const
{
   return mCurrentTask;
}

//--------------------------------------------------------------------------------------------------

size_t TaskPool::size() const
{
   return mTasks.size();
}

//--------------------------------------------------------------------------------------------------

Thread::Status TaskPool::status_protected(const Thread::tid_t& tid)
{
   std::lock_guard<std::mutex> guard(mMutex);
   return status(tid);
}

//--------------------------------------------------------------------------------------------------

void TaskPool::set_status_protected(const Thread::tid_t& tid, const Thread::Status& status)
{
   std::lock_guard<std::mutex> guard(mMutex);
   set_status(tid, status);
   if (status == Thread::Status::DISABLED || status == Thread::Status::FINISHED)
   {
      mModified.notify_one();
   }
}

//--------------------------------------------------------------------------------------------------

Tids TaskPool::enabled_set() const
{
   using namespace boost::adaptors;
   Tids tids;
   const auto is_enabled = [](const auto& thread) {
      return thread.second.status() == Thread::Status::ENABLED;
   };
   boost::copy(mThreads | filtered(is_enabled) | map_keys, std::inserter(tids, tids.end()));
   return tids;
}

//--------------------------------------------------------------------------------------------------

Tids TaskPool::enabled_set_protected()
{
   std::lock_guard<std::mutex> guard(mMutex);
   return enabled_set();
}

//--------------------------------------------------------------------------------------------------

NextSet TaskPool::nextset_protected()
{
   using zip_function_t = std::function<next_t(const instruction_t&, const Thread&)>;
   const zip_function_t zip_function = [](const auto& task, const auto& thread) {
      return next_t{task, thread.status() == Thread::Status::ENABLED};
   };
   std::lock_guard<std::mutex> guard(mMutex);
   /// @throws std::invalid_argument (rethrow)
   return utils::algorithm::zip_map_values(mTasks, mThreads, zip_function);
}

//--------------------------------------------------------------------------------------------------

std::unique_ptr<State> TaskPool::program_state()
{
   std::lock_guard<std::mutex> guard(mMutex);
   NextSet N{};
   Tids Enabled{};
   for (const auto& entry : mTasks)
   {
      bool enabled = (status(entry.first) == Thread::Status::ENABLED);
      N.insert(NextSet::value_type(entry.first, next_t{entry.second, enabled}));
      if (enabled)
      {
         Enabled.insert(entry.first);
      }
   }
   return std::make_unique<State>(std::move(Enabled), std::move(N));
}

//--------------------------------------------------------------------------------------------------

std::vector<data_race_t> TaskPool::data_races() const
{
   std::lock_guard<std::mutex> lock(m_objects_mutex);
   return m_data_races;
}

//--------------------------------------------------------------------------------------------------

bool TaskPool::has_assertion_failures()
{
   std::lock_guard<std::mutex> guard(mMutex);
   return !mAssertionFailures.empty();
}

//--------------------------------------------------------------------------------------------------

std::vector<assertion_failure> TaskPool::assertion_failures()
{
   std::lock_guard<std::mutex> guard(mMutex);
   return mAssertionFailures;
}

//--------------------------------------------------------------------------------------------------

Thread::Status TaskPool::status(const Thread::tid_t& tid) const
{
   auto it = mThreads.find(tid);
   /// @pre mThreads.find(tid) != mThreads.end()
   assert(it != mThreads.end());
   return it->second.status();
}

//--------------------------------------------------------------------------------------------------

void TaskPool::set_status(const Thread::tid_t& tid, const Thread::Status& status)
{
   auto thread_it = mThreads.find(tid);
   assert(thread_it != mThreads.end());
   thread_it->second.set_status(status);
}

//--------------------------------------------------------------------------------------------------

void TaskPool::update_object_post(const Thread::tid_t& tid, const instruction_t& task)
{
   bool enabled = true;

   // lock instructions
   if (const auto* lock_instr = boost::get<program_model::lock_instruction>(&task))
   {
      const auto address = lock_instr->operand().address();
      std::lock_guard<std::mutex> lock(m_objects_mutex);

      // get or create item for address
      auto it = m_lock_objects.find(address);
      if (it == m_lock_objects.end())
         it = m_lock_objects
                 .insert(lock_objects_t::value_type(address, lock_object(lock_instr->operand())))
                 .first;

      enabled = it->second.request(*lock_instr);
   }

   // memory instructions
   else if (const auto* memory_instr = boost::get<program_model::memory_instruction>(&task))
   {
      const auto address = memory_instr->operand().address();
      std::lock_guard<std::mutex> lock(m_objects_mutex);

      // get or create item for address
      auto it = m_objects.find(address);
      if (it == m_objects.end())
         it =
            m_objects.insert(objects_t::value_type(address, memory_object(memory_instr->operand())))
               .first;

      // update dataraces
      auto data_races = get_data_races(it->second, *memory_instr);
      std::move(data_races.begin(), data_races.end(), std::back_inserter(m_data_races));

      enabled = it->second.request(*memory_instr);
   }

   // thread management instructions
   else if (const auto* management_instr =
               boost::get<program_model::thread_management_instruction>(&task))
   {
      if (management_instr->operation() == program_model::thread_management_operation::Join)
      {
         std::lock_guard<std::mutex> lock(m_objects_mutex);
         auto it = m_thread_states.find(management_instr->operand().tid());
         assert(it != m_thread_states.end());
         enabled = it->second.request(*management_instr);
      }
   }

   // update thread status
   set_status(tid, (enabled ? Thread::Status::ENABLED : Thread::Status::DISABLED));
}

//--------------------------------------------------------------------------------------------------

void TaskPool::update_object_yield(const instruction_t& task)
{
   // lock instructions
   if (const auto* lock_instr = boost::get<program_model::lock_instruction>(&task))
   {
      std::lock_guard<std::mutex> lock(m_objects_mutex);
      auto it = m_lock_objects.find(lock_instr->operand().address());
      assert(it != m_lock_objects.end());

      const bool success = it->second.perform(*lock_instr);

      // update status of threads waiting on tasks's lock_object operand
      const bool did_lock =
         (lock_instr->operation() != program_model::lock_operation::Unlock && success);
      const auto status = did_lock ? Thread::Status::DISABLED : Thread::Status::ENABLED;
      const auto waiting = it->second.waiting();
      std::for_each(waiting.begin(), waiting.end(),
                    [this, status](const auto tid) { set_status(tid, status); });
   }

   // memory instruction
   else if (const auto* memory_instr = boost::get<program_model::memory_instruction>(&task))
   {
      std::lock_guard<std::mutex> lock(m_objects_mutex);
      auto it = m_objects.find(memory_instr->operand().address());
      assert(it != m_objects.end());

      it->second.perform(*memory_instr);
   }
}

//--------------------------------------------------------------------------------------------------

bool TaskPool::all_finished() const
{
   return std::all_of(mThreads.begin(), mThreads.end(), [](const auto& thread) {
      return thread.second.status() != Thread::Status::FINISHED;
   });
}

//--------------------------------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, TaskPool& task_pool)
{
   os << "TaskPool {";
   std::for_each(
      task_pool.tasks_cbegin(), task_pool.tasks_cend(), [&os, &task_pool](const auto& task) {
         const auto enabled =
            task_pool.status_protected(task.first) == program_model::Thread::Status::ENABLED;
         os << "\n\t[" << task.first << "]\t" << (enabled ? "enabled " : "disabled") << "\t->\t"
            << boost::apply_visitor(program_model::instruction_to_short_string(), task.second);
      });
   os << "\n}";
   return os;
}

//--------------------------------------------------------------------------------------------------

} // end namespace scheduler
