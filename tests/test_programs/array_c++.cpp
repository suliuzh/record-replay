
#include <array>
#include <thread>


void write_to_array(std::array<int,3>& array, const int id)
{
   array[id] = id;
}

int main()
{
   std::array<int,3> local_array;
   
   std::thread thread1(write_to_array, std::ref(local_array), 1);
   std::thread thread2(write_to_array, std::ref(local_array), 2);
   local_array[0] = 0;
   
   thread1.join();
   thread2.join();

   return 0;
}
