#include "logistics/traffic/TrafficSimulator.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace logistics {
TrafficSimulator::TrafficSimulator(TrafficOptions options) : options_(options), random_(options.seed) {
    if (!std::isfinite(options.interval_minutes) || options.interval_minutes <= 0 ||
        !std::isfinite(options.fraction) || options.fraction <= 0 || options.fraction > 1 ||
        !std::isfinite(options.min_multiplier) || options.min_multiplier < 1.2 ||
        !std::isfinite(options.max_multiplier) || options.max_multiplier < options.min_multiplier) {
        throw std::invalid_argument("invalid traffic interval, fraction or congestion multiplier");
    }
}
bool TrafficSimulator::significant_increase(Minutes before, Minutes after) {
    if (!std::isfinite(before) || !std::isfinite(after) || before < 0 || after < 0) {
        throw std::invalid_argument("traffic time must be finite and nonnegative");
    }
    if (before == 0) { return after > 0; }
    return after > before && (after - before) / before >= 0.2 - 1e-12;
}
std::vector<TrafficUpdate> TrafficSimulator::sample(const Graph& graph) {
    std::vector<const Edge*> eligible;
    for (const auto& node : graph.nodes()) {
        for (const auto& edge : graph.outgoing_edges(node.first)) {
            if (edge.traffic_status() != TrafficStatus::Closed && edge.base_time() > 0) { eligible.push_back(&edge); }
        }
    }
    std::sort(eligible.begin(), eligible.end(), [](const Edge* a, const Edge* b) { return a->id() < b->id(); });
    std::shuffle(eligible.begin(), eligible.end(), random_);
    const auto count = static_cast<std::size_t>(std::ceil(options_.fraction * eligible.size()));
    std::uniform_real_distribution<double> multiplier(options_.min_multiplier, options_.max_multiplier);
    std::vector<TrafficUpdate> result;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& edge = *eligible[i];
        const bool recover = edge.traffic_status() == TrafficStatus::Congested;
        const double time = recover ? edge.base_time() : edge.base_time() * multiplier(random_);
        if (!std::isfinite(time)) { throw std::overflow_error("congestion time overflow"); }
        result.push_back({edge.id(), time, recover ? TrafficStatus::Clear : TrafficStatus::Congested});
    }
    return result;
}
std::vector<TrafficChange> TrafficSimulator::validate(const Graph& graph, Minutes at, const std::vector<TrafficUpdate>& updates) {
    if (!std::isfinite(at) || at < 0) { throw std::invalid_argument("invalid traffic event time"); }
    std::set<RoadId> unique;
    std::vector<TrafficChange> changes;
    for (const auto& update : updates) {
        const auto* edge = graph.find_edge_by_id(update.road_id);
        if (!edge || !unique.insert(update.road_id).second) { throw std::invalid_argument("unknown or duplicate traffic road"); }
        if (edge->traffic_status() == TrafficStatus::Closed ||
            (update.status != TrafficStatus::Clear && update.status != TrafficStatus::Congested)) {
            throw std::invalid_argument("stage 6 traffic supports clear/congested roads only");
        }
        const bool trigger = significant_increase(edge->time(), update.time);
        changes.push_back({at, edge->id(), edge->time(), update.time, edge->traffic_status(), update.status, trigger});
    }
    return changes;
}
std::vector<TrafficChange> TrafficSimulator::apply(Graph& graph, Minutes at, const std::vector<TrafficUpdate>& updates) {
    auto changes = validate(graph, at, updates);
    for (const auto& update : updates) {
        const auto* edge = graph.find_edge_by_id(update.road_id);
        const auto from = edge->from_id(), to = edge->to_id();
        graph.update_edge_traffic(from, to, update.time, update.status);
    }
    return changes;
}
}  // namespace logistics
