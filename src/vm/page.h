#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h>
#include <stddef.h>
#include "vm/frame.h"
#include "thread/vaddr.h"

struct supp_page_table {
  struct hash page_table;
}

struct page 
{
  tid_t tid;
  void* upage;
  void* kpage;
}

void init_page(struct page* p, tid_t owner_tid, void* raw_upage);
void* supp_pt_locate (struct supp_page_tabl* spt, euint8_t* upage);

void* page_table_get_kpage (void* raw_upage); // for process to use; given upage returns kpage
bool supp_page_table_remove (tid_t tid, void* raw_upage); // for frame.c to use when evict 
bool supp_page_table_add (tid_t tid, void* raw_upage);

void per_process_cleanup (struct list* per_process_upages); // called at process_exit to clean up all page resources that was palloc'd by the exiting process // TODO: add this to process_exit
struct per_process_upages_elem
{
  void* upage; //upage address that is already rounded down
  list_elem elem;
}
#endif
