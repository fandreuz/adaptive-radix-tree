#include "lock.hpp"

namespace Lock {
size_t awaitNodeUnlocked(Nodes::Header* node) {
  size_t version;
  __atomic_load(&(node->version), &version, __ATOMIC_SEQ_CST);
  while ((version & 2) == 2) {
    __atomic_load(&(node->version), &version, __ATOMIC_SEQ_CST);
  }
  return version;
}

bool isObsolete(size_t version) { return (version & 1) == 1; }

void writeUnlock(Nodes::Header* node) {
  __atomic_fetch_add(&(node->version), 2, __ATOMIC_SEQ_CST);
}

void writeUnlockObsolete(Nodes::Header* node) {
  __atomic_fetch_add(&(node->version), 3, __ATOMIC_SEQ_CST);
}

size_t setLockedBit(size_t version) { return version + 2; }
} // namespace Lock
