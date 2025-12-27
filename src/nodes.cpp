#include "nodes.hpp"
#include "utils.hpp"
#include <algorithm>
#include <emmintrin.h>

namespace Nodes {

void* Header::getNode() const {
  return (void*)(((uintptr_t)this) + sizeof(Header));
}

#define DISPATCH_CHILDREN_COUNT(action, nt)                                    \
  if (nt == Type::NODE4)                                                       \
    action(4);                                                                 \
  if (nt == Type::NODE16)                                                      \
    action(16);                                                                \
  if (nt == Type::NODE48)                                                      \
    action(48);                                                                \
  if (nt == Type::NODE256)                                                     \
    action(256);                                                               \
  ShouldNotReachHere;

size_t nodeSize(Type nt) {
#define SIZEOF_ACTION(N) return sizeof(Node##N)
  DISPATCH_CHILDREN_COUNT(SIZEOF_ACTION, nt)
  return 0;
}

template <Type NT, bool END_CHILD> Header* makeNewNode() {
  // O1+ will inline the call to nodeSize to a constant after
  // templating
  size_t node_size = nodeSize(NT);

  const size_t end_child_size = END_CHILD ? sizeof(void*) : 0;
  Header* header = (Header*)malloc(sizeof(Header) + node_size + end_child_size);
  header->type = NT;
  header->prefix_len = 0;
  header->prefix = nullptr;
  header->version = 0;
  header->min_key = 255;

  if (NT == Type::NODE48) {
    memset(header->getNode(), Node48::EMPTY, 256);
    memset(((uint8_t*)header->getNode()) + 256, 0, node_size - 256);
  } else {
    memset(header->getNode(), 0, node_size);
  }

  return header;
}

template Header* makeNewNode<Type::NODE4, true>();
template Header* makeNewNode<Type::NODE16, true>();
template Header* makeNewNode<Type::NODE48, true>();
template Header* makeNewNode<Type::NODE256, true>();
// Only the root node needs to be end-child-less
template Header* makeNewNode<Type::NODE256, false>();

Header* makeNewRoot() { return makeNewNode<Type::NODE256, true>(); }

Leaf* makeNewLeaf(KEY, Value value) {
  Leaf* leaf = (Leaf*)malloc(sizeof(Leaf) + key_len);
  assert((((uintptr_t)leaf) & 1) == 0);

  memcpy(getKey(leaf), KARGS);
  leaf->key_len = key_len;
  leaf->value = value;
  return leaf;
}

bool isFull(const Header* node_header) {
#define ISFULL_ACTION(N) return node_header->children_count == N
  DISPATCH_CHILDREN_COUNT(ISFULL_ACTION, node_header->type)
  return false;
}

void grow(Header** node_header) {
  assert(isFull(*node_header));

  Header* new_header;
  if ((*node_header)->type == Type::NODE4) {
    auto node = (Node4*)(*node_header)->getNode();
    new_header = makeNewNode<Type::NODE16, true>();
    auto new_node = (Node16*)new_header->getNode();
    memcpy(new_node->keys, node->keys, 4);
    memcpy(new_node->children, node->children, 4 * sizeof(void*));
  } else if ((*node_header)->type == Type::NODE16) {
    auto node = (Node16*)(*node_header)->getNode();
    new_header = makeNewNode<Type::NODE48, true>();
    auto new_node = (Node48*)new_header->getNode();
    new_header->min_key = node->keys[0];
    for (uint8_t i = 0; i < 16; ++i) {
      new_node->child_index[node->keys[i]] = i;
      new_node->children[i] = node->children[i];
    }
  } else if ((*node_header)->type == Type::NODE48) {
    auto node = (Node48*)(*node_header)->getNode();
    new_header = makeNewNode<Type::NODE256, true>();
    auto new_node = (Node256*)new_header->getNode();
    new_header->min_key = (*node_header)->min_key;
    uint8_t found = 0;
    for (uint8_t i = 0; found < 48; ++i) {
      if (node->child_index[i] != Node48::EMPTY) {
        new_node->children[i] = node->children[node->child_index[i]];
        ++found;
      }
    }
  } else {
    // Node256 can't and should not need to be grown, as it can
    // hold all key bits at once.
    ShouldNotReachHere;
    return;
  }

  new_header->children_count = (*node_header)->children_count;
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

void addChild(Header* node_header, KEY, Value value, size_t depth) {
  addChild(node_header, key[depth], smuggleLeaf(makeNewLeaf(KARGS, value)));
}

void addChild(Header* node_header, uint8_t key, void* child) {
  assert(!isFull(node_header));

  if (node_header->type == Type::NODE4) {
    auto node = (Node4*)node_header->getNode();
    uint8_t i;
    for (i = 0; i < node_header->children_count && node->keys[i] < key; ++i)
      ;

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

void addChildKeyEnd(Header* node_header, KEY, Value value) {
  addChildKeyEnd(node_header, makeNewLeaf(KARGS, value));
}

void addChildKeyEnd(Header* node_header, Leaf* child) {
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

void** findChild(Header* node_header, uint8_t key) {
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

Leaf** findChildKeyEnd(Header* node_header) {
  size_t node_size = nodeSize(node_header->type);
  void* node = node_header->getNode();
  void* key_end_child = (uint8_t*)node + node_size;
  return (Leaf**)key_end_child;
}

} // namespace Nodes
