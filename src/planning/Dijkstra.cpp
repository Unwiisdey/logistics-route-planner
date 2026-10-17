#include "logistics/planning/Dijkstra.h"
#include "logistics/detail/checked_math.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>

namespace logistics {

ShortestPaths::ShortestPaths(NodeId source) : source_(std::move(source)) {}

std::optional<double> ShortestPaths::distance_to(const NodeId& target) const {
    const auto found = distances_.find(target);
    if (found == distances_.end()) {
        throw std::invalid_argument("unknown shortest-path target: " + target);
    }
    return std::isfinite(found->second) ? std::optional<double>(found->second) : std::nullopt;
}

PathResult ShortestPaths::path_to(const NodeId& target) const {
    PathResult result;
    if (!distance_to(target)) {
        return result;
    }
    result.reachable = true;
    NodeId current = target;
    result.node_sequence.push_back(current);
    while (current != source_) {
        const auto& edge = predecessors_.at(current);
        result.road_sequence.push_back(edge.id());
        current = edge.from_id();
        result.node_sequence.push_back(current);
    }
    std::reverse(result.node_sequence.begin(), result.node_sequence.end());
    std::reverse(result.road_sequence.begin(), result.road_sequence.end());
    for (std::size_t index = 1; index < result.node_sequence.size(); ++index) {
        const auto& edge = predecessors_.at(result.node_sequence[index]);
        result.total_distance_km = detail::checked_sum(result.total_distance_km, edge.distance());
        result.total_travel_minutes = detail::checked_sum(result.total_travel_minutes, edge.time());
        result.total_cost_yuan = detail::checked_sum(result.total_cost_yuan, edge.cost());
    }
    return result;
}

ShortestPaths Dijkstra::single_source(const Graph& graph, const NodeId& source, WeightMetric metric) {
    if (!graph.find_node(source)) {
        throw std::invalid_argument("unknown shortest-path source: " + source);
    }
    if (metric != WeightMetric::Distance && metric != WeightMetric::Cost && metric != WeightMetric::Time) {
        throw std::invalid_argument("invalid shortest-path weight metric");
    }
    ShortestPaths result(source);
    for (const auto& entry : graph.nodes()) {
        result.distances_.emplace(entry.first, std::numeric_limits<double>::infinity());
    }
    result.distances_.at(source) = 0.0;
    using Entry = std::pair<double, NodeId>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    queue.emplace(0.0, source);
    while (!queue.empty()) {
        const auto [distance, node_id] = queue.top();
        queue.pop();
        if (distance != result.distances_.at(node_id)) {
            continue;  // Discard stale heap entries after a shorter relaxation.
        }
        // Sorting plus the heap's NodeId tie-break makes equal-cost paths reproducible.
        std::vector<const Edge*> outgoing;
        for (const auto& edge : graph.outgoing_edges(node_id)) {
            if (edge.traffic_status() != TrafficStatus::Closed) {
                outgoing.push_back(&edge);
            }
        }
        std::sort(outgoing.begin(), outgoing.end(), [](const Edge* left, const Edge* right) {
            return left->to_id() < right->to_id();
        });
        for (const auto* edge : outgoing) {
            const double weight = edge->weight(metric);
            // An overflowing detour must not invalidate other, finite shortest paths.
            if (weight > std::numeric_limits<double>::max() - distance) { continue; }
            const double candidate = distance + weight;
            auto& best = result.distances_.at(edge->to_id());
            // Strict improvement is essential: zero-weight cycles must not rewrite parents.
            if (candidate < best) {
                best = candidate;
                result.predecessors_.insert_or_assign(edge->to_id(), *edge);
                queue.emplace(candidate, edge->to_id());
            }
        }
    }
    return result;
}

PathResult Dijkstra::shortest_path(const Graph& graph, const NodeId& source,
                                   const NodeId& target, WeightMetric metric) {
    if (!graph.find_node(target)) {
        throw std::invalid_argument("unknown shortest-path target: " + target);
    }
    return single_source(graph, source, metric).path_to(target);
}

}  // namespace logistics
