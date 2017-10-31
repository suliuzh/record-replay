
#include <replay.hpp>

#include <gtest/gtest.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/preprocessor/stringize.hpp>

#include <chrono>
#include <exception>
#include <fstream>
#include <unordered_map>

//--------------------------------------------------------------------------------------------------

namespace record_replay {
namespace test {
namespace detail {

static const auto test_programs_dir =
   boost::filesystem::path(BOOST_PP_STRINGIZE(TEST_PROGRAMS_DIR));
static const auto tests_build_dir = boost::filesystem::path{BOOST_PP_STRINGIZE(TESTS_BUILD_DIR)};
static const auto test_data_dir = tests_build_dir / "test_data";

} // end namespace detail

//--------------------------------------------------------------------------------------------------

struct InstrumentedProgramRunTestData
{
   boost::filesystem::path test_program;
   std::string optimization_level;
   std::string compiler_options;

}; // end struct InstrumentedProgramRunTestData

struct InstrumentedProgramRunTest : public ::testing::TestWithParam<InstrumentedProgramRunTestData>
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
}; // end struct InstrumentedProgramRunTest

TEST_P(InstrumentedProgramRunTest, InstrumentedProgramRunsThrough)
{
   const auto instrumented_executable = scheduler::instrument(
      detail::test_programs_dir / GetParam().test_program, test_output_dir() / "instrumented",
      GetParam().optimization_level, GetParam().compiler_options);

   ASSERT_NO_THROW(scheduler::run_under_schedule(
      instrumented_executable, {}, std::chrono::milliseconds(3000), test_output_dir() / "records"));
}

INSTANTIATE_TEST_CASE_P(
   RealWorldPrograms, InstrumentedProgramRunTest,
   ::testing::Values(
      // InstrumentedProgramRunTestData{"real_world/filesystem.c", "0", ""},
      // InstrumentedProgramRunTestData{"real_world/dining_philosophers.c", "0", ""},
      InstrumentedProgramRunTestData{"real_world/background_thread.cpp", "3", "-std=c++14"}//,
      // InstrumentedProgramRunTestData{"real_world/dining_philosophers.cpp", "0", "-std=c++14"},
      // InstrumentedProgramRunTestData{"real_world/work_stealing_queue.cpp", "0", "-std=c++14"} //
      ));

//--------------------------------------------------------------------------------------------------

struct OperandNamesTestData
{
   boost::filesystem::path test_program;
   std::string optimization_level;
   std::string compiler_options;
   std::unordered_map<std::string, unsigned int> expected_operand_names;

}; // end struct InstrumentedProgramRunTestData

struct OperandNamesTest : public ::testing::TestWithParam<OperandNamesTestData>
{
   void SetUp() override
   {
      const auto records_dir = test_output_dir() / "records";
      if (!boost::filesystem::exists(records_dir))
         boost::filesystem::create_directories(records_dir);
   }

