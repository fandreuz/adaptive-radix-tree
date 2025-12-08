#include "actions.hpp"
#include "utils.hpp"
#include <cassert>
#include <cstring>

namespace Actions {

void findMinimumKey(const void* node, const uint8_t*& out_key,
                    size_t& out_len) {
  while (true) {
    if (Nodes::isLeaf(node)) {
      auto leaf = Nodes::asLeaf(node);
      out_key = leaf->key;
      out_len = leaf->key_len;
      return;
    }

    auto header = Nodes::asHeader(node);
    if (header->type == Nodes::Type::NODE4) {
      auto next = (Nodes::Node4*)header->getNode();
      node = next->children[0];
      continue;
    }
    if (header->type == Nodes::Type::NODE16) {
      auto next = (Nodes::Node16*)header->getNode();
      node = next->children[0];
      continue;
    }
    if (header->type == Nodes::Type::NODE48) {
      auto next = (Nodes::Node48*)header->getNode();
      for (uint8_t i = 0; i < 48; ++i) {
        if (next->children[i] != nullptr) {
          node = next->children[i];
          break;
        }
      }
      continue;
    }
    if (header->type == Nodes::Type::NODE256) {
      auto next = (Nodes::Node256*)header->getNode();
      for (uint8_t i = 0; i < 256; ++i) {
        if (next->children[i] != nullptr) {
          node = next->children[i];
          break;
        }
      }
      continue;
    }
  }
}

bool prefixMatches(const Nodes::Header* node_header, KEY, size_t depth,
                   size_t& first_diff, const uint8_t*& min_key,
                   size_t& min_key_len) {
  min_key = nullptr;

  size_t i;
  {
    const size_t prefix_len = Nodes::cap_prefix_size(node_header->prefix_len);
    const size_t stop = min(prefix_len, key_len - depth);
    for (i = 0; i < stop; ++i) {
      if (key[i + depth] != node_header->prefix[i]) {
        first_diff = i;
        return false;
      }
    }
  }
  // no difference found yet

  if (i + depth == key_len) {
    // we can stop now: the new key is exhausted
    return true;
  }
  if (i == node_header->prefix_len) {
    // node prefix is exhausted
    return true;
  }

  // some stuff is not in the prefix, we need to find a key
  findMinimumKey(node_header, min_key, min_key_len);

  i += depth;
  const size_t stop =
      min(/* should not look farther than the prefix */
          depth + node_header->prefix_len, min(min_key_len, key_len));
  for (; i < stop; ++i) {
    if (key[i] != min_key[i]) {
      first_diff = i - depth;
      return false;
    }
  }
  return true;
}

const Value* searchImpl(Nodes::Header* node_header_ptr, KEY, size_t depth) {
  while (true) {
    assert(node_header_ptr != nullptr);
    assert(!Nodes::isLeaf(node_header_ptr));
    assert(depth < key_len);

    if (key_len < depth + node_header_ptr->prefix_len + 1)
      return nullptr;

    size_t first_diff;
    const uint8_t* min_key;
    size_t min_key_len;

    {
      bool match = prefixMatches(node_header_ptr, key, key_len, depth,
                                 first_diff, min_key, min_key_len);
      if (!match)
        return nullptr;
    }

    depth += node_header_ptr->prefix_len;

    void** next_src = Nodes::findChild(node_header_ptr, key[depth]);
    if (next_src == nullptr)
      return nullptr;
    assert(*next_src != nullptr);
    ++depth;

    if (Nodes::isLeaf(*next_src)) {
      auto leaf = Nodes::asLeaf(*next_src);
      bool match = key_len == leaf->key_len && memcmp(leaf->key, KARGS) == 0;
      return match ? &leaf->value : nullptr;
    }

    node_header_ptr = Nodes::asHeader(*next_src);
  }
}

const Value* search(Nodes::Header* node_header_ptr, KEY) {
  return searchImpl(node_header_ptr, KARGS, 0 /* depth */);
}

void insertInOrder(Nodes::Node4* new_node, uint8_t k1, uint8_t k2, void* v1,
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

void insertImpl(Nodes::Header** node_header_ptr, KEY, Value value,
                size_t depth) {
  assert(node_header_ptr != nullptr);
  assert(*node_header_ptr != nullptr);
  assert(!Nodes::isLeaf(*node_header_ptr));
  assert(depth < key_len);

  size_t first_diff;
  const uint8_t* min_key;
  size_t min_key_len;
  bool prefix_matches = prefixMatches(*node_header_ptr, key, key_len, depth,
                                      first_diff, min_key, min_key_len);

  if (!prefix_matches) {
    Nodes::Header* new_node_header = Nodes::makeNewNode<Nodes::Type::NODE4>();
    Nodes::Node4* new_node = (Nodes::Node4*)new_node_header->getNode();

    // handle new prefix: it will contain a prefix of the old prefix, the common
    // section with the new key
    new_node_header->prefix_len = first_diff;
    size_t actual_prefix_len =
        Nodes::cap_prefix_size(new_node_header->prefix_len);
    new_node_header->prefix = (uint8_t*)malloc(actual_prefix_len);
    memcpy(new_node_header->prefix, (*node_header_ptr)->prefix,
           actual_prefix_len);

    // shorten old prefix: it'll be a suffix of the old prefix.
    // +1 because an element of the prefix (the first diff) will
    // be part of the new parent.
    (*node_header_ptr)->prefix_len -= (1 + new_node_header->prefix_len);
    uint8_t diff_bit;
    if (min_key != nullptr) {
      diff_bit = min_key[depth + first_diff];
      memcpy((*node_header_ptr)->prefix, min_key + depth + first_diff + 1,
             Nodes::cap_prefix_size((*node_header_ptr)->prefix_len));
    } else {
      diff_bit = (*node_header_ptr)->prefix[first_diff];
      memmove((*node_header_ptr)->prefix,
              (*node_header_ptr)->prefix + first_diff + 1,
              Nodes::cap_prefix_size((*node_header_ptr)->prefix_len));
    }

    Nodes::Leaf* new_leaf = Nodes::makeNewLeaf(key, key_len, value);
    insertInOrder(new_node, key[first_diff + depth], diff_bit,
                  Nodes::smuggleLeaf(new_leaf), *node_header_ptr);
    new_node_header->children_count = 2;
    *node_header_ptr = new_node_header;
    return;
  }

  depth += (*node_header_ptr)->prefix_len;

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
    const size_t stop = min(key_len, leaf->key_len);
    while (i < stop && key[i] == leaf->key[i]) {
      ++i;
    }
    assert(i != key_len || i != leaf->key_len); // TODO: support key update

    new_node_header->prefix_len = i - depth;
    size_t actual_prefix_size =
        Nodes::cap_prefix_size(new_node_header->prefix_len);
    new_node_header->prefix = (uint8_t*)malloc(actual_prefix_size);
    memcpy(new_node_header->prefix, leaf->key + depth, actual_prefix_size);

    new_node_header->children_count = 2;

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
    KEY, Value value) {
  assert(key[key_len - 1] == 0);
  insertImpl(node_header_ptr, KARGS, value, 0 /* depth */);
}

} // namespace Actions
