#include "actions.hpp"
#include "lock.hpp"
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

const Value* searchImpl(Nodes::Header* root, KEY) {
  Nodes::Header* parent;
  Nodes::Header* node_header;
  size_t depth;
  size_t version;
  size_t parent_version;

RESTART_POINT:
  node_header = root;
  parent = nullptr;
  depth = 0;

  while (true) {
    assert(node_header != nullptr);
    assert(!Nodes::isLeaf(node_header));
    assert(depth < key_len);

    READ_LOCK_OR_RESTART(node_header, version)
    if (parent != nullptr) {
      // TODO: get rid of the if
      READ_UNLOCK_OR_RESTART(parent, parent_version)
    }

    if (key_len < depth + node_header->prefix_len + 1) {
      READ_UNLOCK_OR_RESTART(node_header, version)
      return nullptr;
    }

    {
      size_t first_diff;
      const uint8_t* min_key;
      size_t min_key_len;
      bool match = prefixMatches(node_header, key, key_len, depth, first_diff,
                                 min_key, min_key_len);
      if (!match) {
        READ_UNLOCK_OR_RESTART(node_header, version)
        return nullptr;
      }
    }

    depth += node_header->prefix_len;

    void** next_src = Nodes::findChild(node_header, key[depth]);
    CHECK_OR_RESTART(node_header, version)

    if (next_src == nullptr) {
      READ_UNLOCK_OR_RESTART(node_header, version)
      return nullptr;
    }

    assert(*next_src != nullptr);
    ++depth;

    if (Nodes::isLeaf(*next_src)) {
      auto leaf = Nodes::asLeaf(*next_src);
      bool match = key_len == leaf->key_len && memcmp(leaf->key, KARGS) == 0;
      READ_UNLOCK_OR_RESTART(node_header, version)
      return match ? &leaf->value : nullptr;
    }

    parent = node_header;
    parent_version = version;
    node_header = Nodes::asHeader(*next_src);
  }
}

