#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h>
#include <stddef.h>
#include "vm/frame.h"
#include "threads/vaddr.h"


void supp_page_table_init(void); 
void spt_mark_evicted(tid_t tid, void* upage); //upage already must been rounded down
void spt_unmark_evicted(tid_t tid, void* upage);

void cleanup_evicted(tid_t tid); //use when process exit to clean up resources used for evicted and make swap slots as free

void* supp_pt_locate_fault (void* upage);

#endif
