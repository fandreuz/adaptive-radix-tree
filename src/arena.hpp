#include <cstring>

template <size_t S> class Arena {
public:
  void* allocate();
  void free(void* addr);
};
