#include "shmem_list.h"
#include <string.h>
#include <sys/ioctl.h>
#include "shmem_utils.h"
#include "time.h"

/*
 * @Description: Sets a pointer to (m)alloc function of the list
 * @p_list: pointer to the list
 * @p_free: pointer to the alloc funciton
 */
static int  shmem_list_set_mem_alloc(shmem_list_t *p_list, mem_alloc p_alloc);

/*
 * @Description: Sets a pointer to free function of the list
 * @p_list: pointer to the list
 * @p_free: pointer to the free funciton
 */
static int  shmem_list_set_mem_free(shmem_list_t *p_list, mem_free p_free);

/*
 * @Description: Helper funciton for allocation
 */
static int shmem_list_private_mem_alloc(const char *shmem_name,
                                        shmem_list_t *list,
                                        const shmem_list_type_t list_type,
                                        const int packet_size,
                                        const int number_of_packets,
                                        void *address);

void shmem_list_set_loglevel(const int level){
  LOGLEVEL = level;
}
/*
 * @Description: Helper funciton for offset calculation
 */
static int shmem_get_offset(shmem_list_t *list, int offset) {
  return (offset + list->packet_size) % list->total_size;
}

static void shmem_normalize_timespec(struct timespec *ts,
                                     const struct timespec tm) {
  if (ts) {
    ts->tv_nsec += tm.tv_nsec;
    ts->tv_sec += tm.tv_sec;
    ts->tv_sec += ts->tv_nsec / 1000000000;
    ts->tv_nsec %= 1000000000;
  }
}

shmem_list_t *shmem_list_mem_open(const char *shmem_name,
                                   const shmem_list_type_t type,
                                   const int packet_size,
                                   const int number_of_packets, void *address) {
  LOG(0, "lib version %d.%d", VERSION_MAJOR, VERSION_MINOR);
  if (strlen(shmem_name) > MEM_STRING) {
    LOG(0, "error: shmem_name is too long %lu, maximum length %d",
        strlen(shmem_name), MEM_STRING);
    return NULL;
  }
  LOG(0, "shmem_name %s type %d, packet_size %d number_of_packets %d address %p", 
  shmem_name, type, packet_size, number_of_packets, address);

  shmem_list_t *p_list = (shmem_list_t *)malloc(sizeof(shmem_list_t));
  shmem_list_set_mem_alloc(p_list, shm_alloc);
  shmem_list_set_mem_free(p_list, shm_free);
  if (p_list != NULL) {
    int retVal = shmem_list_private_mem_alloc(
        shmem_name, p_list, type, packet_size, number_of_packets, address);
    if (retVal == 0) {
      LOG(0,"semstate 0x%x", *p_list->sem_state);
      if (*p_list->sem_state != 0xCAFE) {
        LOG(0, "init semaphores");
        *p_list->sem_state = 0xCAFE;
        if (sem_init(p_list->spacesem, 1, p_list->number_packets) == -1) {
          LOG(0, "sem_init error %s", strerror(errno));
          return NULL;
        }
        if (sem_init(p_list->countsem, 1, 0) == -1) {
          LOG(0, "sem_init error %s", strerror(errno));
          return NULL;
        }
      }
    }
  }
  return p_list;
}

int shmem_list_set_mem_alloc(shmem_list_t *p_list, mem_alloc p_alloc) {
  int retVal = -1;
  if (p_list != NULL && p_alloc != NULL) {
    p_list->alloc = p_alloc;
    retVal = 0;
  }
  return retVal;
}

int shmem_list_set_mem_free(shmem_list_t *p_list, mem_free p_free) {
  int retVal = -1;
  if (p_list != NULL && p_free != NULL) {
    p_list->free = p_free;
    retVal = 0;
  }
  return retVal;
}

/*
 * @Description read/write: Return 0 if all is OK.
 * TODO : Check return code for overflow
 */
int shmem_list_read(shmem_list_t *p_list) {
  int retVal = -1, count = 0;
  if ((p_list != NULL) && (p_list->number_packets > 0)) {
    sem_getvalue(p_list->spacesem, &count);
    *p_list->read_offset = shmem_get_offset(p_list, *p_list->read_offset);
    if (count < p_list->number_packets) {
      retVal = sem_post(p_list->spacesem);
    }
    LOG(3, "ssem %d, offset %d", count, *p_list->read_offset);
  }
  return retVal;
}

void *shmem_list_acquire_read_packet(shmem_list_t *p_list, 
                                     int *size,
                                     struct timespec timeout) {
  void *retVal = NULL;
  struct timespec ts;
  int s, packetIndex;
  if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
    LOG(0,"clock_gettime %s", strerror(errno));
    return NULL;
  }
  
  shmem_normalize_timespec(&ts, timeout);

  if (p_list != NULL) {
    while ((s = sem_timedwait(p_list->countsem, &ts)) == -1 && errno == EINTR)
      continue; /* Restart if interrupted by handler */

    if (s != -1){
      packetIndex = (*p_list->read_offset / p_list->packet_size) % p_list->number_packets;
      retVal = p_list->packets[0] + *(p_list->read_offset);
      *size =  *p_list->packet_sizes[packetIndex] % (p_list->packet_size + 1);
      LOG(4, "packetIndex %d size %d sored size %d", packetIndex, *size, *p_list->packet_sizes[packetIndex]);
    }
    else {
      if (errno != ETIMEDOUT)
        LOG(0, "sem_timedwait error %s", strerror(errno));
    }
  }
  return retVal;
}

/*
 * @Description read/write: Return 0 if all is OK.
 * TODO : Check return code for overflow
 */
