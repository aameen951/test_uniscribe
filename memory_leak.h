
#include <stdlib.h>
#include <stdio.h>



int allocated_memory = 0;
void *all_memory[2000];
int all_memory_sizes[2000];
int all_memory_count;
void *_alloc(int count, int size){
  auto result = calloc(count, size);
  allocated_memory += count * size;
  all_memory[all_memory_count] = result;
  all_memory_sizes[all_memory_count] = count * size;
  all_memory_count++;
  return result;
}
void _free(void *memory){
  free(memory);
  for(int i=0; i<all_memory_count; i++) {
    if(all_memory[i] == memory){
      allocated_memory -= all_memory_sizes[i];
      all_memory_count--;
      all_memory_sizes[i] = all_memory_sizes[all_memory_count];
      all_memory[i] = all_memory[all_memory_count];
      return;
    }
  }
  printf("Bad free\n");
}
void *_realloc(void *memory, int size){
  allocated_memory += size;
  auto result = realloc(memory, size);
  if(!memory) {
    all_memory[all_memory_count] = result;
    all_memory_sizes[all_memory_count] = size;
    all_memory_count++;
  } else {
    for(int i=0; i<all_memory_count; i++) {
      if(all_memory[i] == memory){
        allocated_memory -= all_memory_sizes[i];
        all_memory_sizes[i] = size;
        all_memory[i] = result;
        return result;
      }
    }
    printf("Bad realloc\n");
  }
  return result;
}
#ifdef ENABLE_MEMORY_LEAK_HOOK

#define calloc _alloc
#define free _free
#define realloc _realloc

void dump_non_free_memory(){
  printf("Allocated memory %d, %d\n", allocated_memory, all_memory_count);
}
#else
void dump_non_free_memory(){
}
#endif