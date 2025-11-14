#ifndef __KALLOC_H
#define __KALLOC_H

#include "types.h"

void*           kalloc(void);
void            kfree(void *);
void            kinit(void);
uint64          freemem_amount(void);
uint64          allocated_pages(void);
void            incref(uint64 pa);
int             getref(uint64 pa);

#endif