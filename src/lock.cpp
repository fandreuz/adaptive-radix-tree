#include "lock.hpp"

namespace Lock {
uint64_t awaitNodeUnlocked(uint64_t* version_ptr) {
  uint64_t version;
  __atomic_load(version_ptr, &version, __ATOMIC_SEQ_CST);
  while ((version & 3) == 2) {
    __atomic_load(version_ptr, &version, __ATOMIC_SEQ_CST);
  }
  return version;
}

bool isObsolete(uint64_t version) { return (version & 1) == 1; }

void writeUnlock(uint64_t* version_ptr) {
  __atomic_fetch_add(version_ptr, 2, __ATOMIC_SEQ_CST);
}

void writeUnlockObsolete(uint64_t* version_ptr) {
  __atomic_fetch_add(version_ptr, 3, __ATOMIC_SEQ_CST);
}

uint64_t setLockedBit(uint64_t version) { return version + 2; }
} // namespace Lock
