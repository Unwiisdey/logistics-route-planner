#include "logistics/planning/Replanner.h"
#include "logistics/planning/Dijkstra.h"
#include "logistics/detail/checked_math.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace logistics {
ReplanResult Replanner::repair(const Graph& graph, const OrderPlanningContext& ctx,
                               const OrderRoute& old, const std::set<RoadId>& triggers, ShortestPathCache* cache) {
    if (!graph.find_node(ctx.start_node) || !graph.find_node(ctx.warehouse) ||
        graph.find_node(ctx.warehouse)->type() != NodeType::Warehouse ||
        !std::isfinite(ctx.start_time) || ctx.start_time < 0 ||
        !std::isfinite(ctx.current_load) || ctx.current_load < 0 ||
        !std::isfinite(ctx.capacity) || ctx.capacity < 0 || ctx.current_load - ctx.capacity > 1e-9 ||
        !std::isfinite(ctx.unload_minutes) || ctx.unload_minutes < 0 ||
        old.nodes.empty() || old.nodes.size() != old.roads.size() + 1 ||
        old.nodes.front() != ctx.start_node || old.nodes.back() != ctx.warehouse) {
        throw std::invalid_argument("remaining route does not match replanning anchor");
    }
    ReplanResult result;
    auto& route = result.route;
    route.nodes = {ctx.start_node}; route.start_time = ctx.start_time;
    double now = ctx.start_time, load = ctx.current_load;
    auto loaded = ctx.loaded;
    std::set<OrderId> delivered;
    std::map<OrderId, const Order*> orders;
    for (const auto& order : ctx.orders) {
        if (!orders.emplace(order.id(), &order).second) { throw std::invalid_argument("duplicate remaining order"); }
    }
    for (const auto& id : loaded) {
        if (!orders.count(id)) { throw std::invalid_argument("unknown loaded order"); }
    }
    std::size_t previous = 0;
    const auto leg = [&](std::size_t end) {
        if (end < previous || end >= old.nodes.size()) { throw std::invalid_argument("invalid remaining stop index"); }
        std::vector<RoadId> roads(old.roads.begin() + static_cast<std::ptrdiff_t>(previous),
                                  old.roads.begin() + static_cast<std::ptrdiff_t>(end));
        const bool affected = std::any_of(roads.begin(), roads.end(), [&](const RoadId& id) { return triggers.count(id) != 0; });
        if (affected) {
            ++result.searched_legs;
            const auto path = cache ? cache->query(graph, route.nodes.back(), old.nodes[end], WeightMetric::Time)
                                    : Dijkstra::shortest_path(graph, route.nodes.back(), old.nodes[end], WeightMetric::Time);
            if (!path.reachable) { throw std::runtime_error("affected leg has no traversable path"); }
            roads = path.road_sequence;
        }
        for (const auto& id : roads) {
            const auto* edge = graph.find_edge_by_id(id);
            if (!edge || edge->from_id() != route.nodes.back() || edge->traffic_status() == TrafficStatus::Closed) {
                throw std::invalid_argument("invalid remaining road");
            }
            route.roads.push_back(id); route.nodes.push_back(edge->to_id());
            now = detail::checked_sum(now, edge->time());
            route.travel = detail::checked_sum(route.travel, edge->time());
            route.distance = detail::checked_sum(route.distance, edge->distance());
            route.cost = detail::checked_sum(route.cost, edge->cost());
        }
        if (route.nodes.back() != old.nodes[end]) { throw std::invalid_argument("disconnected remaining leg"); }
        previous = end;
    };
    for (const auto& stop : old.stops) {
        leg(stop.node_index);
        const auto found = orders.find(stop.action.order_id);
        if (found == orders.end() || delivered.count(stop.action.order_id)) { throw std::invalid_argument("invalid remaining action"); }
        const auto& order = *found->second;
        const bool pickup = stop.action.kind == OrderActionKind::Pickup;
        if (!pickup && stop.action.kind != OrderActionKind::Delivery) { throw std::invalid_argument("invalid action kind"); }
        if (route.nodes.back() != (pickup ? order.warehouse_id() : order.delivery_node_id())) {
            throw std::invalid_argument("order action endpoint mismatch");
        }
        const auto arrival = now;
        if (pickup) {
            if (!loaded.insert(order.id()).second || order.quantity() - (ctx.capacity - load) > 1e-9) {
                throw std::invalid_argument("invalid pickup load");
            }
            now = std::max(now, order.release_time());
            load = std::min(ctx.capacity, detail::checked_sum(load, order.quantity()));
        } else {
            if (!loaded.erase(order.id()) || order.quantity() > load + 1e-9) { throw std::invalid_argument("delivery cargo missing"); }
            if (order.release_time() > now) { throw std::invalid_argument("delivery before order release"); }
            if (order.time_window()) {
                const auto window = order.time_window()->evaluate(now);
                now = window.service_start;
                if (!window.on_time()) { result.predicted_late.push_back(order.id()); }
            }
            load = std::max(0.0, load - order.quantity()); delivered.insert(order.id());
        }
        const auto service = now;
        if (!pickup) { now = detail::checked_sum(now, ctx.unload_minutes); }
        route.stops.push_back({stop.action, route.nodes.size() - 1, arrival, service, now, load});
    }
    if (delivered.size() != orders.size()) { throw std::invalid_argument("remaining route omits orders"); }
    leg(old.roads.size()); route.finish_time = now;
    return result;
}
}  // namespace logistics
