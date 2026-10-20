#pragma once

#include "logistics/graph/graph.hpp"
#include "logistics/simulation/simulation_state.hpp"

namespace logistics {

// GUI-independent interface for a future live view; current Python views replay exported snapshots.
class NetworkView {
public:
    virtual ~NetworkView() = default;
    virtual void render(const Graph& graph, const SimulationState& state) = 0;
};

}  // namespace logistics