const Value* search(Nodes::Header* root, KEY) {
  return searchImpl(root, KARGS);
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

// Returns the new header
void* splitLeafPrefix(Nodes::Leaf* old_leaf, KEY, Value value, size_t depth) {
  // What is the common key segment?
  size_t i = depth;
  const size_t stop = min(key_len, old_leaf->key_len);
  while (i < stop && key[i] == old_leaf->key[i]) {
    ++i;
  }
  if (i == key_len && key_len == old_leaf->key_len) {
    old_leaf->value = value;
    return Nodes::smuggleLeaf(old_leaf);
  }
  assert(i == key_len || i == old_leaf->key_len || key[i] != old_leaf->key[i]);

  // The new parent of both leaf and the new value
  Nodes::Header* new_node_header = Nodes::makeNewNode<Nodes::Type::NODE4>();
  Nodes::Node4* new_node = (Nodes::Node4*)new_node_header->getNode();

  new_node_header->prefix_len = i - depth;
  size_t actual_prefix_size =
      Nodes::cap_prefix_size(new_node_header->prefix_len);
  new_node_header->prefix = (uint8_t*)malloc(actual_prefix_size);
  memcpy(new_node_header->prefix, old_leaf->key + depth, actual_prefix_size);

  new_node_header->children_count = 2;

  Nodes::Leaf* new_leaf = Nodes::makeNewLeaf(key, key_len, value);
  insertInOrder(new_node, key[i], old_leaf->key[i],
                Nodes::smuggleLeaf(new_leaf), Nodes::smuggleLeaf(old_leaf));
  return new_node_header;
}

void insertImpl(Nodes::Header* root, KEY, Value value) {
  Nodes::Header** node_header_ptr;
  Nodes::Header* parent;
  size_t depth;
  size_t parent_version;
  size_t version;

RESTART_POINT:
  parent = nullptr;

  READ_LOCK_OR_RESTART(root, version)
  void** next_src = Nodes::findChild(root, key[0]);
  CHECK_OR_RESTART(root, version)

  if (next_src == nullptr || *next_src == nullptr) {
    assert(!Nodes::isFull(root));
    UPGRADE_TO_WRITE_LOCK_OR_RESTART(root, version)
    Nodes::addChild(root, KARGS, value, 0);
    Lock::writeUnlock(root);
    return;
  }

  depth = 1;
  if (Nodes::isLeaf(*next_src)) {
    UPGRADE_TO_WRITE_LOCK_OR_RESTART(root, version)
    *next_src = splitLeafPrefix(Nodes::asLeaf(*next_src), KARGS, value, depth);
    Lock::writeUnlock(root);
    return;
  }

  parent = root;
  parent_version = version;
  node_header_ptr = (Nodes::Header**)next_src;

  while (true) {
    Nodes::Header* node_header = *node_header_ptr;
    READ_LOCK_OR_RESTART(node_header, version)

    assert(node_header_ptr != nullptr);
    assert(node_header != nullptr);
    assert(!Nodes::isLeaf(node_header));
    assert(depth < key_len);

    size_t first_diff;
    const uint8_t* min_key;
    size_t min_key_len;
    bool prefix_matches = prefixMatches(node_header, key, key_len, depth,
                                        first_diff, min_key, min_key_len);

    if (!prefix_matches) {
      UPGRADE_TO_WRITE_LOCK_OR_RESTART(parent, parent_version)
      UPGRADE_TO_WRITE_LOCK_OR_RESTART_WITH_LOCKED_NODE(node_header, version,
                                                        parent)

      Nodes::Header* new_node_header = Nodes::makeNewNode<Nodes::Type::NODE4>();
      Nodes::Node4* new_node = (Nodes::Node4*)new_node_header->getNode();

      // handle new prefix: it will contain a prefix of the old prefix, the
      // common section with the new key
      new_node_header->prefix_len = first_diff;
      size_t actual_prefix_len =
          Nodes::cap_prefix_size(new_node_header->prefix_len);
      new_node_header->prefix = (uint8_t*)malloc(actual_prefix_len);
      memcpy(new_node_header->prefix, node_header->prefix, actual_prefix_len);

      // shorten old prefix: it'll be a suffix of the old prefix.
      // +1 because an element of the prefix (the first diff) will
      // be part of the new parent.
      node_header->prefix_len -= (1 + new_node_header->prefix_len);
      uint8_t diff_bit;
      if (min_key != nullptr) {
        diff_bit = min_key[depth + first_diff];
        memcpy(node_header->prefix, min_key + depth + first_diff + 1,
               Nodes::cap_prefix_size(node_header->prefix_len));
      } else {
        diff_bit = node_header->prefix[first_diff];
        memmove(node_header->prefix, node_header->prefix + first_diff + 1,
                Nodes::cap_prefix_size(node_header->prefix_len));
      }

      Nodes::Leaf* new_leaf = Nodes::makeNewLeaf(key, key_len, value);
      insertInOrder(new_node, key[first_diff + depth], diff_bit,
                    Nodes::smuggleLeaf(new_leaf), node_header);
      new_node_header->children_count = 2;
      assert(*node_header_ptr != root);
      *node_header_ptr = new_node_header;

      Lock::writeUnlock(node_header);
      Lock::writeUnlock(parent);

      return;
    }

    depth += node_header->prefix_len;

    void** next_src = Nodes::findChild(node_header, key[depth]);
    CHECK_OR_RESTART(node_header, version)

    if (next_src == nullptr || *next_src == nullptr) {
      if (Nodes::isFull(node_header)) {
        UPGRADE_TO_WRITE_LOCK_OR_RESTART(parent, parent_version)
        UPGRADE_TO_WRITE_LOCK_OR_RESTART_WITH_LOCKED_NODE(node_header, version,
                                                          parent)

        assert(*node_header_ptr != root); // root should not need to be grown
        Nodes::grow(node_header_ptr);
        Nodes::addChild(*node_header_ptr, KARGS, value, depth);

        Lock::writeUnlockObsolete(node_header);
        Lock::writeUnlock(parent);
        node_header = *node_header_ptr;
      } else {
        UPGRADE_TO_WRITE_LOCK_OR_RESTART(node_header, version)
        READ_UNLOCK_OR_RESTART_WITH_LOCKED_NODE(parent, parent_version,
                                                node_header)
        Nodes::addChild(node_header, KARGS, value, depth);
        Lock::writeUnlock(node_header);
      }
      return;
    }

    READ_UNLOCK_OR_RESTART(parent, parent_version)

    depth += 1;
    if (Nodes::isLeaf(*next_src)) {
      UPGRADE_TO_WRITE_LOCK_OR_RESTART(node_header, version)
      *next_src =
          splitLeafPrefix(Nodes::asLeaf(*next_src), KARGS, value, depth);
      Lock::writeUnlock(node_header);
      return;
    }

    parent = node_header;
    parent_version = version;
    node_header_ptr = (Nodes::Header**)next_src;
  }
}

void insert(Nodes::Header* root, KEY, Value value) {
  assert(key[key_len - 1] == 0);
  insertImpl(root, KARGS, value);
}

} // namespace Actions
