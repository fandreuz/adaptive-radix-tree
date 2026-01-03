#include "nodes.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cassert>

using namespace Nodes;

void Tree::findMinimumKey(const void* node, const uint8_t*& out_key,
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
bool Tree::prefixMatches(Header* node_header, KEY, size_t depth,
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

const Value* Tree::searchImpl(KEY) {
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
void* Tree::splitLeafPrefix(Leaf* old_leaf, KEY, Value value, size_t depth) {
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
  Header* new_node_header = makeNewNode<Type::NODE4, true>();
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

void Tree::insertImpl(KEY, Value value) {
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
      Header* new_node_header = makeNewNode<Type::NODE4, true>();
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
        free(node_header);
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
