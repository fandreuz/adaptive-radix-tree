#include "arena.hpp"

template <size_t S> void* Arena<S>::allocate() { return nullptr; }

template <size_t S> void Arena<S>::free(void* address) {}
