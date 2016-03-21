#ifndef VM_SWAP_H 
#define VM_SWAP_H

#include <stdint.h>
#include <stddef.h>
#include "threads/thread.h"

void init_swap_table(void);
void evict_to_swap(tid_t thread, void *raw_upage, void* kpage);
void* swap_restore_page(void* raw_upage);  /* raw because user virtual address might not have been rounded down
                                              ie bit 0-11 is still the offset of the page */

//void per_process_cleanup_swap(void);
#endif
