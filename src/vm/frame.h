#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include "threads/palloc.h"

void* frame_get_page(void* raw_upage, enum palloc_flags);
void frame_table_init(void);

#endif /* vm/frame.h */
