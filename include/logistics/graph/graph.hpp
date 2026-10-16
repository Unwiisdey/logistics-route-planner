#pragma once

#include "logistics/model/types.hpp"

#include <unordered_map>
#include <vector>

namespace logistics {

// Storage prototype only. Stage 2 will add validated mutation and query APIs.
struct Graph {
    std::unordered_map<NodeId, Node> nodes;
    std::unordered_map<NodeId, std::vector<Road>> adjacency;
};

}  // namespace logistics
