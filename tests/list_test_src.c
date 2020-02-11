#include <stdint.h>
#include <stdio.h>
#include "../shmem_list.h"

#define shmem "TestSharedMem"

int main() {
  int i, j = 0;
  struct timespec ts;
  shmem_list_type_t l_type = LIST_TYPE_SOURCE;
  shmem_list_t *src_list = shmem_list_mem_open(shmem, l_type, 1024, 2, 0);
  if (src_list == NULL) {
    printf("shmem_list_initfailed \n");
    return -1;
  }
  i = 0;
  ts.tv_sec = 1;
  ts.tv_nsec = 0; //500000000; //500ms
  char *str = (char*)malloc(32);
  while (j<101) {
    void *ptr = shmem_list_acquire_write_packet(src_list, ts);
    if (ptr) {
      sprintf(str,"Producer count %d", i++);
      memcpy(ptr, str, strlen(str));
      printf("write: %s size %lu \n", str, strlen(str));
      shmem_list_write(src_list, strlen(str));
    } else {
      if (j % 10 == 0)
        printf("source %s returning NULL pointer %d, timeout\n", __FUNCTION__, j);
    }
    sleep(1);
    j++;
  }
  shmem_list_close(src_list);
  return 0;
}
