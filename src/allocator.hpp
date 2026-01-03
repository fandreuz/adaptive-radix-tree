#ifndef ALLOCATOR
#define ALLOCATOR

#include <cstdlib>
#include <cstring>

struct MallocAlloc {
  template <size_t S> static void* allocate() { return malloc(S); }
  static void* allocateDynamic(size_t size) { return malloc(size); }
  template <size_t S> static void release(void* ptr) { free(ptr); }
  static void releaseDynamic(void* ptr) { free(ptr); }
};

#endif // ALLOCATOR
