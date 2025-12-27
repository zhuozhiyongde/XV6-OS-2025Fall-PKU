#ifndef __SEMAPHORE_H
#define __SEMAPHORE_H

#include "types.h"
#include "spinlock.h"

// 内核可管理的最大信号量数量
#define NSEM 128

struct semaphore {
  struct spinlock lock; // 保护信号量内部状态
  int used;             // 是否已被分配
  int value;            // 当前计数值
};

void seminit(void);
int sem_create(int init_value);
int sem_destroy(int semid);
int sem_p(int semid);
int sem_v(int semid);

#endif
