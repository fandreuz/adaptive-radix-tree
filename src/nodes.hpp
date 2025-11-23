#ifndef NODES
#define NODES

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

class Key {
public:
  size_t length;
  const char* data;

  Key(const char* s) {
    length = strlen(s) + 1;
    char* c = (char*)malloc(length);
    memcpy(c, s, length - 1);
    c[length - 1] = 0;
    data = (const char*)c;
  }

  char operator[](size_t idx) const { return data[idx]; }

  bool operator==(const Key& o) const {
    return o.length == length && memcmp(data, o.data, length) == 0;
  }

  bool operator==(const char* s) const {
    return strlen(s) + 1 == length && memcmp(data, s, length - 1) == 0;
  }

  friend std::ostream& operator<<(std::ostream& os, const Key& k) {
    os << k.data;
    return os;
  }
};

typedef uint64_t Value;

namespace Nodes {

const size_t PREFIX_SIZE = 8;

enum class Type : uint8_t { NODE4, NODE16, NODE48, NODE256 };

struct Header {
  Type type;
  size_t prefix_len;
  uint8_t* prefix;
  uint8_t children_count;

  void* getNode() const;
};

struct Leaf {
  Key key;
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

constexpr size_t nodeSize(Type nt);

// Check if we should grow the node before adding a new child
void maybeGrow(Header** node_header);

void addChild(Header* node_header, Key key, Value value, size_t depth);
void** findChild(Nodes::Header* node_header, uint8_t key);
void print(Header* node_header, std::ostream& os, size_t depth = 0);

inline bool isLeaf(void* ptr) { return (((uintptr_t)ptr) & 1) == 1; }

inline Leaf* asLeaf(void* ptr) {
  assert(isLeaf(ptr));
  return (Leaf*)((uintptr_t)ptr - 1);
}

inline void* smuggleLeaf(Leaf* leaf) {
  assert((((uintptr_t)leaf) & 1) == 0); // should be at least 2-aligned
  return (void*)(((uintptr_t)leaf) + 1);
}

Leaf* makeNewLeaf(Key key, Value value);

} // namespace Nodes

#endif // NODES
