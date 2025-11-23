#ifndef ACTIONS
#define ACTIONS

#include "nodes.hpp"
#include <string>

namespace Actions {

bool search(Nodes::Header *node_header, Key key, size_t depth);
// Returns the new root after insertion
void insert(Nodes::Header **node_header, Key key, Value value,
            size_t depth = 0);

} // namespace Actions

#endif // ACTIONS
