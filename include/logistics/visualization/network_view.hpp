#pragma once

#include "logistics/graph/graph.hpp"
#include "logistics/simulation/simulation_state.hpp"

namespace logistics {

// GUI-independent interface. There is no GUI implementation in stage 1.
class NetworkView {
public:
    virtual ~NetworkView() = default;
    virtual void render(const Graph& graph, const SimulationState& state) = 0;
};

}  // namespace logistics
