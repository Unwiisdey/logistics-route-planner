#pragma once
#include "logistics/graph/Graph.h"
#include <cstdint>
#include <random>

namespace logistics {
struct TrafficOptions {
    Minutes interval_minutes{0.5};
    double fraction{0.1};
    double min_multiplier{1.2};
    double max_multiplier{2.0};
    std::uint32_t seed{42};
};
struct TrafficUpdate {
    RoadId road_id;
    Minutes time;
    TrafficStatus status;
};
struct TrafficChange {
    Minutes at;
    RoadId road_id;
    Minutes before;
    Minutes after;
    TrafficStatus before_status;
    TrafficStatus after_status;
    bool triggers_replan;
};
class TrafficSimulator {
public:
    explicit TrafficSimulator(TrafficOptions options = {});
    const TrafficOptions& options() const noexcept { return options_; }
    std::vector<TrafficUpdate> sample(const Graph& graph);
    // Inclusive 20%, relative to the immediately preceding time. Zero -> positive triggers.
    static bool significant_increase(Minutes before, Minutes after);
    static std::vector<TrafficChange> validate(const Graph& graph, Minutes at,
                                              const std::vector<TrafficUpdate>& updates);
    // Validate the complete batch before mutating the graph; no topology changes.
    static std::vector<TrafficChange> apply(Graph& graph, Minutes at,
                                           const std::vector<TrafficUpdate>& updates);
private:
    TrafficOptions options_;
    std::mt19937 random_;
};
}  // namespace logistics
