#include "src/actions.hpp"
#include "src/nodes.hpp"
#include <cassert>
#include <iostream>
#include <string>

#define ASSERT_KEY_EQ(leaf, expected)                                          \
  assert(leaf->key_len == strlen(expected) + 1);                               \
  assert(memcmp(leaf->key, expected, leaf->key_len - 1) == 0);

#define ASSERT_VALUE(out, expected)                                            \
  assert(out != nullptr);                                                      \
  assert(*out == expected);

int main() {
  { // new node
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();
    assert(root->children_count == 0);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);
  }

  { // root + one leaf
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    Actions::insert(&root, "hello", 12);

    assert(root->children_count == 1);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    auto node = (Nodes::Node4*)root->getNode();
    assert(node->keys[0] == 'h');
    assert(Nodes::isLeaf(node->children[0]));
    auto leaf = Nodes::asLeaf(node->children[0]);
    ASSERT_KEY_EQ(leaf, "hello");
    assert(leaf->value == 12);
  }

  { // root + two leafs
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    Actions::insert(&root, "hello", 12);
    Actions::insert(&root, "ciao", 13);

    assert(root->children_count == 2);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    auto node = (Nodes::Node4*)root->getNode();
    {
      assert(node->keys[0] == 'c');
      assert(Nodes::isLeaf(node->children[0]));
      auto leaf = Nodes::asLeaf(node->children[0]);
      ASSERT_KEY_EQ(leaf, "ciao");
      assert(leaf->value == 13);
    }
    {
      assert(node->keys[1] == 'h');
      assert(Nodes::isLeaf(node->children[1]));
      auto leaf = Nodes::asLeaf(node->children[1]);
      ASSERT_KEY_EQ(leaf, "hello");
      assert(leaf->value == 12);
    }
  }

  { // root + three leafs
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    Actions::insert(&root, "hello", 12);
    Actions::insert(&root, "ciao", 13);
    Actions::insert(&root, "bonjour", 14);

    assert(root->children_count == 3);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    auto node = (Nodes::Node4*)root->getNode();
    {
      assert(node->keys[0] == 'b');
      assert(Nodes::isLeaf(node->children[0]));
      auto leaf = Nodes::asLeaf(node->children[0]);
      ASSERT_KEY_EQ(leaf, "bonjour");
      assert(leaf->value == 14);
    }
    {
      assert(node->keys[1] == 'c');
      assert(Nodes::isLeaf(node->children[1]));
      auto leaf = Nodes::asLeaf(node->children[1]);
      ASSERT_KEY_EQ(leaf, "ciao");
      assert(leaf->value == 13);
    }
    {
      assert(node->keys[2] == 'h');
      assert(Nodes::isLeaf(node->children[2]));
      auto leaf = Nodes::asLeaf(node->children[2]);
      ASSERT_KEY_EQ(leaf, "hello");
      assert(leaf->value == 12);
    }
  }

  { // root growth
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    Actions::insert(&root, "hello", 12);
    Actions::insert(&root, "ciao", 13);
    Actions::insert(&root, "bonjour", 14);
    Actions::insert(&root, "doberdan", 15);
    Actions::insert(&root, "aufwiedersehen", 16);

    assert(root->children_count == 5);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);
    assert(root->type == Nodes::Type::NODE16);

    auto node = (Nodes::Node16*)root->getNode();
    {
      assert(node->keys[0] == 'a');
      assert(Nodes::isLeaf(node->children[0]));
      auto leaf = Nodes::asLeaf(node->children[0]);
      ASSERT_KEY_EQ(leaf, "aufwiedersehen");
      assert(leaf->value == 16);
    }
    {
      assert(node->keys[1] == 'b');
      assert(Nodes::isLeaf(node->children[1]));
      auto leaf = Nodes::asLeaf(node->children[1]);
      ASSERT_KEY_EQ(leaf, "bonjour");
      assert(leaf->value == 14);
    }
    {
      assert(node->keys[2] == 'c');
      assert(Nodes::isLeaf(node->children[2]));
      auto leaf = Nodes::asLeaf(node->children[2]);
      ASSERT_KEY_EQ(leaf, "ciao");
      assert(leaf->value == 13);
    }
    {
      assert(node->keys[3] == 'd');
      assert(Nodes::isLeaf(node->children[3]));
      auto leaf = Nodes::asLeaf(node->children[3]);
      ASSERT_KEY_EQ(leaf, "doberdan");
      assert(leaf->value == 15);
    }
    {
      assert(node->keys[4] == 'h');
      assert(Nodes::isLeaf(node->children[4]));
      auto leaf = Nodes::asLeaf(node->children[4]);
      ASSERT_KEY_EQ(leaf, "hello");
      assert(leaf->value == 12);
    }

    ASSERT_VALUE(Actions::search(root, "hello"), 12);
    ASSERT_VALUE(Actions::search(root, "ciao"), 13);
    ASSERT_VALUE(Actions::search(root, "bonjour"), 14);
    ASSERT_VALUE(Actions::search(root, "doberdan"), 15);
    ASSERT_VALUE(Actions::search(root, "aufwiedersehen"), 16);
    assert(Actions::search(root, "come xe") == nullptr);
  }

  { // root + node + node
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    Actions::insert(&root, "hello", 12);
    Actions::insert(&root, "hella", 13);

    assert(root->children_count == 1);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    auto root_node = (Nodes::Node4*)root->getNode();
    assert(root_node->keys[0] == 'h');
    assert(!Nodes::isLeaf(root_node->children[0]));
    auto header = Nodes::asHeader(root_node->children[0]);
    assert(header->children_count == 2);
    assert(header->prefix_len == 3);
    assert(std::string((char*)header->prefix, 3) == "ell");
    auto node = (Nodes::Node4*)header->getNode();
    {
      assert(node->keys[0] == 'a');
      assert(Nodes::isLeaf(node->children[0]));
      auto leaf = Nodes::asLeaf(node->children[0]);
      ASSERT_KEY_EQ(leaf, "hella");
      assert(leaf->value == 13);
    }
    {
      assert(node->keys[1] == 'o');
      assert(Nodes::isLeaf(node->children[1]));
      auto leaf = Nodes::asLeaf(node->children[1]);
      ASSERT_KEY_EQ(leaf, "hello");
      assert(leaf->value == 12);
    }
  }

  { // root + node + node (second is shorter)
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    Actions::insert(&root, "hello", 12);
    Actions::insert(&root, "hell", 13);

    assert(root->children_count == 1);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    auto root_node = (Nodes::Node4*)root->getNode();
    assert(root_node->keys[0] == 'h');
    assert(!Nodes::isLeaf(root_node->children[0]));
    auto header = Nodes::asHeader(root_node->children[0]);
    assert(header->children_count == 2);
    assert(header->prefix_len == 3);
    assert(std::string((char*)header->prefix, 3) == "ell");

    auto node = (Nodes::Node4*)header->getNode();
    {
      assert(node->keys[0] == 0);
      assert(Nodes::isLeaf(node->children[0]));
      auto leaf = Nodes::asLeaf(node->children[0]);
      ASSERT_KEY_EQ(leaf, "hell");
      assert(leaf->value == 13);
    }
    {
      assert(node->keys[1] == 'o');
      assert(Nodes::isLeaf(node->children[1]));
      auto leaf = Nodes::asLeaf(node->children[1]);
      ASSERT_KEY_EQ(leaf, "hello");
      assert(leaf->value == 12);
    }
  }

  { // grow to node 16
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    uint8_t key[2];
    key[1] = 0;
    for (uint16_t i = 1; i <= 5; ++i) {
      key[0] = i;
      Actions::insert(&root, key, 2, 100 + i);
    }
    for (uint16_t i = 1; i <= 5; ++i) {
      key[0] = i;
      ASSERT_VALUE(Actions::search(root, key, 2), (100 + i));
    }
  }

  { // grow to node 48
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    uint8_t key[2];
    key[1] = 0;
    for (uint16_t i = 1; i <= 17; ++i) {
      key[0] = i;
      Actions::insert(&root, key, 2, 100 + i);
    }
    for (uint16_t i = 1; i <= 17; ++i) {
      key[0] = i;
      ASSERT_VALUE(Actions::search(root, key, 2), (100 + i));
    }
  }

  { // grow to node 256
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    uint8_t key[2];
    key[1] = 0;
    for (uint16_t i = 1; i <= 49; ++i) {
      key[0] = i;
      Actions::insert(&root, key, 2, 100 + i);
    }
    for (uint16_t i = 1; i <= 49; ++i) {
      key[0] = i;
      ASSERT_VALUE(Actions::search(root, key, 2), (100 + i));
    }
  }

  { // common prefix
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    uint8_t key[Nodes::PREFIX_SIZE + 3]; // prefix + eq + diff + 0
    for (size_t i = 0; i < Nodes::PREFIX_SIZE + 2; ++i) {
      key[i] = 1;
    }
    key[Nodes::PREFIX_SIZE + 2] = 0;
    Actions::insert(&root, key, Nodes::PREFIX_SIZE + 3, 10);
    ASSERT_VALUE(Actions::search(root, key, Nodes::PREFIX_SIZE + 3), 10);

    uint8_t key2[Nodes::PREFIX_SIZE + 3];
    for (size_t i = 0; i < Nodes::PREFIX_SIZE + 1; ++i) {
      key2[i] = 1;
    }
    key2[Nodes::PREFIX_SIZE + 1] = 2;
    key2[Nodes::PREFIX_SIZE + 2] = 0;
    Actions::insert(&root, key2, Nodes::PREFIX_SIZE + 3, 11);

    ASSERT_VALUE(Actions::search(root, key, Nodes::PREFIX_SIZE + 3), 10);
    ASSERT_VALUE(Actions::search(root, key2, Nodes::PREFIX_SIZE + 3), 11);
  }

  { // common prefix + 1
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    uint8_t key[Nodes::PREFIX_SIZE + 4]; // prefix + eq + eq + diff + 0
    for (size_t i = 0; i < Nodes::PREFIX_SIZE + 3; ++i) {
      key[i] = 1;
    }
    key[Nodes::PREFIX_SIZE + 3] = 0;
    Actions::insert(&root, key, Nodes::PREFIX_SIZE + 4, 10);
    ASSERT_VALUE(Actions::search(root, key, Nodes::PREFIX_SIZE + 4), 10);

    uint8_t key2[Nodes::PREFIX_SIZE + 4];
    for (size_t i = 0; i < Nodes::PREFIX_SIZE + 2; ++i) {
      key2[i] = 1;
    }
    key2[Nodes::PREFIX_SIZE + 2] = 2;
    key2[Nodes::PREFIX_SIZE + 3] = 0;
    Actions::insert(&root, key2, Nodes::PREFIX_SIZE + 4, 11);

    ASSERT_VALUE(Actions::search(root, key, Nodes::PREFIX_SIZE + 4), 10);
    ASSERT_VALUE(Actions::search(root, key2, Nodes::PREFIX_SIZE + 4), 11);
  }

  { // common prefix with a non-leaf
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    uint8_t key[Nodes::PREFIX_SIZE + 5]; // prefix + eq + eq + eq + diff + 0
    for (size_t i = 0; i < Nodes::PREFIX_SIZE + 4; ++i) {
      key[i] = 1;
    }
    key[Nodes::PREFIX_SIZE + 4] = 0;
    Actions::insert(&root, key, Nodes::PREFIX_SIZE + 5, 10);

    ASSERT_VALUE(Actions::search(root, key, Nodes::PREFIX_SIZE + 5), 10);

    uint8_t key2[Nodes::PREFIX_SIZE + 5];
    for (size_t i = 0; i < Nodes::PREFIX_SIZE + 3; ++i) {
      key2[i] = 1;
    }
    key2[Nodes::PREFIX_SIZE + 3] = 2;
    key2[Nodes::PREFIX_SIZE + 4] = 0;
    Actions::insert(&root, key2, Nodes::PREFIX_SIZE + 5, 11);

    ASSERT_VALUE(Actions::search(root, key, Nodes::PREFIX_SIZE + 5), 10);
    ASSERT_VALUE(Actions::search(root, key2, Nodes::PREFIX_SIZE + 5), 11);

    uint8_t key3[Nodes::PREFIX_SIZE + 4];
    for (size_t i = 0; i < Nodes::PREFIX_SIZE + 2; ++i) {
      key3[i] = 1;
    }
    key3[Nodes::PREFIX_SIZE + 2] = 2;
    key3[Nodes::PREFIX_SIZE + 3] = 0;
    Actions::insert(&root, key3, Nodes::PREFIX_SIZE + 4, 12);

    ASSERT_VALUE(Actions::search(root, key, Nodes::PREFIX_SIZE + 5), 10);
    ASSERT_VALUE(Actions::search(root, key2, Nodes::PREFIX_SIZE + 5), 11);
    ASSERT_VALUE(Actions::search(root, key3, Nodes::PREFIX_SIZE + 4), 12);
  }

  { // test min key
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();

    uint8_t key[3];
    key[0] = 1;
    key[1] = 2;
    key[2] = 0;
    Actions::insert(&root, key, 3, 12);

    const uint8_t* out;
    size_t out_len;
    Actions::findMinimumKey(root, out, out_len);
    assert(out_len == 3);
    assert(memcmp(key, out, 3) == 0);

    uint8_t key2[4];
    key2[0] = 1;
    key2[1] = 1;
    key2[2] = 2;
    key2[3] = 0;
    Actions::insert(&root, key2, 4, 13);

    Actions::findMinimumKey(root, out, out_len);
    assert(out_len == 4);
    assert(memcmp(key2, out, 4) == 0);
  }
}
