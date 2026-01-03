#ifndef NODES
#define NODES

#include "utils.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>

#define KEY const uint8_t *key, size_t key_len
#define KARGS key, key_len

#define PREFIX_SIZE 8

namespace Nodes {

typedef long Value;
typedef uint32_t prefix_size_t;
typedef uint64_t version_t;

struct Leaf;
bool isLeaf(const void* ptr);

inline size_t capPrefixSize(size_t prefix_size) {
  return std::min(prefix_size, (size_t)PREFIX_SIZE);
}

enum class Type : uint8_t { NODE4, NODE16, NODE48, NODE256 };

// When a node is allocated, the memory allocated shall have the
// following structure:
// Header -- Prefix[PREFIX_SIZE] -- NodeX -- [End child ptr]
struct Header {
  // Compressed prefix length. Real prefix length in Header::prefix
  // is capped at PREFIX_SIZE.
  prefix_size_t prefix_len;
  Type type;
  uint8_t children_count;
  // Value of the minimum key bit currently stored in this node.
  // Valid only for Node48 and Node256
  uint8_t min_key;

  uint8_t* getPrefix();
  void* getNode();
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
  static constexpr uint8_t CHILDREN_COUNT = 48;
  static constexpr uint8_t EMPTY =
      CHILDREN_COUNT; // this index will never be used

  uint8_t child_index[256];
  void* children[CHILDREN_COUNT];
};

struct Node256 {
  void* children[256];
};

bool isFull(const Header* node_header);

inline Header* asHeader(const void* ptr) {
  assert(!isLeaf(ptr));
  return (Nodes::Header*)ptr;
}

// When a Leaf is allocated, the memory allocated shall always be enough
// to accomodate the whole key, immediately after the last field in the
// struct.
struct Leaf {
  size_t key_len;
  Value value;
};

inline uint8_t* getKey(Leaf* leaf) { return (uint8_t*)(leaf + 1); }

inline bool isLeaf(const void* ptr) { return (((uintptr_t)ptr) & 1) == 1; }

inline Leaf* asLeaf(const void* ptr) {
  assert(isLeaf(ptr));
  return (Leaf*)((uintptr_t)ptr - 1);
}

inline void* smuggleLeaf(const Leaf* leaf) {
  assert((((uintptr_t)leaf) & 1) == 0); // should be at least 2-aligned
  return (void*)(((uintptr_t)leaf) + 1);
}

} // namespace Nodes

using namespace Nodes;
class Tree {

private:
  Nodes::Header* root;

  template <Type NT, bool END_CHILD> Header* makeNewNode();
  Header* makeNewRoot();
  Leaf* makeNewLeaf(KEY, Value value);

  void freeNode(void* node);
  void freeRecursive(Header* node_header);

  void grow(Header** node_header);

  void addChild(Header* node_header, KEY, Value value, size_t depth);
  void addChild(Header* node_header, uint8_t key, void* child);
  void addChildKeyEnd(Header* node_header, KEY, Value value);
  void addChildKeyEnd(Header* node_header, Leaf* child);

  void** findChild(Nodes::Header* node_header, uint8_t key) const;
  Leaf** findChildKeyEnd(Header* node_header) const;

  void findMinimumKey(const void* node, const uint8_t*& out_key,
                      size_t& out_len) const;
  void* splitLeafPrefix(Leaf* old_leaf, KEY, Value value, size_t depth);
  bool prefixMatches(Header* node_header, KEY, size_t depth, size_t& first_diff,
                     const uint8_t*& min_key, size_t& min_key_len);

public:
  Tree() : root(makeNewRoot()) {}
  ~Tree() { freeRecursive(root); }

  void findMinimumKey(const uint8_t*& out_key, size_t& out_len) const {
    findMinimumKey(root, out_key, out_len);
  }

  const Value* searchImpl(KEY);
  const Value* search(KEY) { return searchImpl(KARGS); }
  inline const Value* search(const char* key) {
    size_t len = strlen(key) + 1;
    return search((const uint8_t*)key, len);
  }

  void insertImpl(KEY, Value value);
  inline void insert(KEY, Value value) {
    assert(key_len > 0);
    insertImpl(KARGS, value);
  }
  inline void insert(const char* key, Value value) {
    size_t len = strlen(key) + 1;
    insert((const uint8_t*)key, len, value);
  }
};

#endif // NODES
