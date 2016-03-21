#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include "threads/palloc.h"
#include "threads/thread.h"

void* frame_get_page(void* raw_upage, enum palloc_flags);
void frame_table_init(void);
void frame_free_page(tid_t thread);

#endif /* vm/frame.h */
