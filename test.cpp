#include <iostream>
#include <cassert>
#include <string>
#include "src/actions.hpp"
#include "src/nodes.hpp"

int main() {
{ // new node
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();
    assert(root->children_count == 0);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);
}

{ // root + one leaf
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();
    assert(root->children_count == 0);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    Actions::insert(&root, "hello", 12);

    assert(root->children_count == 1);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    auto node = (Nodes::Node4*) root->getNode();
    assert(node->keys[0] == 'h');
    assert(Nodes::isLeaf(node->children[0]));
    auto leaf = Nodes::asLeaf(node->children[0]);
    assert(leaf->key == "hello");
    assert(leaf->value == 12);
}

{ // root + two leafs
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();
    assert(root->children_count == 0);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    Actions::insert(&root, "hello", 12);
    Actions::insert(&root, "ciao", 13);

    assert(root->children_count == 2);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    auto node = (Nodes::Node4*) root->getNode();
    {
        assert(node->keys[0] == 'c');
        assert(Nodes::isLeaf(node->children[0]));
        auto leaf = Nodes::asLeaf(node->children[0]);
        assert(leaf->key == "ciao");
        assert(leaf->value == 13);
    }
    {
        assert(node->keys[1] == 'h');
        assert(Nodes::isLeaf(node->children[1]));
        auto leaf = Nodes::asLeaf(node->children[1]);
        assert(leaf->key == "hello");
        assert(leaf->value == 12);
    }
}

{ // root + three leafs
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();
    assert(root->children_count == 0);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    Actions::insert(&root, "hello", 12);
    Actions::insert(&root, "ciao", 13);
    Actions::insert(&root, "bonjour", 14);

    assert(root->children_count == 3);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    auto node = (Nodes::Node4*) root->getNode();
    {
        assert(node->keys[0] == 'b');
        assert(Nodes::isLeaf(node->children[0]));
        auto leaf = Nodes::asLeaf(node->children[0]);
        assert(leaf->key == "bonjour");
        assert(leaf->value == 14);
    }
    {
        assert(node->keys[1] == 'c');
        assert(Nodes::isLeaf(node->children[1]));
        auto leaf = Nodes::asLeaf(node->children[1]);
        assert(leaf->key == "ciao");
        assert(leaf->value == 13);
    }
    {
        assert(node->keys[2] == 'h');
        assert(Nodes::isLeaf(node->children[2]));
        auto leaf = Nodes::asLeaf(node->children[2]);
        assert(leaf->key == "hello");
        assert(leaf->value == 12);
    }
}

{ // root growth
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();
    assert(root->children_count == 0);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    Actions::insert(&root, "hello", 12);
    Actions::insert(&root, "ciao", 13);
    Actions::insert(&root, "bonjour", 14);
    Actions::insert(&root, "doberdan", 15);
    Actions::insert(&root, "aufwiedersehen", 16);

    assert(root->children_count == 5);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);
    assert(root->type == Nodes::Type::NODE16);

    auto node = (Nodes::Node16*) root->getNode();
    {
        assert(node->keys[0] == 'a');
        assert(Nodes::isLeaf(node->children[0]));
        auto leaf = Nodes::asLeaf(node->children[0]);
        assert(leaf->key == "aufwiedersehen");
        assert(leaf->value == 16);
    }
    {
        assert(node->keys[1] == 'b');
        assert(Nodes::isLeaf(node->children[1]));
        auto leaf = Nodes::asLeaf(node->children[1]);
        assert(leaf->key == "bonjour");
        assert(leaf->value == 14);
    }
    {
        assert(node->keys[2] == 'c');
        assert(Nodes::isLeaf(node->children[2]));
        auto leaf = Nodes::asLeaf(node->children[2]);
        assert(leaf->key == "ciao");
        assert(leaf->value == 13);
    }
    {
        assert(node->keys[3] == 'd');
        assert(Nodes::isLeaf(node->children[3]));
        auto leaf = Nodes::asLeaf(node->children[3]);
        assert(leaf->key == "doberdan");
        assert(leaf->value == 15);
    }
    {
        assert(node->keys[4] == 'h');
        assert(Nodes::isLeaf(node->children[4]));
        auto leaf = Nodes::asLeaf(node->children[4]);
        assert(leaf->key == "hello");
        assert(leaf->value == 12);
    }
}

{ // root + node + node
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();
    assert(root->children_count == 0);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    Actions::insert(&root, "hello", 12);
    Actions::insert(&root, "hella", 13);

    assert(root->children_count == 1);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    auto root_node = (Nodes::Node4*) root->getNode();
    assert(root_node->keys[0] == 'h');
    assert(!Nodes::isLeaf(root_node->children[0]));
    auto header = (Nodes::Header*) root_node->children[0];
    assert(header->children_count == 2);
    assert(header->prefix_len == 3);
    assert(std::string((char*) header->prefix, 3) == "ell");
    auto node = (Nodes::Node4*) header->getNode();
    {
        assert(node->keys[0] == 'a');
        assert(Nodes::isLeaf(node->children[0]));
        auto leaf = Nodes::asLeaf(node->children[0]);
        assert(leaf->key == "hella");
        assert(leaf->value == 13);
    }
    {
        assert(node->keys[1] == 'o');
        assert(Nodes::isLeaf(node->children[1]));
        auto leaf = Nodes::asLeaf(node->children[1]);
        assert(leaf->key == "hello");
        assert(leaf->value == 12);
    }
}

{ // root + node + node (second is shorter)
    Nodes::Header* root = Nodes::makeNewNode<Nodes::Type::NODE4>();
    assert(root->children_count == 0);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    Actions::insert(&root, "hello", 12);
    Actions::insert(&root, "hell", 13);

    assert(root->children_count == 1);
    assert(root->prefix == nullptr);
    assert(root->prefix_len == 0);

    auto root_node = (Nodes::Node4*) root->getNode();
    assert(root_node->keys[0] == 'h');
    assert(!Nodes::isLeaf(root_node->children[0]));
    auto header = (Nodes::Header*) root_node->children[0];
    assert(header->children_count == 2);
    assert(header->prefix_len == 3);
    assert(std::string((char*) header->prefix, 3) == "ell");

    auto node = (Nodes::Node4*) header->getNode();
    {
        assert(node->keys[0] == 0);
        assert(Nodes::isLeaf(node->children[0]));
        auto leaf = Nodes::asLeaf(node->children[0]);
        assert(leaf->key == "hell");
        assert(leaf->value == 13);
    }
    {
        assert(node->keys[1] == 'o');
        assert(Nodes::isLeaf(node->children[1]));
        auto leaf = Nodes::asLeaf(node->children[1]);
        assert(leaf->key == "hello");
        assert(leaf->value == 12);
    }
}
}
