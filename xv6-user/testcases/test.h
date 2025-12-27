#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

#if defined(SCHEDULER_TYPE) // Part 4
int set_timeslice(int);
int set_priority(int);
int get_priority(void);
#elif defined(TYPE) // Part 5
int getpgcnt(void);
int getprocsz(void);
#elif defined(ALGO) // Part 6
uint64 mmap(uint64 addr, int length, int prot, int flags, int fd, int offset);
int munmap(uint64 addr, int length);
int set_max_page_in_mem(int);
int get_swap_count(void);
int lru_access_notify(uint64 addr);

#elif defined(CASE) // Part 8
int sem_p(int);
int sem_v(int);
int sem_create(int);
int sem_destroy(int);
#endif
