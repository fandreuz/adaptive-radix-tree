#include <cassert>
#include <cstdlib>
#include <new>
#include "nodes.hpp"

using namespace Nodes;

void* Header::get_node() const {
    return (void*) (((uintptr_t) this) + sizeof(Header));
}

#define DISPATCH_TYPE(action, nt)             \
    if (nt == Type::NODE4)   action(Node4);   \
    if (nt == Type::NODE16)  action(Node16);  \
    if (nt == Type::NODE48)  action(Node48);  \
    if (nt == Type::NODE256) action(Node256);

constexpr size_t Nodes::node_size(Type nt) {
    #define SIZEOF_ACTION(N) return sizeof(N)
    DISPATCH_TYPE(SIZEOF_ACTION, nt);
}

template <Type NT>
Header* Nodes::new_node() {
    Header* header = (Header*) malloc(sizeof(Header) + node_size(NT));
    header->type = NT;
    
    void* node = header->get_node();
    #define CTOR_ACTION(N) new (node) N();
    DISPATCH_TYPE(CTOR_ACTION, NT);

    return header;
}
