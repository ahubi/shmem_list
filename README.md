## Producer Consumer via shared memory
A list using shared memory implementing producer consumer pattern.  
[Wikipedia.](https://en.wikipedia.org/wiki/Producer%E2%80%93consumer_problem)
Producer will write an element into the list and consumer will read the element from the list.
Semaphores are used to synchronize producer and consumer. Mutex is not used to protect the buffer access, therefore only one producer and one consumer are allowed per one shared memory channel.

### Producer
```C
#include <stdint.h>
#include <stdio.h>
#include "../shmem_list.h"
#define shmem "TestSharedMem"

int main() {
  int i = 0;
  struct timespec ts;
  shmem_list_type_t l_type = LIST_TYPE_SOURCE;
  shmem_list_t *src_list = shmem_list_mem_open(shmem, l_type 2048, 2, 0);
  ts.tv_sec = 1;
  char *str = (char*)malloc(32);
  while(i < 10){
    void *ptr = shmem_list_acquire_write_packet(src_list, ts);
    if (ptr) {
        sprintf(str,"Producer count %d", i++);
        memcpy(ptr, str, strlen(str));
        shmem_list_write(src_list);
    }
    sleep(1);
    i++
  }
  shmem_list_close(src_list);
  return 0;
}
```
### Consumer
```C
#include <stdint.h>
#include <stdio.h>
#include "../shmem_list.h"
#define shmem "TestSharedMem"

int main() {
  int i = 0;
  struct timespec ts;
  shmem_list_type_t l_type = LIST_TYPE_SINK;
  shmem_list_t *sink_list = shmem_list_mem_open(shmem, l_type, 2048, 2, 0);
  ts.tv_sec = 1;
  char *str = (char*)malloc(32);
  while(i < 10){
    void *ptr = shmem_list_acquire_read_packet(sink_list, ts);
    if (ptr) {
      memcpy(str, ptr, 32);
      printf("packet OK %s \n", str);
      shmem_list_read(sink_list);
    }
    sleep(1);
    i++;
  }
  shmem_list_close(sink_list);
  return 0;
}
```

