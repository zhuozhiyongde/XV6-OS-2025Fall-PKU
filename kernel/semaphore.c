#include "include/types.h"
#include "include/param.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/semaphore.h"

static struct {
  struct spinlock lock;         // 保护信号量槽的分配
  struct semaphore sems[NSEM];  // 固定的信号量表
} semtable;

/**
 * @brief 内核启动时初始化信号量表。
 */
void
seminit(void) {
  initlock(&semtable.lock, "semtable");
  for (int i = 0; i < NSEM; i++) {
    semtable.sems[i].used = 0;
    semtable.sems[i].value = 0;
    initlock(&semtable.sems[i].lock, "sem");
  }
}

/**
 * @brief 分配一个信号量并设置初始值。
 * @param init_value 初始计数
 * @return 成功返回信号量 id（下标），失败返回 -1
 */
int
sem_create(int init_value) {
  int id = -1;

  acquire(&semtable.lock);
  for (int i = 0; i < NSEM; i++) {
    if (!semtable.sems[i].used) {
      semtable.sems[i].used = 1;
      semtable.sems[i].value = init_value;
      id = i;
      break;
    }
  }
  release(&semtable.lock);

  return id;
}

/**
 * @brief 按 id 销毁信号量。
 * @param semid 由 sem_create 返回的信号量 id
 * @return 成功返回 0，失败返回 -1
 */
int
sem_destroy(int semid) {
  if (semid < 0 || semid >= NSEM) {
    return -1;
  }

  acquire(&semtable.lock);
  if (!semtable.sems[semid].used) {
    release(&semtable.lock);
    return -1;
  }
  semtable.sems[semid].used = 0;
  semtable.sems[semid].value = 0;
  release(&semtable.lock);

  // 若仍有阻塞在该信号量上的进程，唤醒它们避免永久休眠
  acquire(&semtable.sems[semid].lock);
  wakeup(&semtable.sems[semid]);
  release(&semtable.sems[semid].lock);
  return 0;
}

/**
 * @brief P 操作（等待），计数为 0 时阻塞。
 * @param semid 信号量 id
 * @return 成功返回 0，id 非法或未被使用返回 -1
 */
int
sem_p(int semid) {
  if (semid < 0 || semid >= NSEM) {
    return -1;
  }

  struct semaphore* s = &semtable.sems[semid];

  acquire(&s->lock);
  if (!s->used) {
    release(&s->lock);
    return -1;
  }
  while (s->value == 0) {
    // 如果等待期间信号量被销毁，则返回错误避免无限阻塞
    if (!s->used) {
      release(&s->lock);
      return -1;
    }
    sleep(s, &s->lock);
  }
  s->value--;
  release(&s->lock);
  return 0;
}

/**
 * @brief V 操作（释放），计数加一并唤醒等待者。
 * @param semid 信号量 id
 * @return 成功返回 0，id 非法或未被使用返回 -1
 */
int
sem_v(int semid) {
  if (semid < 0 || semid >= NSEM) {
    return -1;
  }

  struct semaphore* s = &semtable.sems[semid];

  acquire(&s->lock);
  if (!s->used) {
    release(&s->lock);
    return -1;
  }
  s->value++;
  wakeup(s);
  release(&s->lock);
  return 0;
}