   boost::filesystem::path test_output_dir() const
   {
      return detail::test_data_dir / GetParam().test_program.filename() /
             boost::filesystem::path("0" + GetParam().optimization_level);
   }
}; // end struct InstrumentedProgramRunTest

TEST_P(OperandNamesTest, TestOperandNames)
{
   scheduler::instrument(detail::test_programs_dir / GetParam().test_program,
                         test_output_dir() / "instrumented", GetParam().optimization_level,
                         GetParam().compiler_options);

   scheduler::run_under_schedule(
      test_output_dir() / "instrumented" / GetParam().test_program.stem(), {},
      std::chrono::milliseconds(3000), test_output_dir() / "records");

   std::unordered_map<std::string, unsigned int> operand_names;

   std::ifstream ifs{(test_output_dir() / "records/record_short.txt").string()};
   std::string line;
   while (std::getline(ifs, line))
   {
      std::vector<std::string> components;
      boost::split(components, line, boost::is_any_of(" "), boost::token_compress_on);
      operand_names[components[4]]++;
   }

   for (const auto& operand_name : GetParam().expected_operand_names)
   {
      ASSERT_TRUE(operand_names.find(operand_name.first) != operand_names.end());
      ASSERT_EQ(operand_names[operand_name.first], operand_name.second);
   }
}

// INSTANTIATE_TEST_CASE_P(
//    DummyPrograms, OperandNamesTest,
//    ::testing::Values(
//       OperandNamesTestData{"global_variable.cpp", "0", "-std=c++14", {{"\"global_variable\"",
//       2}}}, OperandNamesTestData{"global_struct_member.cpp", "0", "-std=c++14",
//       {{"\"global_struct[0][0]\"", 2}}}, OperandNamesTestData{"shared_variable_reference.cpp",
//       "0", "-std=c++14", {{"\"shared_variable\"", 1}}},
//       // TODO: Doesn't work with -03!
//       OperandNamesTestData{"function_static_variable_initialization.cpp", "0", "-std=c++14",
//       {{"\"_ZZ26initialize_static_variableiE15static_variable\"", 1}}},
//       // TODO: Doesn't work with -03!
//       OperandNamesTestData{"function_static_variable.cpp", "0", "-std=c++14",
//       {{"\"_ZZ22modify_static_variableiE15static_variable\"", 2}}},
//       OperandNamesTestData{"pthread_join_get_pointer_to_global_variable.cpp", "0", "-std=c++14",
//       {{"\"global_variable\"", 1}, {"\"pointer\"", 1}}}
//    ));
// 
// INSTANTIATE_TEST_CASE_P(RealWorldPrograms, OperandNamesTest,
//                         ::testing::Values(
//                            // NOTE: when locki[0] is locked the operand name = ""
//                            // TODO: works with locki[][]
//                            OperandNamesTestData{"real_world/filesystem.c",
//                                                 "0",
//                                                 "",
//                                                 {// locki
//                                                  {"\"locki[0]\"", 1},
//                                                  {"\"locki[1]\"", 2},
//                                                  {"\"locki[2]\"", 2},
//                                                  {"\"locki[3]\"", 2},
//                                                  {"\"locki[4]\"", 2},
//                                                  {"\"locki[5]\"", 2},
//                                                  {"\"locki[6]\"", 2},
//                                                  {"\"locki[7]\"", 2},
//                                                  {"\"locki[8]\"", 2},
//                                                  {"\"locki[9]\"", 2},
//                                                  {"\"locki[10]\"", 2},
//                                                  {"\"locki[11]\"", 2},
//                                                  {"\"locki[12]\"", 2},
//                                                  {"\"locki[13]\"", 2},
//                                                  // inode
//                                                  {"\"inode[0][0]\"", 2},
//                                                  {"\"inode[0][1]\"", 2},
//                                                  {"\"inode[0][2]\"", 2},
//                                                  {"\"inode[0][3]\"", 2},
//                                                  {"\"inode[0][4]\"", 2},
//                                                  {"\"inode[0][5]\"", 2},
//                                                  {"\"inode[0][6]\"", 2},
//                                                  {"\"inode[0][7]\"", 2},
//                                                  {"\"inode[0][8]\"", 2},
//                                                  {"\"inode[0][9]\"", 2},
//                                                  {"\"inode[0][10]\"", 2},
//                                                  {"\"inode[0][11]\"", 2},
//                                                  {"\"inode[0][12]\"", 2},
//                                                  {"\"inode[0][13]\"", 2},
//                                                  // lockb
//                                                  {"\"lockb[0]\"", 4},
//                                                  {"\"lockb[1]\"", 2},
//                                                  {"\"lockb[2]\"", 2},
//                                                  {"\"lockb[4]\"", 2},
//                                                  {"\"lockb[6]\"", 2},
//                                                  {"\"lockb[8]\"", 2},
//                                                  {"\"lockb[10]\"", 2},
//                                                  {"\"lockb[12]\"", 2},
//                                                  {"\"lockb[14]\"", 2},
//                                                  {"\"lockb[16]\"", 2},
//                                                  {"\"lockb[18]\"", 2},
//                                                  {"\"lockb[20]\"", 2},
//                                                  {"\"lockb[22]\"", 2},
//                                                  {"\"lockb[24]\"", 2},
//                                                  // busy
//                                                  {"\"busy[0][0]\"", 3},
//                                                  {"\"busy[0][1]\"", 2},
//                                                  {"\"busy[0][2]\"", 2},
//                                                  {"\"busy[0][4]\"", 2},
//                                                  {"\"busy[0][6]\"", 2},
//                                                  {"\"busy[0][8]\"", 2},
//                                                  {"\"busy[0][10]\"", 2},
//                                                  {"\"busy[0][12]\"", 2},
//                                                  {"\"busy[0][14]\"", 2},
//                                                  {"\"busy[0][16]\"", 2},
//                                                  {"\"busy[0][18]\"", 2},
//                                                  {"\"busy[0][20]\"", 2},
//                                                  {"\"busy[0][22]\"", 2},
//                                                  {"\"busy[0][24]\"", 2}}},
//                            // NOTE: one time lock[0] is locked the operand name = ""
//                            // TODO: works in -O3 with lock[][]
//                            OperandNamesTestData{"real_world/dining_philosophers.c",
//                                                 "0",
//                                                 "",
//                                                 {// fork
//                                                  {"\"fork[0][0]\"", 7},
//                                                  {"\"fork[0][1]\"", 7},
//                                                  // lock
//                                                  {"\"lock[0]\"", 7},
//                                                  {"\"lock[1]\"", 8},
//                                                  // meals
//                                                  {"\"meals[0][0]\"", 3},
//                                                  {"\"meals[0][1]\"", 3}}},
// 
//                            OperandNamesTestData{"real_world/dining_philosophers.cpp",
//                                                 "3",
//                                                 "-std=c++14",
//                                                 {// fork
//                                                  {"\"forks[0][0][0][0]\"", 3},
//                                                  {"\"forks[0][0][1][0]\"", 4},
//                                                  // nr_meals
//                                                  {"\"nr_meals[0][0][0]\"", 3},
//                                                  {"\"nr_meals[0][0][1]\"", 3}}}
// 
//                            ));

//--------------------------------------------------------------------------------------------------

} // end namespace test
} // end namespace record_replay
