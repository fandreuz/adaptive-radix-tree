#include <cstdint>

namespace Lock {
uint64_t awaitNodeUnlocked(uint64_t* version_ptr);
bool isObsolete(uint64_t version);
void writeUnlock(uint64_t* version_ptr);
void writeUnlockObsolete(uint64_t* version_ptr);
uint64_t setLockedBit(uint64_t version);
} // namespace Lock

#define RESTART goto RESTART_POINT;

#define READ_LOCK_OR_RESTART(version_ptr, version)                             \
  version = Lock::awaitNodeUnlocked(version_ptr);                              \
  if (Lock::isObsolete(version)) {                                             \
    RESTART                                                                    \
  }

#define READ_UNLOCK_OR_RESTART(version_ptr, expected)                          \
  {                                                                            \
    uint64_t actual;                                                           \
    __atomic_load(version_ptr, &actual, __ATOMIC_SEQ_CST);                     \
    if (expected != actual) {                                                  \
      RESTART                                                                  \
    }                                                                          \
  }

#define READ_UNLOCK_OR_RESTART_WITH_LOCKED_NODE(version_ptr, expected,         \
                                                version_ptr_locked)            \
  {                                                                            \
    uint64_t actual;                                                           \
    __atomic_load(version_ptr, &actual, __ATOMIC_SEQ_CST);                     \
    if (expected != actual) {                                                  \
      Lock::writeUnlock(version_ptr_locked);                                   \
      RESTART                                                                  \
    }                                                                          \
  }

#define UPGRADE_TO_WRITE_LOCK_OR_RESTART(version_ptr, expected)                \
  {                                                                            \
    uint64_t exp_copy = expected;                                              \
    if (!__atomic_compare_exchange_n(                                          \
            version_ptr, &exp_copy, Lock::setLockedBit(exp_copy),              \
            false /* weak */, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {           \
      RESTART                                                                  \
    }                                                                          \
  }

#define UPGRADE_TO_WRITE_LOCK_OR_RESTART_WITH_LOCKED_NODE(                     \
    version_ptr, expected, version_ptr_locked)                                 \
  {                                                                            \
    uint64_t exp_copy = expected;                                              \
    if (!__atomic_compare_exchange_n(                                          \
            version_ptr, &exp_copy, Lock::setLockedBit(exp_copy),              \
            false /* weak */, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {           \
      Lock::writeUnlock(version_ptr_locked);                                   \
      RESTART                                                                  \
    }                                                                          \
  }

#define CHECK_OR_RESTART(version_ptr, expected)                                \
  READ_UNLOCK_OR_RESTART(version_ptr, expected)
