#include "nodes.hpp"
#include <cstddef>

namespace Lock {
size_t awaitNodeUnlocked(Nodes::Header* node);
bool isObsolete(size_t version);
void writeUnlock(Nodes::Header* node);
void writeUnlockObsolete(Nodes::Header* node);
size_t setLockedBit(size_t version);
} // namespace Lock

#define RESTART goto RESTART_POINT;

#define READ_LOCK_OR_RESTART(node, version)                                    \
  version = Lock::awaitNodeUnlocked(node);                                     \
  if (Lock::isObsolete(version)) {                                             \
    RESTART                                                                    \
  }

#define READ_UNLOCK_OR_RESTART(node, expected)                                 \
  {                                                                            \
    size_t actual;                                                             \
    __atomic_load(&(node->version), &actual, __ATOMIC_SEQ_CST);                \
    if (expected != actual) {                                                  \
      RESTART                                                                  \
    }                                                                          \
  }

#define READ_UNLOCK_OR_RESTART_WITH_LOCKED_NODE(node, expected, locked_node)   \
  {                                                                            \
    size_t actual;                                                             \
    __atomic_load(&(node->version), &actual, __ATOMIC_SEQ_CST);                \
    if (expected != actual) {                                                  \
      Lock::writeUnlock(locked_node);                                          \
      RESTART                                                                  \
    }                                                                          \
  }

#define UPGRADE_TO_WRITE_LOCK_OR_RESTART(node, expected)                       \
  {                                                                            \
    size_t exp_copy = expected;                                                \
    if (!__atomic_compare_exchange_n(                                          \
            &(node->version), &exp_copy, Lock::setLockedBit(exp_copy),         \
            false /* weak */, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {           \
      RESTART                                                                  \
    }                                                                          \
  }

#define UPGRADE_TO_WRITE_LOCK_OR_RESTART_WITH_LOCKED_NODE(node, expected,      \
                                                          locked_node)         \
  {                                                                            \
    size_t exp_copy = expected;                                                \
    if (!__atomic_compare_exchange_n(                                          \
            &(node->version), &exp_copy, Lock::setLockedBit(exp_copy),         \
            false /* weak */, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {           \
      Lock::writeUnlock(locked_node);                                          \
      RESTART                                                                  \
    }                                                                          \
  }

#define CHECK_OR_RESTART(node, expected) READ_UNLOCK_OR_RESTART(node, expected)
