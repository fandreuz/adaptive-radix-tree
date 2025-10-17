#ifndef NODES
#define NODES

#include <cstdint>

namespace Nodes {

enum class Type : uint8_t {
    NODE4, NODE16, NODE48, NODE256
};

struct Header {
    Type type;

    void* get_node() const;
};

class Node4 {
    uint8_t keys[4];
    Header children[4];
};

class Node16 {
    uint8_t keys[16];
    Header children[16];  
};

class Node48 {
    uint8_t child_index[256];
    Header children[48];
};

class Node256 {
    Header children[256];
};

template <Type NT>
Header* new_node();

constexpr size_t node_size(Type nt);

} // namespace Nodes

#endif // NODES
