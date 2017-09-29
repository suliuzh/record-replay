
#include <pthread.h>


void* return_locally_allocated_by_pointer(void*)
{
    int* new_int = new int;
    *new_int = 1;
    pthread_exit((void*)new_int);
}


int main()
{
    pthread_t thread_id;
    int* allocated_int;
    
    pthread_create(&thread_id, nullptr, &return_locally_allocated_by_pointer, nullptr);
    pthread_join(thread_id, (void**)&allocated_int);
    
    *allocated_int = 2;
    delete allocated_int;
    
    return 0;
}
