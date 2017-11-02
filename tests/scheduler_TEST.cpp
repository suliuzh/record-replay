
#include "include/test_helpers.hpp"

#include <replay.hpp>

#include <execution_io.hpp>

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <chrono>

//--------------------------------------------------------------------------------------------------

namespace record_replay {
namespace test {

//--------------------------------------------------------------------------------------------------
// DeadlockSanityCheck
//--------------------------------------------------------------------------------------------------

struct SchedulerDeadlockSanitityCheck : public ::testing::TestWithParam<InstrumentedProgramTestData>
{
   void SetUp() override
   {
      boost::filesystem::create_directories(
         detail::test_data_dir / GetParam().test_program.filename() /
         boost::filesystem::path("0" + GetParam().optimization_level) / "records");
   }

   boost::filesystem::path test_output_dir() const
   {
      return detail::test_data_dir / GetParam().test_program.filename() /
             boost::filesystem::path("0" + GetParam().optimization_level);
   }
}; // end struct SchedulerDeadlockSanitityCheck

TEST_P(SchedulerDeadlockSanitityCheck, SchedulerDoesNotEndInDeadlockOnMultipleRuns)
{
   const auto instrumented_executable = scheduler::instrument(
      detail::test_programs_dir / GetParam().test_program, test_output_dir() / "instrumented",
      GetParam().optimization_level, GetParam().compiler_options);

   for (int i = 0; i < 500; ++i)
      ASSERT_NO_THROW(scheduler::run_under_schedule(instrumented_executable, {},
                                                    std::chrono::milliseconds(3000),
                                                    test_output_dir() / "records"));
}

INSTANTIATE_TEST_CASE_P(
   RealWorldPrograms, SchedulerDeadlockSanitityCheck,
   ::testing::Values(                                                                       //
      InstrumentedProgramTestData{"real_world/dining_philosophers.cpp", "3", "-std=c++14"}, //
      InstrumentedProgramTestData{"real_world/work_stealing_queue.cpp", "3", "-std=c++14"}  //
      ));

//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// AssertionFailureTest
//--------------------------------------------------------------------------------------------------

struct SchedulerAssertionFailureTest
: public ::testing::TestWithParam<InstrumentedProgramWithScheduleTestData>
{
   void SetUp() override
   {
      boost::filesystem::create_directories(
         detail::test_data_dir / GetParam().instrumentation_data.test_program.filename() /
         boost::filesystem::path("0" + GetParam().instrumentation_data.optimization_level) /
         "records");
   }

   boost::filesystem::path test_output_dir() const
   {
      return detail::test_data_dir / GetParam().instrumentation_data.test_program.filename() /
             boost::filesystem::path("0" + GetParam().instrumentation_data.optimization_level);
   }
}; // end struct SchedulerAssertionFailureTest

TEST_P(SchedulerAssertionFailureTest, AssertionsAreHandledCorrectly)
{
   const auto instrumented_executable = scheduler::instrument(
      detail::test_programs_dir / GetParam().instrumentation_data.test_program,
      test_output_dir() / "instrumented", GetParam().instrumentation_data.optimization_level,
      GetParam().instrumentation_data.compiler_options);

   ASSERT_NO_THROW(scheduler::run_under_schedule(instrumented_executable, GetParam().schedule,
                                                 std::chrono::milliseconds(3000),
                                                 test_output_dir() / "records"));
   
   const auto record = test_output_dir() / "records" / "record.txt";
   ASSERT_TRUE(boost::filesystem::exists(record));
   program_model::Execution execution;
   std::ifstream input_file(record.string());
   input_file >> execution;
   
   ASSERT_EQ(execution.status(), program_model::Execution::Status::ASSERTION_FAILURE);
}

INSTANTIATE_TEST_CASE_P(RealWorldPrograms, SchedulerAssertionFailureTest,
                        ::testing::Values( //
                           InstrumentedProgramWithScheduleTestData{
                              {"real_world/bank_account.cpp", "3", "-std=c++14"},
                              {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2}} //,
                           ));

} // end namespace test
} // end namespace record_replay
