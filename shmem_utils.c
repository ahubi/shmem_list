#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "shmem_utils.h"
int LOGLEVEL = 1;

int shm_alloc(void **addr, const int shared_size, const char *shmstr, int *fd) {
  int shm_fd;
  shm_fd = shm_open(shmstr, O_RDWR | O_CREAT, 0666);
  if (shm_fd < 0) {
    if (errno != ENOENT) {
      LOG(0, "cannot open existing shm registry segment (%s)", strerror(errno));
    }
    close(shm_fd);
    return -1;
  }
  if (fchmod(shm_fd, S_IRWXG | S_IRWXU | S_IRWXO) != 0) {
    LOG(0, "Error running fchmod (%s)", strerror(errno));
    return -1;
  }
  if (ftruncate(shm_fd, shared_size) != 0) {
    LOG(0, "Error running ftruncate (%s)", strerror(errno));
    return -1;
  }
  if ((*addr = mmap(0, (size_t)shared_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    shm_fd, 0)) == MAP_FAILED) {
    LOG(0, "cannot mmap existing shm registry segment (%s)", strerror(errno));
    close(shm_fd);
    return EINVAL;
  }
  if (mlock(addr, (size_t)shared_size) != 0) {
    LOG(0, "cannot lock memory (%s)", strerror(errno));
    return -1;
  }
  *fd = shm_fd;
  LOG(3, "OK");
  return 0;
}

int shm_free(void **addr, const int shared_size, const char *shmstr,
             int shm_fd) {
  munlock(*addr, (size_t)shared_size);
  munmap(*addr, (size_t)shared_size);
  close(shm_fd);
  shm_unlink(shmstr);
  return 0;
}
