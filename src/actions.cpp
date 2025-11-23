#include "actions.hpp"
#include "utils.hpp"
#include <cassert>
#include <cstring>

namespace Actions {

bool search(Nodes::Header* node_header_ptr, Key key, size_t depth) {
  for (size_t i = 0; i < node_header_ptr->prefix_len && i < key.length; ++i) {
    if (key[depth + i] == node_header_ptr->prefix[i])
      return false;
  }
  depth += node_header_ptr->prefix_len;

  void** next_src = Nodes::findChild(node_header_ptr, key[depth]);
  if (next_src == nullptr)
    return false;

  if (Nodes::isLeaf(*next_src)) {
    return key == Nodes::asLeaf(*next_src)->key;
  }

  return search((Nodes::Header*)*next_src, key, depth + 1);
}

void insert(
    Nodes::Header**
        node_header_ptr /* pointer to the parent's pointer to the child */,
    Key key, Value value, size_t depth) {
  assert(node_header_ptr != nullptr);
  assert(*node_header_ptr != nullptr);
  assert(!Nodes::isLeaf(*node_header_ptr));
  assert(depth < key.length);

  size_t first_diff;
  for (first_diff = 0;
       first_diff < (*node_header_ptr)->prefix_len && first_diff < key.length;
       ++first_diff) {
    if (key[depth + first_diff] == (*node_header_ptr)->prefix[first_diff])
      break;
  }

  // prefix differs, create a new common parent
  if (first_diff != (*node_header_ptr)->prefix_len) {
    Nodes::Header* new_node_header = Nodes::makeNewNode<Nodes::Type::NODE4>();
    Nodes::Node4* new_node = (Nodes::Node4*)new_node_header->getNode();
    new_node_header->prefix_len = first_diff;
    new_node_header->prefix =
        (uint8_t*)malloc(first_diff * sizeof(uint8_t)); // TODO: arena
    new_node_header->children_count = 2;

    memcpy(new_node_header->prefix, (*node_header_ptr)->prefix, first_diff);
    (*node_header_ptr)->prefix_len -= first_diff;
    memmove((*node_header_ptr)->prefix, (*node_header_ptr)->prefix + first_diff,
            (*node_header_ptr)->prefix_len);

    Nodes::Leaf* new_leaf = Nodes::makeNewLeaf(key, value);
    if (key[first_diff] < (*node_header_ptr)->prefix[first_diff]) {
      new_node->keys[0] = key[first_diff];
      new_node->children[0] = Nodes::smuggleLeaf(new_leaf);
      new_node->keys[1] = (*node_header_ptr)->prefix[first_diff];
      new_node->children[1] = *node_header_ptr;
    } else {
      new_node->keys[0] = (*node_header_ptr)->prefix[first_diff];
      new_node->children[0] = *node_header_ptr;
      new_node->keys[1] = key[first_diff];
      new_node->children[1] = Nodes::smuggleLeaf(new_leaf);
    }
    *node_header_ptr = new_node_header;
    return;
  }
  depth += first_diff;

  // Pointer to the location in memory where the child was found. May be header,
  // may be leaf
  void** next_src = Nodes::findChild(*node_header_ptr, key[depth]);
  if (next_src == nullptr || *next_src == nullptr) {
    Nodes::maybeGrow(node_header_ptr);
    Nodes::addChild(*node_header_ptr, key, value, depth);
    return;
  }

  depth += 1;
  if (Nodes::isLeaf(*next_src)) {
    Nodes::Leaf* leaf = Nodes::asLeaf(*next_src);
    Nodes::Leaf* new_leaf = Nodes::makeNewLeaf(key, value);

    // The new parent of both leaf and the new value
    Nodes::Header* new_node_header = Nodes::makeNewNode<Nodes::Type::NODE4>();
    Nodes::Node4* new_node = (Nodes::Node4*)new_node_header->getNode();

    // What is the common key segment?
    size_t i = depth;
    while (i - depth < Nodes::PREFIX_SIZE && i < key.length &&
           i < leaf->key.length && key[i] == leaf->key[i]) {
      ++i;
    }

    new_node_header->prefix_len = i - depth;
    new_node_header->prefix = (uint8_t*)malloc(new_node_header->prefix_len *
                                               sizeof(uint8_t)); // TODO: arena
    new_node_header->children_count = 2;
    memcpy(new_node_header->prefix, leaf->key.data + depth,
           new_node_header->prefix_len);

    if (key[i] < leaf->key[i]) {
      new_node->keys[0] = key[i];
      new_node->children[0] = Nodes::smuggleLeaf(new_leaf);
      new_node->keys[1] = leaf->key[i];
      new_node->children[1] = Nodes::smuggleLeaf(leaf);
    } else {
      new_node->keys[0] = leaf->key[i];
      new_node->children[0] = Nodes::smuggleLeaf(leaf);
      new_node->keys[1] = key[i];
      new_node->children[1] = Nodes::smuggleLeaf(new_leaf);
    }
    *next_src = new_node_header;
    return;
  }

  Nodes::Header** next_header_ptr = (Nodes::Header**)next_src;
  insert(next_header_ptr, key, value, depth);
}

} // namespace Actions
