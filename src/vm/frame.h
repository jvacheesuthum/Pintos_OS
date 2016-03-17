#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>

void* frame_get_page(uint8_t *);
void* frame_get_page_zero(uint8_t *);
void frame_table_init(void);

#endif /* vm/frame.h */
