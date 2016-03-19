#ifndef VM_SWAP_H 
#define VM_SWAP_H

#include <stdint.h>
#include <stddef.h>

void init_swap_table(void);
void evict_to_swap(void* kpage);
void* swap_restore_page(void* upage);

#endif
