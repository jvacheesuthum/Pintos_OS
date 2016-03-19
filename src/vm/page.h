#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h>
#include <stddef.h>
#include "vm/frame.h"
#include "thread/vaddr.h"

struct supp_page_table {
  uint32_t *pagedir;  /* the normal pagetable from pintos skeleton */
  bool evicted[PHYS_BASE/PGSIZE]; /* a bit map to track if a page at a user virtual address exists */
}

void* supp_pt_locate (struct supp_page_tabl* spt, euint8_t* upage);

#endif
