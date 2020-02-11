#ifndef SHMEM_UTILS_H
#define SHMEM_UTILS_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif
#define VERSION_MAJOR 0
#define VERSION_MINOR 4
extern int LOGLEVEL;
#define LOG(level, fmt, args...)                                   \
  do {                                                             \
    if (level <= LOGLEVEL)                                          \
      printf("shl: %s(%d) " fmt "\n", __func__, __LINE__, ##args); \
  } while (0)

int shm_alloc(void **addr, const int shared_size, const char *shm_str, int *fd);
int shm_free(void **addr, const int shared_size, const char *shm_str, int fd);

#ifdef __cplusplus
}
#endif

#endif /* SHMEM_UTILS_H */
