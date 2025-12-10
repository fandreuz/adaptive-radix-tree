#include "nodes.hpp"
#include "utils.hpp"
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

template <Type NT> Header* makeNewNode() {
  // O1+ will inline the call to nodeSize to a constant after
  // templating
  size_t node_size = nodeSize(NT);

  Header* header = (Header*)malloc(sizeof(Header) + node_size);
  header->type = NT;
  header->prefix_len = 0;
  header->prefix = nullptr;
  header->version = 0;

  if (NT == Type::NODE48) {
    memset(header->getNode(), Node48::EMPTY, 256);
    memset(((uint8_t*)header->getNode()) + 256, 0, sizeof(Node48) - 256);
  } else {
    memset(header->getNode(), 0, node_size);
  }

  return header;
}

template Header* makeNewNode<Type::NODE4>();
template Header* makeNewNode<Type::NODE16>();
template Header* makeNewNode<Type::NODE48>();
template Header* makeNewNode<Type::NODE256>();

Header* makeNewRoot() { return makeNewNode<Type::NODE256>(); }

Leaf* makeNewLeaf(KEY, Value value) {
  Leaf* leaf = (Leaf*)malloc(sizeof(Leaf) + key_len);
  assert((((uintptr_t)leaf) & 1) == 0);

  memcpy(getKey(leaf), key, key_len);
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
    new_header = makeNewNode<Type::NODE16>();
    auto new_node = (Node16*)new_header->getNode();
    memcpy(new_node->keys, node->keys, (*node_header)->children_count);
    memcpy(new_node->children, node->children,
           (*node_header)->children_count * sizeof(void*));
  } else if ((*node_header)->type == Type::NODE16) {
    auto node = (Node16*)(*node_header)->getNode();
    new_header = makeNewNode<Type::NODE48>();
    auto new_node = (Node48*)new_header->getNode();
    uint8_t child_index = 0;
    for (uint8_t i = 0; child_index < (*node_header)->children_count; ++i) {
      if (node->children[i] != nullptr) {
        new_node->child_index[node->keys[i]] = child_index;
        new_node->children[child_index++] = node->children[i];
      }
    }
  } else if ((*node_header)->type == Type::NODE48) {
    auto node = (Node48*)(*node_header)->getNode();
    new_header = makeNewNode<Type::NODE256>();
    auto new_node = (Node256*)new_header->getNode();
    uint8_t found = 0;
    for (uint8_t i = 0; found == (*node_header)->children_count; ++i) {
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

void addChildNode16(Header* node_header, KEY, Value value, size_t depth) {
  assert(node_header->type == Type::NODE16);

  auto node = (Node16*)node_header->getNode();
  __m128i key_vec = _mm_set1_epi8(key[depth]);
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
  node->keys[index] = key[depth];
  node->children[index] = smuggleLeaf(makeNewLeaf(key, key_len, value));
}

void addChild(Header* node_header, KEY, Value value, size_t depth) {
  assert(!isFull(node_header));

  if (node_header->type == Type::NODE4) {
    auto node = (Node4*)node_header->getNode();
    uint8_t i;
    for (i = 0; i < node_header->children_count && node->keys[i] < key[depth];
         ++i)
      ;

    shiftRight(node->keys, node->children, node_header->children_count, i);
    node->keys[i] = key[depth];
    node->children[i] = smuggleLeaf(makeNewLeaf(key, key_len, value));
  } else if (node_header->type == Type::NODE16) {
    addChildNode16(node_header, KARGS, value, depth);
  } else if (node_header->type == Type::NODE48) {
    auto node = (Node48*)node_header->getNode();
    assert(node->child_index[(uint8_t)key[depth]] == Node48::EMPTY);
    node->child_index[(uint8_t)key[depth]] = node_header->children_count;
    node->children[node_header->children_count] =
        smuggleLeaf(makeNewLeaf(key, key_len, value));
  } else if (node_header->type == Type::NODE256) {
    auto node = (Node256*)node_header->getNode();
    assert(node->children[(uint8_t)key[depth]] == nullptr);
    node->children[(uint8_t)key[depth]] =
        smuggleLeaf(makeNewLeaf(key, key_len, value));
  } else {
    ShouldNotReachHere;
  }

  ++(node_header->children_count);
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

} // namespace Nodes
