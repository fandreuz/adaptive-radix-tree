#ifndef NODES
#define NODES

#include "utils.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#define KEY const uint8_t *key, size_t key_len
#define KARGS key, key_len
typedef long Value;

namespace Nodes {

const size_t PREFIX_SIZE = 8;

inline size_t cap_prefix_size(size_t prefix_size) {
  return min(prefix_size, PREFIX_SIZE);
}

enum class Type : uint8_t { NODE4, NODE16, NODE48, NODE256 };

struct Header {
  Type type;
  size_t prefix_len;
  uint8_t* prefix;
  uint8_t children_count;
  size_t version;

  void* getNode() const;
};

struct Leaf {
  const uint8_t* key;
  size_t key_len;
  Value value;
};

struct Node4 {
  uint8_t keys[4];
  void* children[4];
};

struct Node16 {
  uint8_t keys[16];
  void* children[16];
};

struct Node48 {
  static const uint8_t EMPTY = 48;

  uint8_t child_index[256];
  void* children[48];
};

struct Node256 {
  void* children[256];
};

template <Type NT> Header* makeNewNode();
Header* makeNewRoot();

bool isFull(const Header* node_header);
void grow(Header** node_header);

void addChild(Header* node_header, KEY, Value value, size_t depth);
void** findChild(Nodes::Header* node_header, uint8_t key);

inline bool isLeaf(const void* ptr) { return (((uintptr_t)ptr) & 1) == 1; }

inline Header* asHeader(const void* ptr) {
  assert(!isLeaf(ptr));
  return (Nodes::Header*)ptr;
}

inline Leaf* asLeaf(const void* ptr) {
  assert(isLeaf(ptr));
  return (Leaf*)((uintptr_t)ptr - 1);
}

inline void* smuggleLeaf(const Leaf* leaf) {
  assert((((uintptr_t)leaf) & 1) == 0); // should be at least 2-aligned
  return (void*)(((uintptr_t)leaf) + 1);
}

Leaf* makeNewLeaf(KEY, Value value);

} // namespace Nodes

#endif // NODES
