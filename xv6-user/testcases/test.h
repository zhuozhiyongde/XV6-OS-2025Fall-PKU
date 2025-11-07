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

#endif