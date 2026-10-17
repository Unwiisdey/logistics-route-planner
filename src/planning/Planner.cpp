#include "logistics/planning/Planner.h"
#include "logistics/planning/Dijkstra.h"
#include "logistics/detail/checked_math.hpp"

#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace logistics {

RoutePlan Planner::plan(const Graph& graph, const PlanningRequest& request) {
    const auto* warehouse = graph.find_node(request.warehouse_id);
    if (!warehouse || warehouse->type() != NodeType::Warehouse) {
        throw std::invalid_argument("return warehouse must identify a warehouse node");
    }
    const NodeId start = request.start_node_id.value_or(request.warehouse_id);
    if (!graph.find_node(start)) {
        throw std::invalid_argument("unknown planning start node: " + start);
    }
    if (request.strategy != RouteStrategy::ShortestDistance && request.strategy != RouteStrategy::LowestCost) {
        throw std::invalid_argument("invalid route strategy");
    }
    if (!std::isfinite(request.departure_time) || request.departure_time < 0.0 ||
        (request.capacity_kg && (!std::isfinite(*request.capacity_kg) || *request.capacity_kg < 0.0))) {
        throw std::invalid_argument("departure time and optional capacity must be finite and nonnegative");
    }
    std::set<NodeId> pending;
    if (request.delivery_node_ids) {
        for (const auto& id : *request.delivery_node_ids) {
            const auto* node = graph.find_node(id);
            if (!node || node->type() != NodeType::DeliveryPoint) {
                throw std::invalid_argument("requested delivery must identify a delivery point: " + id);
            }
            if (!pending.insert(id).second) {
                throw std::invalid_argument("duplicate requested delivery point: " + id);
            }
        }
    } else {
        for (const auto& entry : graph.nodes()) {
            if (entry.second.type() == NodeType::DeliveryPoint) {
                pending.insert(entry.first);
            }
        }
    }
    RoutePlan result;
    result.strategy = request.strategy;
    result.start_node_id = start;
    result.warehouse_id = request.warehouse_id;
    result.departure_time = request.departure_time;
    result.requested_delivery_node_ids.assign(pending.begin(), pending.end());
    const auto failure = [&](const std::string& diagnostic) {
        RoutePlan failed;
        failed.strategy = result.strategy;
        failed.status = PlanStatus::NoFeasiblePlanFound;
        failed.start_node_id = start;
        failed.warehouse_id = request.warehouse_id;
        failed.departure_time = request.departure_time;
        failed.requested_delivery_node_ids = result.requested_delivery_node_ids;
        failed.unassigned_delivery_node_ids = result.requested_delivery_node_ids;
        failed.diagnostic = diagnostic;
        return failed;  // An exploratory prefix is never presented as an executable tour.
    };
    if (request.capacity_kg) {
        double demand = 0.0;
        for (const auto& id : pending) {
            demand = detail::checked_sum(demand, graph.find_node(id)->demand());
        }
        if (demand > *request.capacity_kg) {
            return failure("requested node demand exceeds vehicle capacity; no reload or multiple trips are planned");
        }
    }
    result.node_sequence.push_back(start);
    result.stops.push_back({start, std::nullopt, request.departure_time, request.departure_time, 0, StopRole::Start});
    const auto deliver_here = [&]() {
        const auto& id = result.node_sequence.back();
        if (pending.erase(id) != 0) {
            const auto arrival = detail::checked_sum(request.departure_time, result.total_travel_minutes);
            result.stops.push_back({id, std::nullopt, arrival, arrival,
                                    result.node_sequence.size() - 1, StopRole::Delivery});
        }
    };
    deliver_here();
    const auto append_path = [&](const PathResult& path) {
        for (std::size_t index = 0; index < path.road_sequence.size(); ++index) {
            const auto* edge = graph.find_edge_by_id(path.road_sequence[index]);
            result.road_sequence.push_back(edge->id());
            result.node_sequence.push_back(edge->to_id());
            result.total_distance_km = detail::checked_sum(result.total_distance_km, edge->distance());
            result.total_travel_minutes = detail::checked_sum(result.total_travel_minutes, edge->time());
            result.total_cost_yuan = detail::checked_sum(result.total_cost_yuan, edge->cost());
            // Also serve pending delivery points encountered along a shortest path.
            deliver_here();
        }
    };
    const WeightMetric metric = request.strategy == RouteStrategy::ShortestDistance
        ? WeightMetric::Distance : WeightMetric::Cost;
    while (!pending.empty()) {
        const auto paths = Dijkstra::single_source(graph, result.node_sequence.back(), metric);
        std::optional<NodeId> next;
        double best = std::numeric_limits<double>::infinity();
        for (const auto& id : pending) {
            const auto distance = paths.distance_to(id);
            if (distance && *distance < best) {
                next = id;
                best = *distance;
            }
        }
        if (!next) {
            return failure("nearest-neighbor planning cannot reach all remaining delivery points from " +
                           result.node_sequence.back());
        }
        append_path(paths.path_to(*next));
    }
    const auto home = Dijkstra::shortest_path(graph, result.node_sequence.back(), request.warehouse_id, metric);
    if (!home.reachable) {
        return failure("no open directed path returns the vehicle to warehouse " + request.warehouse_id);
    }
    append_path(home);
    const auto arrival = detail::checked_sum(request.departure_time, result.total_travel_minutes);
    result.stops.push_back({request.warehouse_id, std::nullopt, arrival, arrival,
                            result.node_sequence.size() - 1, StopRole::Return});
    result.status = PlanStatus::Feasible;
    result.diagnostic = "nearest-neighbor tour; globally optimal tour and time-window feasibility are not guaranteed";
    return result;
}

}  // namespace logistics
