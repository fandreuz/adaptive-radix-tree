#include "nodes.hpp"

inline void increaseReferences(Nodes::Header* node_header) {
  __atomic_fetch_add(&node_header->reference_count, 1, __ATOMIC_RELAXED);
}

inline void decreaseReferences(Nodes::Header* node_header) {
  uint16_t refcount;
#ifndef NDEBUG
  __atomic_load(&node_header->reference_count, &refcount, __ATOMIC_RELAXED);
  assert(refcount > 0);
#endif
  refcount =
      __atomic_sub_fetch(&node_header->reference_count, 1, __ATOMIC_RELAXED);
  if (refcount == 0) {
    free(node_header);
  }
}
