#include "lock.hpp"

namespace Lock {
Nodes::version_t awaitNodeUnlocked(Nodes::version_t* version_ptr) {
  Nodes::version_t version;
  __atomic_load(version_ptr, &version, __ATOMIC_SEQ_CST);
  while ((version & 3) == 2) {
    __atomic_load(version_ptr, &version, __ATOMIC_SEQ_CST);
  }
  return version;
}

bool isObsolete(Nodes::version_t version) { return (version & 1) == 1; }

void writeUnlock(Nodes::version_t* version_ptr) {
  __atomic_fetch_add(version_ptr, 2, __ATOMIC_SEQ_CST);
}

void writeUnlockObsolete(Nodes::version_t* version_ptr) {
  __atomic_fetch_add(version_ptr, 3, __ATOMIC_SEQ_CST);
}

uint64_t setLockedBit(Nodes::version_t version) { return version + 2; }
} // namespace Lock
