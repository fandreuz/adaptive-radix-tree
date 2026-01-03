#ifndef NODES
#define NODES

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <emmintrin.h>

#define ShouldNotReachHere assert(false)

#define KEY const uint8_t *key, size_t key_len
#define KARGS key, key_len

#define PREFIX_SIZE 8

namespace Nodes {

typedef long Value;
typedef uint32_t prefix_size_t;
typedef uint64_t version_t;

static size_t capPrefixSize(size_t prefix_size) {
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

  uint8_t* getPrefix() { return (uint8_t*)((uintptr_t)this) + sizeof(Header); }

  void* getNode() { return (void*)(getPrefix() + PREFIX_SIZE); }
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

static size_t nodeSize(Type t) {
  switch (t) {
  case Type::NODE4:
    return sizeof(Node4);
  case Type::NODE16:
    return sizeof(Node16);
  case Type::NODE48:
    return sizeof(Node48);
  case Type::NODE256:
    return sizeof(Node256);
  default:
    ShouldNotReachHere;
    return false;
  }
}

constexpr size_t fullNodeSize(size_t s) {
  return s + sizeof(Header) + PREFIX_SIZE + sizeof(void*);
}

static bool isFull(const Header* node_header) {
  if (node_header->type == Type::NODE256) {
    return false;
  }

  switch (node_header->type) {
  case Type::NODE4:
    return node_header->children_count == 4;
  case Type::NODE16:
    return node_header->children_count == 16;
  case Type::NODE48:
    return node_header->children_count == 48;
  default:
    ShouldNotReachHere;
    return false;
  }
}

static bool isLeaf(const void* ptr) { return (((uintptr_t)ptr) & 1) == 1; }

static Header* asHeader(const void* ptr) {
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

static uint8_t* getKey(Leaf* leaf) { return (uint8_t*)(leaf + 1); }

static Leaf* asLeaf(const void* ptr) {
  assert(isLeaf(ptr));
  return (Leaf*)((uintptr_t)ptr - 1);
}

static void* smuggleLeaf(const Leaf* leaf) {
  assert((((uintptr_t)leaf) & 1) == 0); // should be at least 2-aligned
  return (void*)(((uintptr_t)leaf) + 1);
}

} // namespace Nodes

using namespace Nodes;

template <typename Allocator> class Tree {

private:
  Nodes::Header* root;

  template <Type NT> Header* makeNewNode();
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
  void copyHeader(Header* new_header, Header* old_header);

public:
  Tree() : root(makeNewRoot()) {}
  ~Tree() { freeRecursive(root); }

  void findMinimumKey(const uint8_t*& out_key, size_t& out_len) const {
    findMinimumKey(root, out_key, out_len);
  }

  const Value* searchImpl(KEY);
  const Value* search(KEY) { return searchImpl(KARGS); }
  const Value* search(const char* key) {
    size_t len = strlen(key) + 1;
    return search((const uint8_t*)key, len);
  }

  void insertImpl(KEY, Value value);
  void insert(KEY, Value value) {
    assert(key_len > 0);
    insertImpl(KARGS, value);
  }
  void insert(const char* key, Value value) {
    size_t len = strlen(key) + 1;
    insert((const uint8_t*)key, len, value);
  }
};

template <typename Allocator, Type NT> Header* allocateNewNode() {
  switch (NT) {
  case Type::NODE4:
    return (
        Header*)(Allocator::template allocate<fullNodeSize(sizeof(Node4))>());
  case Type::NODE16:
    return (
        Header*)(Allocator::template allocate<fullNodeSize(sizeof(Node16))>());
  case Type::NODE48:
    return (
        Header*)(Allocator::template allocate<fullNodeSize(sizeof(Node48))>());
  case Type::NODE256:
    return (
        Header*)(Allocator::template allocate<fullNodeSize(sizeof(Node256))>());
  default:
    ShouldNotReachHere;
    return nullptr;
  }
}

template <typename Allocator>
template <Type NT>
Header* Tree<Allocator>::makeNewNode() {
  Header* header = allocateNewNode<Allocator, NT>();
  header->type = NT;
  header->prefix_len = 0;
  header->min_key = 255;
  header->children_count = 0;

  size_t memset_size =
      nodeSize(NT) + sizeof(void*); // nullify both the node and the end child
  if (NT == Type::NODE48) {
    memset(header->getNode(), Node48::EMPTY, 256);
    memset(((uint8_t*)header->getNode()) + 256, 0, memset_size - 256);
  } else {
    memset(header->getNode(), 0, memset_size);
  }

  return header;
}

template <typename Allocator> Header* Tree<Allocator>::makeNewRoot() {
  return makeNewNode<Type::NODE256>();
}

template <typename Allocator> void Tree<Allocator>::freeNode(void* node) {
  assert(node != nullptr);
  if (isLeaf(node)) {
    Allocator::releaseDynamic(asLeaf(node));
  } else {
    freeRecursive((Header*)node);
  }
}

template <typename Allocator>
void Tree<Allocator>::freeRecursive(Header* node_header) {
  Allocator::releaseDynamic(*findChildKeyEnd(node_header));

  if (node_header->type == Type::NODE4) {
    auto node = (Node4*)node_header->getNode();
    for (uint8_t i = 0; i < node_header->children_count; ++i) {
      freeNode(node->children[i]);
    }
    Allocator::template release<fullNodeSize(sizeof(Node4))>(node_header);
  } else if (node_header->type == Type::NODE16) {
    auto node = (Node16*)node_header->getNode();
    for (uint8_t i = 0; i < node_header->children_count; ++i) {
      freeNode(node->children[i]);
    }
    Allocator::template release<fullNodeSize(sizeof(Node16))>(node_header);
  } else if (node_header->type == Type::NODE48) {
    auto node = (Node48*)node_header->getNode();
    for (uint8_t i = 0; node_header->children_count > 0; ++i) {
      if (node->child_index[i] != Node48::EMPTY) {
        freeNode(node->children[node->child_index[i]]);
        --node_header->children_count;
      }
    }
    assert(node_header->children_count == 0);
    Allocator::template release<fullNodeSize(sizeof(Node48))>(node_header);
  } else if (node_header->type == Type::NODE256) {
    auto node = (Node256*)node_header->getNode();
    for (uint8_t i = 0; node_header->children_count > 0; ++i) {
      if (node->children[i] != nullptr) {
        freeNode(node->children[i]);
        --node_header->children_count;
      }
    }
    assert(node_header->children_count == 0);
    Allocator::template release<fullNodeSize(sizeof(Node48))>(node_header);
  } else {
    ShouldNotReachHere;
    return;
  }
}

template <typename Allocator>
Leaf* Tree<Allocator>::makeNewLeaf(KEY, Value value) {
  Leaf* leaf = (Leaf*)Allocator::allocateDynamic(sizeof(Leaf) + key_len);
  assert((((uintptr_t)leaf) & 1) == 0);

  memcpy(getKey(leaf), KARGS);
  leaf->key_len = key_len;
  leaf->value = value;
  return leaf;
}

template <typename Allocator>
void Tree<Allocator>::copyHeader(Header* new_header, Header* old_header) {
  new_header->children_count = old_header->children_count;
  new_header->min_key = old_header->min_key;
  new_header->prefix_len = old_header->prefix_len;
  memcpy(new_header->getPrefix(), old_header->getPrefix(),
         capPrefixSize(new_header->prefix_len));

  Leaf* child = *findChildKeyEnd(old_header);
  if (child != nullptr) {
    addChildKeyEnd(new_header, child);
  }
}

template <typename Allocator> void Tree<Allocator>::grow(Header** node_header) {
  assert(isFull(*node_header));

  Header* new_header;
  if ((*node_header)->type == Type::NODE4) {
    auto node = (Node4*)(*node_header)->getNode();
    new_header = makeNewNode<Type::NODE16>();
    copyHeader(new_header, *node_header);
    auto new_node = (Node16*)new_header->getNode();
    memcpy(new_node->keys, node->keys, 4);
    memcpy(new_node->children, node->children, 4 * sizeof(void*));
    Allocator::template release<fullNodeSize(sizeof(Node4))>(*node_header);
  } else if ((*node_header)->type == Type::NODE16) {
    auto node = (Node16*)(*node_header)->getNode();
    new_header = makeNewNode<Type::NODE48>();
    copyHeader(new_header, *node_header);
    auto new_node = (Node48*)new_header->getNode();
    new_header->min_key = node->keys[0];
    for (uint8_t i = 0; i < 16; ++i) {
      new_node->child_index[node->keys[i]] = i;
      new_node->children[i] = node->children[i];
    }
    Allocator::template release<fullNodeSize(sizeof(Node16))>(*node_header);
  } else if ((*node_header)->type == Type::NODE48) {
    auto node = (Node48*)(*node_header)->getNode();
    new_header = makeNewNode<Type::NODE256>();
    copyHeader(new_header, *node_header);
    auto new_node = (Node256*)new_header->getNode();
    new_header->min_key = (*node_header)->min_key;
    for (uint8_t i = 0; i < 48; ++i) {
      new_node->children[i] = node->children[node->child_index[i]];
    }
    Allocator::template release<fullNodeSize(sizeof(Node48))>(*node_header);
  } else {
    // Node256 can't and should not need to be grown, as it can
    // hold all key bits at once.
    ShouldNotReachHere;
    return;
  }

  *node_header = new_header;
  return;
}

// Shift right all elements after 'start' (inclusive)
void shiftRight(uint8_t* keys, void** children, size_t count, size_t start) {
  size_t shift_count = count - start;
  memmove(keys + start + 1, keys + start, shift_count);
  memmove(children + start + 1, children + start, shift_count * sizeof(void**));
}

void addChildNode16(Header* node_header, uint8_t key, void* child) {
  assert(node_header->type == Type::NODE16);

  auto node = (Node16*)node_header->getNode();
  __m128i key_vec = _mm_set1_epi8(key);
  __m128i cmp = _mm_cmpgt_epi8(key_vec, _mm_loadu_si128((__m128i*)node->keys));
  uint16_t mask = (1u << node_header->children_count) - 1;
  uint16_t bitfield = _mm_movemask_epi8(cmp) & mask;

  int index;
  if (bitfield) {
    index = __builtin_ctz(bitfield);
  } else {
    index = node_header->children_count;
  }

  shiftRight(node->keys, node->children, node_header->children_count, index);
  node->keys[index] = key;
  node->children[index] = child;
}

template <typename Allocator>
void Tree<Allocator>::addChild(Header* node_header, KEY, Value value,
                               size_t depth) {
  addChild(node_header, key[depth], smuggleLeaf(makeNewLeaf(KARGS, value)));
}

template <typename Allocator>
void Tree<Allocator>::addChild(Header* node_header, uint8_t key, void* child) {
  assert(!isFull(node_header));

  if (node_header->type == Type::NODE4) {
    auto node = (Node4*)node_header->getNode();
    uint8_t i;
    for (i = 0; i < node_header->children_count && node->keys[i] < key; ++i) {
      assert(i < 4);
    }

    shiftRight(node->keys, node->children, node_header->children_count, i);
    node->keys[i] = key;
    node->children[i] = child;
  } else if (node_header->type == Type::NODE16) {
    addChildNode16(node_header, key, child);
  } else if (node_header->type == Type::NODE48) {
    node_header->min_key = std::min(node_header->min_key, key);
    auto node = (Node48*)node_header->getNode();
    assert(node->child_index[key] == Node48::EMPTY);
    node->child_index[key] = node_header->children_count;
    node->children[node_header->children_count] = child;
  } else if (node_header->type == Type::NODE256) {
    node_header->min_key = std::min(node_header->min_key, key);
    auto node = (Node256*)node_header->getNode();
    assert(node->children[key] == nullptr);
    node->children[key] = child;
  } else {
    ShouldNotReachHere;
  }

  ++(node_header->children_count);
}

template <typename Allocator>
void Tree<Allocator>::addChildKeyEnd(Header* node_header, KEY, Value value) {
  addChildKeyEnd(node_header, makeNewLeaf(KARGS, value));
}

template <typename Allocator>
void Tree<Allocator>::addChildKeyEnd(Header* node_header, Leaf* child) {
  size_t node_size = nodeSize(node_header->type);
  void* node = node_header->getNode();
  void** key_end_child = (void**)((uint8_t*)node + node_size);
  *key_end_child = child;
}

void** findChildNode16(Header* node_header, uint8_t key) {
  assert(node_header->type == Type::NODE16);

  auto node = (Node16*)node_header->getNode();
  __m128i key_vec = _mm_set1_epi8(key);
  __m128i cmp = _mm_cmpeq_epi8(key_vec, _mm_loadu_si128((__m128i*)node->keys));
  uint16_t mask = (1u << node_header->children_count) - 1;
  uint16_t bitfield = _mm_movemask_epi8(cmp) & mask;
  return bitfield ? &(node->children[__builtin_ctz(bitfield)]) : nullptr;
}

template <typename Allocator>
void** Tree<Allocator>::findChild(Header* node_header, uint8_t key) const {
  if (node_header->type == Type::NODE4) {
    auto node = (Node4*)node_header->getNode();
    for (uint8_t i = 0; i < node_header->children_count; ++i) {
      if (node->keys[i] == key) {
        return &(node->children[i]);
      }
    }
    return nullptr;
  } else if (node_header->type == Type::NODE16) {
    return findChildNode16(node_header, key);
  } else if (node_header->type == Type::NODE48) {
    auto node = (Node48*)node_header->getNode();
    uint8_t child_index = node->child_index[key];
    if (child_index == Node48::EMPTY)
      return nullptr;
    return &(node->children[child_index]);
  } else if (node_header->type == Type::NODE256) {
    auto node = (Node256*)node_header->getNode();
    if (node->children[key] == nullptr)
      return nullptr;
    return &(node->children[key]);
  }

  ShouldNotReachHere;
  return nullptr;
}

template <typename Allocator>
Leaf** Tree<Allocator>::findChildKeyEnd(Header* node_header) const {
  size_t node_size = nodeSize(node_header->type);
  void* node = node_header->getNode();
  void* key_end_child = (uint8_t*)node + node_size;
  return (Leaf**)key_end_child;
}

template <typename Allocator>
void Tree<Allocator>::findMinimumKey(const void* node, const uint8_t*& out_key,
                                     size_t& out_len) const {
  while (true) {
    assert(node != nullptr);
    if (isLeaf(node)) {
      auto leaf = asLeaf(node);
      out_key = getKey(leaf);
      out_len = leaf->key_len;
      return;
    }

    auto header = asHeader(node);
    if (header->children_count == 0) {
      node = findChildKeyEnd(header);
      continue;
    }

    if (header->type == Type::NODE4) {
      auto next = (Node4*)header->getNode();
      node = next->children[0];
      continue;
    }
    if (header->type == Type::NODE16) {
      auto next = (Node16*)header->getNode();
      node = next->children[0];
      continue;
    }
    if (header->type == Type::NODE48) {
      auto next = (Node48*)header->getNode();
      node = next->children[next->child_index[header->min_key]];
      assert(node != nullptr);
      continue;
    }
    if (header->type == Type::NODE256) {
      auto next = (Node256*)header->getNode();
      node = next->children[header->min_key];
      assert(node != nullptr);
      continue;
    }
  }
}

// True only if a full match is found
template <typename Allocator>
bool Tree<Allocator>::prefixMatches(Header* node_header, KEY, size_t depth,
                                    size_t& first_diff, const uint8_t*& min_key,
                                    size_t& min_key_len) {
  min_key = nullptr;

  size_t i;
  {
    const size_t prefix_len = capPrefixSize(node_header->prefix_len);
    const size_t stop = std::min(prefix_len, key_len - depth);
    for (i = 0; i < stop; ++i) {
      if (key[i + depth] != node_header->getPrefix()[i]) {
        first_diff = i;
        return false;
      }
    }
  }
  // no difference found yet

  first_diff = i;
  if (i + depth == key_len) {
    // new key is exhausted
    return i == node_header->prefix_len;
  }
  if (i == node_header->prefix_len) {
    // node prefix is exhausted
    return true;
  }

  // some stuff is not in the prefix, we need to find a key
  findMinimumKey(node_header, min_key, min_key_len);

  const size_t stop = std::min(/* should not look farther than the prefix */
                               (size_t)node_header->prefix_len,
                               std::min(min_key_len, key_len) - depth);
  for (; i < stop; ++i) {
    if (key[i + depth] != min_key[i + depth]) {
      first_diff = i;
      return false;
    }
  }
  first_diff = i;
  return true;
}

template <typename Allocator> const Value* Tree<Allocator>::searchImpl(KEY) {
  Header* node_header = root;
  size_t depth = 0;

  while (true) {
    assert(node_header != nullptr);
    assert(!isLeaf(node_header));
    assert(depth < key_len);

    if (key_len < depth + node_header->prefix_len) {
      return nullptr;
    }

    {
      size_t first_diff;
      const uint8_t* min_key;
      size_t min_key_len;
      bool match = prefixMatches(node_header, KARGS, depth, first_diff, min_key,
                                 min_key_len);
      if (!match) {
        return nullptr;
      }
    }

    depth += node_header->prefix_len;
    assert(depth <= key_len);

    if (depth == key_len) {
      auto key_end_child = *findChildKeyEnd(node_header);
      if (key_end_child == nullptr) {
        return nullptr;
      }
      return &(key_end_child->value);
    }

    void** next_src = findChild(node_header, key[depth]);
    if (next_src == nullptr) {
      return nullptr;
    }

    assert(*next_src != nullptr);
    ++depth;

    if (isLeaf(*next_src)) {
      auto leaf = asLeaf(*next_src);
      bool match = key_len == leaf->key_len && memcmp(getKey(leaf), KARGS) == 0;
      return match ? &leaf->value : nullptr;
    } else if (depth == key_len) {
      auto key_end_child = *findChildKeyEnd(*((Header**)next_src));
      if (key_end_child == nullptr) {
        return nullptr;
      }
      return &key_end_child->value;
    }

    node_header = asHeader(*next_src);
  }
}

static void insertInOrder(Node4* new_node, uint8_t k1, uint8_t k2, void* v1,
                          void* v2) {
  assert(new_node->keys[0] == 0);
  assert(new_node->keys[1] == 0);
  assert(new_node->children[0] == nullptr);
  assert(new_node->children[1] == nullptr);
  assert(k1 != k2);
  if (k1 < k2) {
    new_node->keys[0] = k1;
    new_node->children[0] = v1;
    new_node->keys[1] = k2;
    new_node->children[1] = v2;
  } else {
    new_node->keys[0] = k2;
    new_node->children[0] = v2;
    new_node->keys[1] = k1;
    new_node->children[1] = v1;
  }
}

// Returns the new header
template <typename Allocator>
void* Tree<Allocator>::splitLeafPrefix(Leaf* old_leaf, KEY, Value value,
                                       size_t depth) {
  // What is the common key segment?
  size_t i = depth;
  const size_t stop = std::min(key_len, old_leaf->key_len);
  while (i < stop && key[i] == getKey(old_leaf)[i]) {
    ++i;
  }
  if (i == key_len && key_len == old_leaf->key_len) {
    old_leaf->value = value;
    return smuggleLeaf(old_leaf);
  }
  assert(i == key_len || i == old_leaf->key_len ||
         key[i] != getKey(old_leaf)[i]);

  // The new parent of both leaf and the new value
  Header* new_node_header = makeNewNode<Type::NODE4>();
  Node4* new_node = (Node4*)new_node_header->getNode();

  new_node_header->prefix_len = i - depth;
  size_t actual_prefix_size = capPrefixSize(new_node_header->prefix_len);
  memcpy(new_node_header->getPrefix(), getKey(old_leaf) + depth,
         actual_prefix_size);

  if (i == key_len) {
    addChildKeyEnd(new_node_header, KARGS, value);
    addChild(new_node_header, getKey(old_leaf)[i], smuggleLeaf(old_leaf));
    new_node_header->children_count = 1;
  } else if (i == old_leaf->key_len) {
    addChild(new_node_header, KARGS, value, i);
    addChildKeyEnd(new_node_header, old_leaf);
    new_node_header->children_count = 1;
  } else {
    Leaf* new_leaf = makeNewLeaf(KARGS, value);
    insertInOrder(new_node, key[i], getKey(old_leaf)[i], smuggleLeaf(new_leaf),
                  smuggleLeaf(old_leaf));
    new_node_header->children_count = 2;
  }
  return new_node_header;
}

template <typename Allocator>
void Tree<Allocator>::insertImpl(KEY, Value value) {
  Header** node_header_ptr;
  size_t depth;

  void** next_src = findChild(root, key[0]);
  if (next_src == nullptr || *next_src == nullptr) {
    assert(!isFull(root));
    addChild(root, KARGS, value, 0);
    return;
  }

  depth = 1;
  if (isLeaf(*next_src)) {
    *next_src = splitLeafPrefix(asLeaf(*next_src), KARGS, value, depth);
    return;
  }

  node_header_ptr = (Header**)next_src;

  while (true) {
    Header* node_header = *node_header_ptr;

    assert(node_header_ptr != nullptr);
    assert(node_header != nullptr);
    assert(!isLeaf(node_header));
    assert(depth < key_len);

    size_t first_diff;
    const uint8_t* min_key;
    size_t min_key_len;
    bool prefix_matches = prefixMatches(node_header, KARGS, depth, first_diff,
                                        min_key, min_key_len);
    depth += first_diff;
    if (!prefix_matches && depth < key_len) {
      Header* new_node_header = makeNewNode<Type::NODE4>();
      Node4* new_node = (Node4*)new_node_header->getNode();

      // handle new prefix: it will contain a prefix of the old prefix, the
      // common section with the new key
      new_node_header->prefix_len = first_diff;
      size_t actual_prefix_len = capPrefixSize(new_node_header->prefix_len);
      memcpy(new_node_header->getPrefix(), node_header->getPrefix(),
             actual_prefix_len);

      // shorten old prefix: it'll be a suffix of the old prefix.
      // +1 because an element of the prefix (the first diff) will
      // be part of the new parent.
      prefix_size_t old_prefix_len = node_header->prefix_len;
      node_header->prefix_len -= (1 + new_node_header->prefix_len);

      if (min_key == nullptr) {
        // How much materialized prefix do we have?
        prefix_size_t residual_prefix =
            capPrefixSize(old_prefix_len) - (first_diff + 1);
        if (capPrefixSize(node_header->prefix_len) > residual_prefix) {
          findMinimumKey(node_header, min_key, min_key_len);
        }
      }

      uint8_t diff_bit;
      if (min_key == nullptr) {
        diff_bit = node_header->getPrefix()[first_diff];
        memmove(node_header->getPrefix(),
                node_header->getPrefix() + first_diff + 1,
                capPrefixSize(node_header->prefix_len));
      } else {
        diff_bit = min_key[depth];
        memcpy(node_header->getPrefix(), min_key + depth + 1,
               capPrefixSize(node_header->prefix_len));
      }

      Leaf* new_leaf = makeNewLeaf(KARGS, value);
      insertInOrder(new_node, key[depth], diff_bit, smuggleLeaf(new_leaf),
                    node_header);
      new_node_header->children_count = 2;
      assert(*node_header_ptr != root);
      *node_header_ptr = new_node_header;

      return;
    }

    if (depth == key_len) {
      addChildKeyEnd(node_header, KARGS, value);
      return;
    }

    assert(depth < key_len);
    void** next_src = findChild(node_header, key[depth]);
    if (next_src == nullptr || *next_src == nullptr) {
      if (!isFull(node_header) || depth == key_len) {
        if (depth < key_len) {
          addChild(*node_header_ptr, KARGS, value, depth);
        } else {
          addChildKeyEnd(node_header, KARGS, value);
        }
      } else {
        assert(*node_header_ptr != root); // root should not need to be grown
        grow(node_header_ptr);
        addChild(*node_header_ptr, KARGS, value, depth);
        assert(*node_header_ptr != node_header);
      }
      return;
    }

    depth += 1;

    if (isLeaf(*next_src)) {
      *next_src = splitLeafPrefix(asLeaf(*next_src), KARGS, value, depth);
      return;
    } else if (depth == key_len) {
      addChildKeyEnd(*((Header**)next_src), KARGS, value);
      return;
    }

    node_header_ptr = (Header**)next_src;
  }
}

#endif // NODES
