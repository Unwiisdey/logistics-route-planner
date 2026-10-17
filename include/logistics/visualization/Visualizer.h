#pragma once

#include "logistics/graph/Graph.h"
#include "logistics/planning/route_plan.hpp"

#include <filesystem>

namespace logistics {

class Visualizer {
public:
    // Export only a complete, consistent plan; plotting remains independent of C++.
    static void export_csv(const Graph& graph, const RoutePlan& plan,
                           const std::filesystem::path& directory);
};

}  // namespace logistics
