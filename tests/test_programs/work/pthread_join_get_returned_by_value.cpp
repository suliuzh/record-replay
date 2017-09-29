
#include <pthread.h>

#include <chrono>
#include <iostream>
#include <thread>


int global_variable1 = 1;
int global_variable2 = 2;
void* global_pointer = &global_variable1;

void* return_pointer(void*)
{
   std::this_thread::sleep_for(std::chrono::nanoseconds(1));
   pthread_exit(global_pointer);
}

void* modify_global_pointer(void*)
{
   std::this_thread::sleep_for(std::chrono::nanoseconds(1));
   global_pointer = &global_variable2;
   pthread_exit(nullptr);
}

int main()
{
   pthread_t thread1;
   pthread_t thread2;
   int value;

   pthread_create(&thread1, nullptr, &return_pointer, nullptr);
   pthread_create(&thread2, nullptr, &modify_global_pointer, nullptr);

   pthread_join(thread1, (void**)&value);
   value = 3;
   pthread_join(thread2, nullptr);

   return 0;
}
