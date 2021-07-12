#ifndef D_RESIZABLE_BUFFER_H
#define D_RESIZABLE_BUFFER_H

#include "my_std.h"
#include <stdlib.h>

struct ResizableBuffer {
  u8 *ptr;
  int size;
};
void *_rb_ensure_size(ResizableBuffer *b, int count, int element_size){
  auto new_size = count * element_size;
  if(new_size > b->size) {
    b->size = new_size;
    b->ptr = (u8 *)realloc(b->ptr, b->size);
  }
  return b->ptr;
}
#define rb_ensure_size(b, count, element_type) ((element_type *)_rb_ensure_size(b, count, sizeof(element_type)))
void rb_free(ResizableBuffer *rb){
  if(rb->ptr){
    free(rb->ptr);
    rb->ptr = NULL;
  }
}

#endif