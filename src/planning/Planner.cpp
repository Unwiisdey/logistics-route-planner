#include "logistics/planning/Planner.h"
#include "logistics/planning/Dijkstra.h"
#include "logistics/detail/checked_math.hpp"

#include <algorithm>
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

RouteTiming Planner::schedule(const Graph& graph, const RoutePlan& plan, Minutes unload_minutes) {
    if (plan.status != PlanStatus::Feasible || plan.node_sequence.empty() ||
        plan.node_sequence.size() != plan.road_sequence.size() + 1 ||
        plan.node_sequence.front() != plan.start_node_id || plan.node_sequence.back() != plan.warehouse_id ||
        !graph.find_node(plan.warehouse_id) || graph.find_node(plan.warehouse_id)->type() != NodeType::Warehouse ||
        !std::isfinite(unload_minutes) || unload_minutes < 0 ||
        !std::isfinite(plan.departure_time) || plan.departure_time < 0 ||
        !plan.unassigned_delivery_node_ids.empty() || !plan.unassigned_order_ids.empty() ||
        (plan.strategy != RouteStrategy::ShortestDistance && plan.strategy != RouteStrategy::LowestCost) ||
        plan.stops.size() < 2 || plan.stops.front().role != StopRole::Start ||
        plan.stops.front().route_node_index != 0 || plan.stops.back().role != StopRole::Return ||
        plan.stops.back().route_node_index != plan.node_sequence.size() - 1) {
        throw std::invalid_argument("schedule requires a complete route and finite nonnegative times");
    }
    std::set<std::size_t> delivery_steps;
    std::set<NodeId> served;
    std::size_t previous_step = 0;
    for (std::size_t index = 0; index < plan.stops.size(); ++index) {
        const auto& stop = plan.stops[index];
        if (stop.route_node_index >= plan.node_sequence.size() || stop.route_node_index < previous_step ||
            stop.node_id != plan.node_sequence[stop.route_node_index] ||
            (index > 0 && index + 1 < plan.stops.size() && stop.role != StopRole::Delivery)) {
            throw std::invalid_argument("scheduled stops must follow the route");
        }
        previous_step = stop.route_node_index;
        if (stop.role != StopRole::Delivery) { continue; }
        const auto* node = graph.find_node(stop.node_id);
        if (!node || node->type() != NodeType::DeliveryPoint || stop.route_node_index >= plan.node_sequence.size() ||
            plan.node_sequence[stop.route_node_index] != stop.node_id || !served.insert(stop.node_id).second ||
            !delivery_steps.insert(stop.route_node_index).second) {
            throw std::invalid_argument("invalid scheduled delivery stop");
        }
    }
    const std::set<NodeId> requested(plan.requested_delivery_node_ids.begin(), plan.requested_delivery_node_ids.end());
    if (requested != served || requested.size() != plan.requested_delivery_node_ids.size()) {
        throw std::invalid_argument("schedule delivery coverage mismatch");
    }
    RouteTiming timing;
    double now = plan.departure_time;
    double distance = 0, travel = 0, cost = 0;
    for (std::size_t i = 0; i < plan.node_sequence.size(); ++i) {
        const auto* node = graph.find_node(plan.node_sequence[i]);
        if (!node) { throw std::invalid_argument("scheduled node is missing"); }
        timing.arrivals.push_back(now);
        if (delivery_steps.count(i)) {
            const double wait = node->time_window() ? std::max(0.0, node->time_window()->earliest - now) : 0;
            timing.total_wait_minutes = detail::checked_sum(timing.total_wait_minutes, wait);
            timing.total_unload_minutes = detail::checked_sum(timing.total_unload_minutes, unload_minutes);
            now = detail::checked_sum(detail::checked_sum(now, wait), unload_minutes);
        }
        timing.departures.push_back(now);
        if (i < plan.road_sequence.size()) {
            const auto* edge = graph.find_edge_by_id(plan.road_sequence[i]);
            if (!edge || edge->from_id() != plan.node_sequence[i] || edge->to_id() != plan.node_sequence[i + 1] ||
                edge->traffic_status() == TrafficStatus::Closed) {
                throw std::invalid_argument("scheduled road is missing, closed or disconnected");
            }
            now = detail::checked_sum(now, edge->time());
            distance = detail::checked_sum(distance, edge->distance());
            travel = detail::checked_sum(travel, edge->time());
            cost = detail::checked_sum(cost, edge->cost());
        }
    }
    const auto near = [](double a, double b) {
        return std::isfinite(a) && std::isfinite(b) && std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)});
    };
    if (!near(distance, plan.total_distance_km) || !near(travel, plan.total_travel_minutes) || !near(cost, plan.total_cost_yuan)) {
        throw std::invalid_argument("route totals are stale; replan before scheduling");
    }
    return timing;
}

}  // namespace logistics
