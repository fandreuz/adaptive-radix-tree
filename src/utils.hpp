#ifndef UTILS
#define UTILS

#include <cassert>
#include <cstddef>

#define ShouldNotReachHere assert(false)

inline size_t min(size_t a, size_t b) { return a < b ? a : b; }

#endif
