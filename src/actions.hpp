#ifndef ACTIONS
#define ACTIONS

#include "nodes.hpp"
#include <string>

namespace Actions {

bool search(Nodes::Header* node_header, KEY, size_t depth);
void insert(Nodes::Header** node_header, KEY, Value value, size_t depth = 0);
inline void insert(Nodes::Header** node_header, const char* key, Value value,
                   size_t depth = 0) {
  size_t len = strlen(key) + 1;
  insert(node_header, (const uint8_t*)key, len, value, depth);
}

} // namespace Actions

#endif // ACTIONS
