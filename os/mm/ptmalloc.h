#ifndef MM_PTMALLOC_H
#define MM_PTMALLOC_H

void* ptmalloc_malloc(long unsigned int size);
void ptmalloc_free(void* ptr);

#endif