int shmem_list_write(shmem_list_t *p_list, const int size) {
  int retVal = -1, count = 0, packetIndex;
  if ((p_list != NULL) && (p_list->number_packets > 0)) {
    sem_getvalue(p_list->countsem, &count);
    
    //firs set the size of the packet
    packetIndex = (*p_list->write_offset / p_list->packet_size) % p_list->number_packets;
    *p_list->packet_sizes[packetIndex] = size % (p_list->packet_size + 1);
    LOG(4, "packetIndex %d size %d", packetIndex, *p_list->packet_sizes[packetIndex]);
    //move the offset to next packet
    *p_list->write_offset = shmem_get_offset(p_list, *p_list->write_offset);
    
    //post semaphore not more than we have packets
    if (count < p_list->number_packets) {
      retVal=sem_post(p_list->countsem);
    }
    LOG(3, "csem %d, offset %d", count, *p_list->write_offset);
  }
  return retVal;
}

void *shmem_list_acquire_write_packet(shmem_list_t *p_list,
                                      struct timespec timeout) {
  void *retVal = NULL;
  struct timespec ts;
  int s;
  if (clock_gettime(CLOCK_REALTIME, &ts) == -1) return NULL;

  shmem_normalize_timespec(&ts, timeout);

  if (p_list != NULL) {
    while ((s = sem_timedwait(p_list->spacesem, &ts)) == -1 && errno == EINTR)
      continue; /* Restart if interrupted by handler */

    if (s != -1)
      retVal = p_list->packets[0] + *(p_list->write_offset);
    else {
      if (errno != ETIMEDOUT)
        LOG(0, "sem_timedwait error %s", strerror(errno));
    }
  }
  return retVal;
}

int shmem_list_close(shmem_list_t *p_list) {
  int retVal = 0;
  if (p_list != NULL) {
    if (*p_list->sem_state != 0x00 ) {
      retVal |= sem_destroy(p_list->countsem);
      retVal |= sem_destroy(p_list->spacesem);
      *p_list->sem_state = 0x00;
    }
    if (p_list->packets != NULL) {
      free(p_list->packets);
      p_list->packets = NULL;
    }
    if (p_list->packet_sizes != NULL) {
      free(p_list->packet_sizes);
      p_list->packet_sizes = NULL;
    }
    if (p_list->free != NULL) {
      p_list->free(&p_list->addr, 
                    p_list->total_size + 
                    (p_list->number_packets * sizeof(int)) + 
                    sizeof(shmem_list_t),
                    p_list->mem_str, p_list->mem_fd);
    }
    if(p_list!=NULL){
      free(p_list);
      p_list = NULL;
    }
  }
  if (retVal == 0)
    LOG(0, "success, lib version %d.%d", VERSION_MAJOR, VERSION_MINOR);
  else
    LOG(0, "error %s", strerror(errno));

  return retVal;
}

int shmem_list_private_mem_alloc(const char *shmem_name, shmem_list_t *p_list,
                                 const shmem_list_type_t list_type,
                                 const int packet_size,
                                 const int number_of_packets, void *address) {
  int retVal = 0, i, sizes_array;
  if (p_list->alloc != NULL) {
    p_list->total_size = packet_size * number_of_packets;
    p_list->type = list_type;
    p_list->packet_size = packet_size;
    p_list->number_packets = number_of_packets;
    sizes_array = (p_list->number_packets * sizeof(int)); /*Array holding actual packet size*/
    if (shmem_name != NULL) {
      strcpy(p_list->mem_str, shmem_name);
    }
    retVal = p_list->alloc(&p_list->addr, 
                            p_list->total_size + sizes_array + sizeof(shmem_list_t),
                            p_list->mem_str, 
                            &p_list->mem_fd);
    if (retVal == -1) {
      LOG(0, "alloc failed");
      return retVal;
    }
    if (p_list->addr != NULL) {
      p_list->spacesem     = (sem_t *)(p_list->addr);
      p_list->countsem     = (sem_t *)(p_list->addr) + 1;
      p_list->read_offset  = ((volatile int*) ((p_list->addr) + (2 * sizeof(sem_t))));
      p_list->write_offset = ((volatile int*) ((p_list->addr) + (2 * sizeof(sem_t)) + sizeof(int)));
      p_list->sem_state    = ((volatile int*) ((p_list->addr) + (2 * sizeof(sem_t)) + (sizeof(int)*2)));
      LOG(3, "aloc addr: spacesem %p, countsem %p, read_offset %p, write_offset %p, sem_state %p",
            p_list->spacesem, p_list->countsem, p_list->read_offset, p_list->write_offset, p_list->sem_state);
      
      /*  Only producers initializes read and write position
      *   If producer is started later or restarted 
      *   it will reset the read position of the consumer
      */  
      if (p_list->type == LIST_TYPE_SOURCE) {
        *(p_list->write_offset) = 0;
        *(p_list->read_offset) = 0;
      }
      

      // No DMA available, use regular shared memory
      p_list->packet_sizes = (int **)malloc(sizeof(int *) * (size_t)p_list->number_packets);
      p_list->packets = (void **)malloc(sizeof(void *) * (size_t)p_list->number_packets);
      for (i = 0; i < p_list->number_packets; i++) {
        p_list->packet_sizes[i] = ((char *)p_list->addr + sizeof(shmem_list_t) + (sizeof(int) * i));
        p_list->packets[i] = ((char *)p_list->addr + sizeof(shmem_list_t) + sizes_array + (p_list->packet_size * i));
        LOG(3, "packet %d, addr %p", i, p_list->packets[i]);
      }
    }
    LOG(3, "total allocated size %lu", p_list->total_size + sizeof(shmem_list_t));
  } else {
    retVal = -1;
    LOG(0, "alloc function is NULL");
  }
  return retVal;
}
