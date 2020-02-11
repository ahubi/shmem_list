#include <stdint.h>
#include <stdio.h>
#include "../shmem_list.h"

#define shmem "TestSharedMem"

int main() {
  // This is a memory sink
  struct timespec ts;
  shmem_list_type_t l_type = LIST_TYPE_SINK;
  shmem_list_t *sink_list = shmem_list_mem_open(shmem, l_type, 1024, 2, 0);
  if (sink_list == NULL) {
    printf("shmem_list_mem_open failed \n");
    return -1;
  }
  shmem_list_set_loglevel(1);
  int j = 0;
  ts.tv_sec = 1;
  ts.tv_nsec = 0; //500000000; //500ms
  char *str = (char*)malloc(32);
  int size=0;
  while (j<101) {
    void *ptr = shmem_list_acquire_read_packet(sink_list, &size, ts);
    memset(str, 0, 32);
    if (ptr) {
      memcpy(str, ptr, size);
      printf("read: %s packet size %d, actual size %d\n", str, sink_list->packet_size, size);
      shmem_list_read(sink_list);
    } else {
      if (j % 10 == 0)
        printf("sink %s returning NULL pointer %d, timeout\n", __FUNCTION__, j);
    }
    j++;
  }
  shmem_list_close(sink_list);
  return 0;
}
