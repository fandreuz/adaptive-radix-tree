#include "nodes.hpp"
#include "utils.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>

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

constexpr size_t nodeSize(Type nt) {
#define SIZEOF_ACTION(N) return sizeof(Node##N)
  DISPATCH_CHILDREN_COUNT(SIZEOF_ACTION, nt)
  return 0;
}

template <Type NT> Header* makeNewNode() {
  Header* header = (Header*)malloc(sizeof(Header) + nodeSize(NT));
  header->type = NT;
  header->prefix_len = 0;
  header->prefix = nullptr;

  if (NT == Type::NODE48) {
    memset(header->getNode(), Node48::EMPTY, 256);
    memset(((uint8_t*)header->getNode()) + 256, 0, sizeof(Node48) - 256);
  } else {
    memset(header->getNode(), 0, nodeSize(NT));
  }

  return header;
}

template Header* makeNewNode<Type::NODE4>();
template Header* makeNewNode<Type::NODE16>();
template Header* makeNewNode<Type::NODE48>();
template Header* makeNewNode<Type::NODE256>();

Leaf* makeNewLeaf(KEY, Value value) {
  Leaf* leaf = (Leaf*)malloc(sizeof(Leaf));

  uint8_t* buf = (uint8_t*)malloc(key_len);
  memcpy(buf, key, key_len);
  leaf->key = buf;

  leaf->key_len = key_len;
  leaf->value = value;
  return leaf;
}

bool isFull(const Header* node_header) {
#define ISFULL_ACTION(N) return node_header->children_count == N
  DISPATCH_CHILDREN_COUNT(ISFULL_ACTION, node_header->type)
  return false;
}

void maybeGrow(Header** node_header) {
  if (!isFull(*node_header))
    return;

  Header* new_header;
  if ((*node_header)->type == Type::NODE4) {
    Node4* node = (Node4*)(*node_header)->getNode();

    new_header = makeNewNode<Type::NODE16>();
    Node16* new_node = (Node16*)new_header->getNode();
    for (uint8_t i = 0; i < (*node_header)->children_count; ++i) {
      if (node->children[i] != nullptr) {
        new_node->keys[i] = node->keys[i];
        new_node->children[i] = node->children[i];
      }
    }
  } else if ((*node_header)->type == Type::NODE16) {
    Node16* node = (Node16*)(*node_header)->getNode();

    new_header = makeNewNode<Type::NODE48>();
    Node48* new_node = (Node48*)new_header->getNode();

    uint8_t child_index = 0;
    for (uint8_t i = 0; i < (*node_header)->children_count; ++i) {
      if (node->children[i] != nullptr) {
        new_node->child_index[node->keys[i]] = child_index;
        new_node->children[child_index++] = node->children[i];

        if (child_index == (*node_header)->children_count)
          break;
      }
    }
  } else if ((*node_header)->type == Type::NODE48) {
    Node48* node = (Node48*)(*node_header)->getNode();

    new_header = makeNewNode<Type::NODE256>();
    Node256* new_node = (Node256*)new_header->getNode();

    uint8_t child_index = 0;
    for (uint8_t i = 0; i < 256; ++i) {
      if (node->child_index[i] != Node48::EMPTY) {
        new_node->children[i] = node->children[node->child_index[i]];

        ++child_index;
        if (child_index == (*node_header)->children_count)
          break;
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

// TODO: Improve
void addChildSmallNode(uint8_t* keys, void** children, uint8_t count, KEY,
                       Value value, size_t depth) {
  uint8_t i;
  for (i = 0; i < count && keys[i] < key[depth]; ++i)
    ;

  size_t shift_count = count - i;
  if (shift_count > 0) {
    memmove(keys + i + 1, keys + i, shift_count);
    memmove(children + i + 1, children + i, shift_count * sizeof(void**));
  }

  keys[i] = key[depth];
  children[i] = smuggleLeaf(makeNewLeaf(key, key_len, value));
}

void addChild(Header* node_header, KEY, Value value, size_t depth) {
  assert(!isFull(node_header));

  if (node_header->type == Type::NODE4) {
    Node4* node = (Node4*)node_header->getNode();
    addChildSmallNode(node->keys, node->children, node_header->children_count,
                      KARGS, value, depth);
  } else if (node_header->type == Type::NODE16) {
    Node16* node = (Node16*)node_header->getNode();
    addChildSmallNode(node->keys, node->children, node_header->children_count,
                      KARGS, value, depth);
  } else if (node_header->type == Type::NODE48) {
    Node48* node = (Node48*)node_header->getNode();
    assert(node->child_index[(uint8_t)key[depth]] == Node48::EMPTY);
    node->child_index[(uint8_t)key[depth]] = node_header->children_count;
    node->children[node_header->children_count] =
        smuggleLeaf(makeNewLeaf(key, key_len, value));
  } else if (node_header->type == Type::NODE256) {
    Node256* node = (Node256*)node_header->getNode();
    assert(node->children[(uint8_t)key[depth]] == nullptr);
    node->children[(uint8_t)key[depth]] =
        smuggleLeaf(makeNewLeaf(key, key_len, value));
  } else {
    ShouldNotReachHere;
  }

  ++(node_header->children_count);
}

// TODO: Improve
void** findChildSmallNode(const uint8_t* keys, void** children, uint8_t key,
                          uint8_t count) {
  for (uint8_t i = 0; i < count; ++i) {
    if (keys[i] == key) {
      return &(children[i]);
    }
  }
  return nullptr;
}

void** findChild(const Header* node_header, uint8_t key) {
  if (node_header->type == Type::NODE4) {
    auto node = (Node4*)node_header->getNode();
    return findChildSmallNode(node->keys, node->children, key,
                              node_header->children_count);
  } else if (node_header->type == Type::NODE16) {
    auto node = (Node16*)node_header->getNode();
    return findChildSmallNode(node->keys, node->children, key,
                              node_header->children_count);
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
