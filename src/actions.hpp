#ifndef ACTIONS
#define ACTIONS

#include "nodes.hpp"
#include <string>

namespace Actions {

const Value* search(Nodes::Header* node_header, KEY);

inline const Value* search(Nodes::Header* node_header, const char* key) {
  size_t len = strlen(key) + 1;
  return search(node_header, (const uint8_t*)key, len);
}

void insert(Nodes::Header** node_header, KEY, Value value);

inline void insert(Nodes::Header** node_header, const char* key, Value value) {
  size_t len = strlen(key) + 1;
  insert(node_header, (const uint8_t*)key, len, value);
}

} // namespace Actions

#endif // ACTIONS
