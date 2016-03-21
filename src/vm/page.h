#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h>
#include <stddef.h>
#include "vm/frame.h"
#include "threads/vaddr.h"

// TODO: is this right?
#define PG_SIZE (4096)
#define PG_TOTAL ((uint32_t)PHYS_BASE/PG_SIZE)

/*
struct supp_page_table {
  struct hash page_table;
}
*/

//TODO: have we put page offfset back when returning kpage in pagedir_get_page?
//      if not we can make a funciton to manipulate the offset here, which uses
//      the original pagedir_get_page first
struct supp_page_table {
  uint32_t* pagedir;
  bool evicted[PG_TOTAL]; 
};
struct supp_page_table* spt_create();
void spt_destroy (struct supp_page_table* spt);

/*
struct page 
{
  tid_t tid;
  void* upage;
  void* kpage;
}
*/

//void init_page(struct page* p, tid_t owner_tid, void* raw_upage);
//void* supp_pt_locate_fault (struct supp_page_table* spt, euint8_t* upage);
void* supp_pt_locate_fault (void* upage);

//void* page_table_get_kpage (void* raw_upage); // for process to use; given upage returns kpage
//bool supp_page_table_remove (tid_t tid, void* raw_upage); // for frame.c to use when evict 
//bool supp_page_table_add (tid_t tid, void* raw_upage);

//void per_process_cleanup (struct list* per_process_upages); // called at process_exit to clean up all page resources that was palloc'd by the exiting process // TODO: add this to process_exit
/*
struct per_process_upages_elem
{
  void* upage; //upage address that is already rounded down
  list_elem elem;
}
*/
#endif
