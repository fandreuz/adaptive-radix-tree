#include "src/nodes.hpp"
#include <cassert>
#include <iostream>
#include <string>

#define ASSERT_VALUE(out, expected)                                            \
  assert(out != nullptr);                                                      \
  assert(*out == expected);

int main() {
  { // common prefix
    Tree tree;

    uint8_t key[PREFIX_SIZE + 3]; // prefix + eq + diff + 0
    for (size_t i = 0; i < PREFIX_SIZE + 2; ++i) {
      key[i] = 1;
    }
    key[PREFIX_SIZE + 2] = 0;
    tree.insert(key, PREFIX_SIZE + 3, 10);
    ASSERT_VALUE(tree.search(key, PREFIX_SIZE + 3), 10);

    uint8_t key2[PREFIX_SIZE + 3];
    for (size_t i = 0; i < PREFIX_SIZE + 1; ++i) {
      key2[i] = 1;
    }
    key2[PREFIX_SIZE + 1] = 2;
    key2[PREFIX_SIZE + 2] = 0;
    tree.insert(key2, PREFIX_SIZE + 3, 11);

    ASSERT_VALUE(tree.search(key, PREFIX_SIZE + 3), 10);
    ASSERT_VALUE(tree.search(key2, PREFIX_SIZE + 3), 11);
  }

  { // common prefix + 1
    Tree tree;

    uint8_t key[PREFIX_SIZE + 4]; // prefix + eq + eq + diff + 0
    for (size_t i = 0; i < PREFIX_SIZE + 3; ++i) {
      key[i] = 1;
    }
    key[PREFIX_SIZE + 3] = 0;
    tree.insert(key, PREFIX_SIZE + 4, 10);
    ASSERT_VALUE(tree.search(key, PREFIX_SIZE + 4), 10);

    uint8_t key2[PREFIX_SIZE + 4];
    for (size_t i = 0; i < PREFIX_SIZE + 2; ++i) {
      key2[i] = 1;
    }
    key2[PREFIX_SIZE + 2] = 2;
    key2[PREFIX_SIZE + 3] = 0;
    tree.insert(key2, PREFIX_SIZE + 4, 11);

    ASSERT_VALUE(tree.search(key, PREFIX_SIZE + 4), 10);
    ASSERT_VALUE(tree.search(key2, PREFIX_SIZE + 4), 11);
  }

  { // common prefix with a non-leaf
    Tree tree;

    uint8_t key[PREFIX_SIZE + 5]; // prefix + eq + eq + eq + diff + 0
    for (size_t i = 0; i < PREFIX_SIZE + 4; ++i) {
      key[i] = 1;
    }
    key[PREFIX_SIZE + 4] = 0;
    tree.insert(key, PREFIX_SIZE + 5, 10);

    ASSERT_VALUE(tree.search(key, PREFIX_SIZE + 5), 10);

    uint8_t key2[PREFIX_SIZE + 5];
    for (size_t i = 0; i < PREFIX_SIZE + 3; ++i) {
      key2[i] = 1;
    }
    key2[PREFIX_SIZE + 3] = 2;
    key2[PREFIX_SIZE + 4] = 0;
    tree.insert(key2, PREFIX_SIZE + 5, 11);

    ASSERT_VALUE(tree.search(key, PREFIX_SIZE + 5), 10);
    ASSERT_VALUE(tree.search(key2, PREFIX_SIZE + 5), 11);

    uint8_t key3[PREFIX_SIZE + 4];
    for (size_t i = 0; i < PREFIX_SIZE + 2; ++i) {
      key3[i] = 1;
    }
    key3[PREFIX_SIZE + 2] = 2;
    key3[PREFIX_SIZE + 3] = 0;
    tree.insert(key3, PREFIX_SIZE + 4, 12);

    ASSERT_VALUE(tree.search(key, PREFIX_SIZE + 5), 10);
    ASSERT_VALUE(tree.search(key2, PREFIX_SIZE + 5), 11);
    ASSERT_VALUE(tree.search(key3, PREFIX_SIZE + 4), 12);
  }

  { // test min key
    Tree tree;

    uint8_t key[3];
    key[0] = 1;
    key[1] = 2;
    key[2] = 0;
    tree.insert(key, 3, 12);

    const uint8_t* out;
    size_t out_len;
    tree.findMinimumKey(out, out_len);
    assert(out_len == 3);
    assert(memcmp(key, out, 3) == 0);

    uint8_t key2[4];
    key2[0] = 1;
    key2[1] = 1;
    key2[2] = 2;
    key2[3] = 0;
    tree.insert(key2, 4, 13);

    tree.findMinimumKey(out, out_len);
    assert(out_len == 4);
    assert(memcmp(key2, out, 4) == 0);
  }
}
