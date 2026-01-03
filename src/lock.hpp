#include "nodes.hpp"

namespace Lock {
Nodes::version_t awaitNodeUnlocked(Nodes::version_t* version_ptr);
bool isObsolete(Nodes::version_t version);
void writeUnlock(Nodes::version_t* version_ptr);
void writeUnlockObsolete(Nodes::version_t* version_ptr);
uint64_t setLockedBit(Nodes::version_t version);
} // namespace Lock

#define RESTART goto RESTART_POINT;

#define READ_LOCK_OR_RESTART(version_ptr, version)                             \
  version = Lock::awaitNodeUnlocked(version_ptr);                              \
  if (Lock::isObsolete(version)) {                                             \
    RESTART                                                                    \
  }

#define READ_UNLOCK_OR_RESTART(version_ptr, expected)                          \
  {                                                                            \
    Nodes::version_t actual;                                                   \
    __atomic_load(version_ptr, &actual, __ATOMIC_SEQ_CST);                     \
    if (expected != actual) {                                                  \
      RESTART                                                                  \
    }                                                                          \
  }

#define READ_UNLOCK_OR_RESTART_WITH_LOCKED_NODE(version_ptr, expected,         \
                                                version_ptr_locked)            \
  {                                                                            \
    Nodes::version_t actual;                                                   \
    __atomic_load(version_ptr, &actual, __ATOMIC_SEQ_CST);                     \
    if (expected != actual) {                                                  \
      Lock::writeUnlock(version_ptr_locked);                                   \
      RESTART                                                                  \
    }                                                                          \
  }

#define UPGRADE_TO_WRITE_LOCK_OR_RESTART(version_ptr, expected)                \
  {                                                                            \
    Nodes::version_t exp_copy = expected;                                      \
    if (!__atomic_compare_exchange_n(                                          \
            version_ptr, &exp_copy, Lock::setLockedBit(exp_copy),              \
            false /* weak */, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {           \
      RESTART                                                                  \
    }                                                                          \
  }

#define UPGRADE_TO_WRITE_LOCK_OR_RESTART_WITH_LOCKED_NODE(                     \
    version_ptr, expected, version_ptr_locked)                                 \
  {                                                                            \
    Nodes::version_t exp_copy = expected;                                      \
    if (!__atomic_compare_exchange_n(                                          \
            version_ptr, &exp_copy, Lock::setLockedBit(exp_copy),              \
            false /* weak */, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {           \
      Lock::writeUnlock(version_ptr_locked);                                   \
      RESTART                                                                  \
    }                                                                          \
  }

#define CHECK_OR_RESTART(version_ptr, expected)                                \
  READ_UNLOCK_OR_RESTART(version_ptr, expected)
