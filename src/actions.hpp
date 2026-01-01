#ifndef ACTIONS
#define ACTIONS

#include "nodes.hpp"
#include <cstdlib>

namespace Actions {

void findMinimumKey(const void* node, const uint8_t*& out_key, size_t& out_len);

const Nodes::Value* search(Nodes::Header* node_header, KEY);

inline const Nodes::Value* search(Nodes::Header* node_header, const char* key) {
  size_t len = strlen(key) + 1;
  return search(node_header, (const uint8_t*)key, len);
}

void insert(Nodes::Header* root, KEY, Nodes::Value value);

inline void insert(Nodes::Header* root, const char* key, Nodes::Value value) {
  size_t len = strlen(key) + 1;
  insert(root, (const uint8_t*)key, len, value);
}

} // namespace Actions

#endif // ACTIONS
