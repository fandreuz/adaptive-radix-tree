#include "actions.hpp"
#include "utils.hpp"
#include <cassert>
#include <cstring>

size_t min(size_t a, size_t b) { return a < b ? a : b; }

namespace Actions {

const Value* searchImpl(Nodes::Header* node_header_ptr, KEY /* key, key_len */,
                        size_t depth, bool needs_full_key_check) {
  assert(node_header_ptr != nullptr);
  assert(!Nodes::isLeaf(node_header_ptr));
  assert(depth < key_len);

  if (key_len < depth + node_header_ptr->prefix_len + 1)
    return nullptr;

  size_t stop = min(Nodes::PREFIX_SIZE, node_header_ptr->prefix_len);
  for (size_t i = 0; i < stop; ++i) {
    if (key[depth + i] != node_header_ptr->prefix[i])
      return nullptr;
  }
  // did we skip something? then we need a full cmp later
  needs_full_key_check |= node_header_ptr->prefix_len > stop;
  depth += node_header_ptr->prefix_len;

  void** next_src = Nodes::findChild(node_header_ptr, key[depth]);
  if (next_src == nullptr)
    return nullptr;
  assert(*next_src != nullptr);
  ++depth;

  if (Nodes::isLeaf(*next_src)) {
    auto leaf = Nodes::asLeaf(*next_src);
    if (key_len == leaf->key_len &&
        (!needs_full_key_check || memcmp(leaf->key, KARGS) == 0)) {
      return &leaf->value;
    }
    return nullptr;
  }

  auto next_header = Nodes::asHeader(*next_src);
  return searchImpl(next_header, KARGS, depth, needs_full_key_check);
}

const Value* search(Nodes::Header* node_header_ptr, KEY /* key, key_len */) {
  return searchImpl(node_header_ptr, KARGS, 0 /* depth */,
                    false /* needs_full_key_check */);
}

void insertInOrder(Nodes::Node4* new_node, uint8_t k1, uint8_t k2, void* v1,
                   void* v2) {
  assert(new_node->keys[0] == 0);
  assert(new_node->keys[1] == 0);
  assert(new_node->children[0] == nullptr);
  assert(new_node->children[1] == nullptr);
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

void insertImpl(
    Nodes::Header**
        node_header_ptr /* pointer to the parent's pointer to the child */,
    KEY /* key, key_len */, Value value, size_t depth) {
  assert(node_header_ptr != nullptr);
  assert(*node_header_ptr != nullptr);
  assert(!Nodes::isLeaf(*node_header_ptr));
  assert(depth < key_len);

  size_t stop =
      min(min((*node_header_ptr)->prefix_len, key_len), Nodes::PREFIX_SIZE);
  size_t i;
  for (i = 0; i < stop && key[depth + i] == (*node_header_ptr)->prefix[i]; ++i)
    ;

  // prefix differs, create a new common parent
  if (i < (*node_header_ptr)->prefix_len) {
    Nodes::Header* new_node_header = Nodes::makeNewNode<Nodes::Type::NODE4>();
    Nodes::Node4* new_node = (Nodes::Node4*)new_node_header->getNode();
    new_node_header->prefix_len = i;
    new_node_header->prefix = (uint8_t*)malloc(i * sizeof(uint8_t));
    new_node_header->children_count = 2;

    memcpy(new_node_header->prefix, (*node_header_ptr)->prefix, i);
    (*node_header_ptr)->prefix_len -= i;
    memmove((*node_header_ptr)->prefix, (*node_header_ptr)->prefix + i,
            (*node_header_ptr)->prefix_len);

    Nodes::Leaf* new_leaf = Nodes::makeNewLeaf(key, key_len, value);
    insertInOrder(new_node, key[i], (*node_header_ptr)->prefix[i],
                  Nodes::smuggleLeaf(new_leaf), *node_header_ptr);
    *node_header_ptr = new_node_header;
    return;
  }
  depth += i;

  void** next_src = Nodes::findChild(*node_header_ptr, key[depth]);
  if (next_src == nullptr || *next_src == nullptr) {
    Nodes::maybeGrow(node_header_ptr);
    Nodes::addChild(*node_header_ptr, KARGS, value, depth);
    return;
  }

  depth += 1;
  if (Nodes::isLeaf(*next_src)) {
    Nodes::Leaf* leaf = Nodes::asLeaf(*next_src);
    Nodes::Leaf* new_leaf = Nodes::makeNewLeaf(key, key_len, value);

    // The new parent of both leaf and the new value
    Nodes::Header* new_node_header = Nodes::makeNewNode<Nodes::Type::NODE4>();
    Nodes::Node4* new_node = (Nodes::Node4*)new_node_header->getNode();

    // What is the common key segment?
    size_t i = depth;
    while (i < key_len && i < leaf->key_len && key[i] == leaf->key[i]) {
      ++i;
    }

    new_node_header->prefix_len = i - depth;
    size_t prefix_size = min(new_node_header->prefix_len, Nodes::PREFIX_SIZE);
    new_node_header->prefix = (uint8_t*)malloc(prefix_size * sizeof(uint8_t));
    new_node_header->children_count = 2;
    memcpy(new_node_header->prefix, leaf->key + depth,
           new_node_header->prefix_len);

    insertInOrder(new_node, key[i], leaf->key[i], Nodes::smuggleLeaf(new_leaf),
                  Nodes::smuggleLeaf(leaf));
    *next_src = new_node_header;
    return;
  }

  Nodes::Header** next_header_ptr = (Nodes::Header**)next_src;
  insertImpl(next_header_ptr, KARGS, value, depth);
}

void insert(
    Nodes::Header**
        node_header_ptr /* pointer to the parent's pointer to the child */,
    KEY /* key, key_len */, Value value) {
  assert(key[key_len - 1] == 0);
  insertImpl(node_header_ptr, KARGS, value, 0 /* depth */);
}

} // namespace Actions
