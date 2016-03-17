#include "frame.h"
#include "list.h"
#include "threads/palloc.h"
#include <stddef.h>

// should actually use hash but for testing correctness use list for now.
struct frame
  {
    struct list_elem elem;
    uint8_t *upage;
    void* physical;
  };

static struct list frame_table;

void 
frame_table_init(void) 
{
  list_init(&frame_table);
}

// only for user processes. 
//kernel should directly use palloc get page as original
void* 
frame_get_page(uint8_t *upage)
{
  void* result = palloc_get_page(PAL_USER);
  
//for now, no swapping, just panic if no more memory.
  if (result == NULL) {
	ASSERT false;
  }

//add entry to frame table
  struct frame* entry = malloc(sizeof(struct frame));
  entry->upage = upage;
  entry->physical = result;
  list_push_back(&frame_table, &entry->elem);

  return result;

}

void* 
frame_get_page_zero(uint8_t *upage)
{
  void* result = palloc_get_page(PAL_USER | PAL_ZERO);
  
//for now, no swapping, just panic if no more memory.
  if (result == NULL) {
	ASSERT false;
  }

//add entry to frame table
  struct frame* entry = malloc(sizeof(struct frame));
  entry->upage = upage;
  entry->physical = result;
  list_push_back(&frame_table, &entry->elem);

  return result;

}
