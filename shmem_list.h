#ifndef SHMEM_LIST_H
#define SHMEM_LIST_H

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define MEM_STRING 256

#ifdef __cplusplus
extern "C" {
#endif

/* Allocator Callback for shared memory */
typedef int (*mem_alloc)(void **vaddr, const int size, const char *str,
                         int *fd);
/* De-Allocator Callback for shared memory */
typedef int (*mem_free)(void **vaddr, const int size, const char *str,
                        const int fd);

typedef enum {
  LIST_TYPE_SOURCE,
  LIST_TYPE_SINK,
} shmem_list_type_t;

typedef struct {
  void *addr;               /* virtual address of the mapped memory in the address space of
                            calling process */
  char mem_str[MEM_STRING]; /* name of the shared memory */
  int mem_fd;               /* file descriptor of the shared memory */
  uint total_size;          /* Total Size of shared memory (number_packets x packet_size)*/
  uint number_packets;      /* Number of packets that can be buffered in this list */
  uint packet_size;         /* Packet Size*/
  shmem_list_type_t type;   /* Type of list being created */

  mem_alloc alloc;          /* Allocator Callback */
  mem_free free;            /* De-Allocator Callback */

  volatile int *read_offset;  /* Read offset of the packets*/
  volatile int *write_offset; /* Read offset of the packets*/
  
  /*  Holds status of semaphore initializaiton
  *   Used to avoid multiple semaphore initialization.
  */
  volatile int *sem_state;

  /*Array holding the actual packet size for each packet. 
  * Actual packet size will be determined during read / write operations 
  * It can be <= packet_size
  */
  uint **packet_sizes;

  /* Array of packets in shared memory */
  void **packets; 

  /* Semaphores */
  sem_t *countsem; /* Semaphore for full packet count */
  sem_t *spacesem; /* Sempahore for available empty packet count */
} shmem_list_t;

/** 
 * Opens a list of packets in shared memory
 * 
 * @param shmem_name - shared memory name where the list will be allocated
 * @param list_type - type of list, sink reads from list or source writes into list
 * @param packet_size - size of one packet
 * @param number_of_packets - number of packets to be in the list
 * @param address - location of the packets, use 0 by default, currently not in use
 * @return - pointer to allocated list, NULL pointer in case of error.
 */
shmem_list_t *shmem_list_mem_open(const char *shmem_name,
                                  const shmem_list_type_t list_type,
                                  const int packet_size,
                                  const int number_of_packets, void *address);
/**
 * Closes shared memory of the list and frees the memory
 * If a use case requires closing of the list both Consumer and Producer must close the list. 
 * Closing and reopening on only one site is not supported. 
 * 
 * @param p_list - list to be closed
 * @return - 0 - no error, else some error happened
 */
int shmem_list_close(shmem_list_t *p_list);

/**
 * Aquire the pointer of the packet to read. 
 * This function can block the caller. If data is availabe for read it will 
 * return immediatelly and provide a valid pointer to read from. 
 * If data isn't available it will block till data is available or the timeout is reached.
 * In case of timeout the return will be NULL pointer. 
 * It's not adviced to provide a timeout of 0.
 * 
 * @param p_list - list to aquire the read pointer
 * @param size - actual size in the packet (<= packet_size)
 * @param timeout - time to be maximum blocked on the call in seconds and nanoseconds
 * @return - pointer to read the packet, NULL in case of timeout or other error
 */
void *shmem_list_acquire_read_packet(shmem_list_t *p_list,
                                     int *size,
                                     struct timespec timeout);

/**
 * Commit read of one packet.
 * This funciton is non blocking.
 * 
 * @param p_list - list to commit the read
 * @return - 0 in case of success, otherwise error
 */
int shmem_list_read(shmem_list_t *p_list);

/**
 * Aquire the pointer of the packet to write
 * This function can block the caller. If data is availabe for write it will
 * return immediatelly and provide a valid pointer to write to. 
 * If space isn't available it will block till space is available or the timeout is reached.
 * In case of timeout the return will be NULL pointer. 
 * It's not adviced to provide a timeout of 0.
 * 
 * @param p_list - list to aquire the write pointer
 * @param timeout - time to be maximum blocked on the call in seconds and nanoseconds
 * @return: pointer to write the packet, NULL in case of timeout or other error
 */
void *shmem_list_acquire_write_packet(shmem_list_t *p_list,
                                      struct timespec timeout);

/**
 * Commit write of one packet
 * This function is non blocking.
 * 
 * @param p_list - list to commit the write
 * @param size - actual size in the packet (<= packet_size)
 * @return: 0 in case of success, otherwise error
 */
int shmem_list_write(shmem_list_t *p_list, const int size);


/**
 * Changes loglevel of the LOG macro
 * This function is non blocking
 * 
 * @param level - log level 0 to 5. 
 * 0 - lowest level of debug output
 * 5 - highest level of output
 */
void shmem_list_set_loglevel(const int level);

#ifdef __cplusplus
}
#endif

#endif  // SHMEM_LIST_H
