#ifndef LOCK
#define LOCK

#include "nodes.hpp"

namespace Lock {
inline Nodes::version_t awaitNodeUnlocked(Nodes::Header* node_header) {
  Nodes::version_t version;
  __atomic_load(&(node_header->version), &version, __ATOMIC_SEQ_CST);
  while ((version & 3) == 2) {
    __atomic_load(&(node_header->version), &version, __ATOMIC_SEQ_CST);
  }
  return version;
}

inline void writeUnlock(Nodes::Header* node_header) {
  __atomic_fetch_add(&(node_header->version), 2, __ATOMIC_SEQ_CST);
}

inline void writeUnlockObsolete(Nodes::Header* node_header) {
  __atomic_fetch_add(&(node_header->version), 3, __ATOMIC_SEQ_CST);
}

inline uint64_t setLockedBit(Nodes::version_t version) { return version + 2; }

inline bool isObsolete(Nodes::version_t version) { return (version & 1) == 1; }
} // namespace Lock

#define RESTART goto RESTART_POINT;

#define DECREMENT(node_header)                                                 \
  if (--node_header->reference_count == 0) {                                   \
    free(node_header);                                                         \
  }

#define READ_LOCK_OR_RESTART(node_header, version)                             \
  ++node_header->reference_count;                                              \
  version = Lock::awaitNodeUnlocked(node_header);                              \
  if (Lock::isObsolete(version)) {                                             \
    DECREMENT(node_header)                                                     \
    RESTART                                                                    \
  }

#define READ_UNLOCK_OR_RESTART(node_header, expected)                          \
  {                                                                            \
    Nodes::version_t actual;                                                   \
    __atomic_load(&(node_header->version), &actual, __ATOMIC_SEQ_CST);         \
    if (expected != actual) {                                                  \
      if (Lock::isObsolete(version)) {                                         \
        DECREMENT(node_header)                                                 \
      }                                                                        \
      RESTART                                                                  \
    }                                                                          \
  }

#define READ_UNLOCK_OR_RESTART_WITH_LOCKED_NODE(node_header, expected,         \
                                                node_header_locked)            \
  {                                                                            \
    Nodes::version_t actual;                                                   \
    __atomic_load(&(node_header->version), &actual, __ATOMIC_SEQ_CST);         \
    if (expected != actual) {                                                  \
      Lock::writeUnlock(node_header_locked);                                   \
      RESTART                                                                  \
    }                                                                          \
  }

#define UPGRADE_TO_WRITE_LOCK_OR_RESTART(node_header, expected)                \
  {                                                                            \
    Nodes::version_t exp_copy = expected;                                      \
    if (!__atomic_compare_exchange_n(                                          \
            &(node_header->version), &exp_copy, Lock::setLockedBit(exp_copy),  \
            false /* weak */, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {           \
      RESTART                                                                  \
    }                                                                          \
  }

#define UPGRADE_TO_WRITE_LOCK_OR_RESTART_WITH_LOCKED_NODE(                     \
    node_header, expected, node_header_locked)                                 \
  {                                                                            \
    Nodes::version_t exp_copy = expected;                                      \
    if (!__atomic_compare_exchange_n(                                          \
            &(node_header->version), &exp_copy, Lock::setLockedBit(exp_copy),  \
            false /* weak */, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {           \
      Lock::writeUnlock(node_header_locked);                                   \
      RESTART                                                                  \
    }                                                                          \
  }

#define CHECK_OR_RESTART(node_header, expected)                                \
  READ_UNLOCK_OR_RESTART(node_header, expected)

#endif // LOCK