#if defined(SCHEDULER_TYPE) // Part 4

#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

int set_timeslice(int);
int set_priority(int);
int get_priority(void);

#elif defined(TYPE) // Part 5

#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

int getpgcnt(void);
int getprocsz(void);

#elif defined(ALGO) // Part 6

#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

uint64 mmap(uint64 addr, int length, int prot, int flags, int fd, int offset);
int munmap(uint64 addr, int length);
int set_max_page_in_mem(int);
int get_swap_count(void);
int lru_access_notify(uint64 addr);

#endif