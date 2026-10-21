#include "logistics/planning/DynamicOrder.h"
#include "logistics/planning/Dijkstra.h"
#include "logistics/detail/checked_math.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>

namespace logistics {
namespace {
InsertionResult reject(const std::string& reason) { return {false, reason, {}}; }
void validate(const Graph& graph, const OrderPlanningContext& ctx) {
    if (!graph.find_node(ctx.start_node) || !graph.find_node(ctx.warehouse) ||
        graph.find_node(ctx.warehouse)->type() != NodeType::Warehouse ||
        !std::isfinite(ctx.start_time) || ctx.start_time < 0 ||
        !std::isfinite(ctx.capacity) || ctx.capacity < 0 ||
        !std::isfinite(ctx.current_load) || ctx.current_load < 0 ||
        !std::isfinite(ctx.unload_minutes) || ctx.unload_minutes < 0 ||
        (ctx.strategy != RouteStrategy::ShortestDistance && ctx.strategy != RouteStrategy::LowestCost)) {
        throw std::invalid_argument("invalid order planning context");
    }
    std::set<OrderId> ids;
    for (const auto& order : ctx.orders) {
        const auto* warehouse = graph.find_node(order.warehouse_id());
        const auto* destination = graph.find_node(order.delivery_node_id());
        if (!ids.insert(order.id()).second || !warehouse || warehouse->type() != NodeType::Warehouse ||
            !destination || destination->type() != NodeType::DeliveryPoint) {
            throw std::invalid_argument("duplicate order ID or invalid warehouse/delivery endpoint");
        }
    }
    for (const auto& id : ctx.loaded) {
        if (!ids.count(id)) { throw std::invalid_argument("loaded order is not in planning context"); }
    }
}
std::vector<OrderAction> actions(const OrderRoute& route) {
    std::vector<OrderAction> result;
    for (const auto& stop : route.stops) { result.push_back(stop.action); }
    return result;
}
double score(const OrderRoute& route, RouteStrategy strategy) {
    return strategy == RouteStrategy::ShortestDistance ? route.distance : route.cost;
}
}  // namespace

Order DynamicOrder::urgent(OrderId id, NodeId warehouse, NodeId destination, double quantity,
                           Minutes received_at, Minutes deadline_after) {
    if (!std::isfinite(deadline_after) || deadline_after <= 0) { throw std::invalid_argument("urgent deadline interval must be positive"); }
    return Order(std::move(id), std::move(warehouse), std::move(destination), quantity,
                 TimeWindow{received_at, detail::checked_sum(received_at, deadline_after)}, OrderPriority::Urgent, received_at);
}
InsertionResult DynamicOrder::evaluate(const Graph& graph, const OrderPlanningContext& ctx,
                                       const std::vector<OrderAction>& sequence, ShortestPathCache* cache) {
    validate(graph, ctx);
    if (ctx.current_load - ctx.capacity > 1e-9) { return reject("initial load exceeds capacity"); }
    std::map<OrderId, const Order*> orders;
    for (const auto& order : ctx.orders) { orders.emplace(order.id(), &order); }
    auto loaded = ctx.loaded;
    std::set<OrderId> delivered;
    OrderRoute route;
    route.nodes.push_back(ctx.start_node);
    route.start_time = ctx.start_time;
    double now = ctx.start_time, load = std::min(ctx.current_load, ctx.capacity);
    const auto metric = ctx.strategy == RouteStrategy::ShortestDistance ? WeightMetric::Distance : WeightMetric::Cost;
    const auto travel_to = [&](const NodeId& target) {
        const auto path = cache ? cache->query(graph, route.nodes.back(), target, metric)
                                : Dijkstra::shortest_path(graph, route.nodes.back(), target, metric);
        if (!path.reachable) { return false; }
        route.nodes.insert(route.nodes.end(), path.node_sequence.begin() + 1, path.node_sequence.end());
        route.roads.insert(route.roads.end(), path.road_sequence.begin(), path.road_sequence.end());
        now = detail::checked_sum(now, path.total_travel_minutes);
        route.distance = detail::checked_sum(route.distance, path.total_distance_km);
        route.travel = detail::checked_sum(route.travel, path.total_travel_minutes);
        route.cost = detail::checked_sum(route.cost, path.total_cost_yuan);
        return true;
    };
    for (const auto& action : sequence) {
        const auto found = orders.find(action.order_id);
        if (found == orders.end()) { return reject("action references unknown order"); }
        const auto& order = *found->second;
        if (delivered.count(order.id())) { return reject("order already delivered in candidate"); }
        const bool pickup = action.kind == OrderActionKind::Pickup;
        if (!pickup && action.kind != OrderActionKind::Delivery) { return reject("invalid order action"); }
        if (!travel_to(pickup ? order.warehouse_id() : order.delivery_node_id())) { return reject("unreachable order stop"); }
        const double arrival = now;
        if (pickup) {
            if (loaded.count(order.id())) { return reject("duplicate pickup"); }
            // Loading is instantaneous in this stage; cargo cannot exist before release.
            now = std::max(now, order.release_time());
            if (order.quantity() - (ctx.capacity - load) > 1e-9) { return reject("pickup would exceed vehicle capacity"); }
            load = std::min(ctx.capacity, detail::checked_sum(load, order.quantity()));
            loaded.insert(order.id());
        } else {
            if (!loaded.erase(order.id())) { return reject("delivery attempted before pickup"); }
            if (order.release_time() > now) { return reject("delivery precedes order release"); }
            if (order.time_window()) {
                const auto window = order.time_window()->evaluate(now);
                if (!window.on_time()) { return reject("time window missed: " + order.id()); }
                now = window.service_start;
            }
            if (order.quantity() > load + 1e-9 * std::max(1.0, load)) { return reject("order cargo missing"); }
            load = std::max(0.0, load - order.quantity());
            delivered.insert(order.id());
        }
        const double start = now;
        if (!pickup) { now = detail::checked_sum(now, ctx.unload_minutes); }
        route.stops.push_back({action, route.nodes.size() - 1, arrival, start, now, load});
    }
    if (delivered.size() != orders.size()) { return reject("candidate omits accepted orders"); }
    if (!travel_to(ctx.warehouse)) { return reject("cannot return to warehouse"); }
    route.finish_time = now;
    return {true, "all remaining windows, pickup precedence and capacity checked", std::move(route)};
}
InsertionResult DynamicOrder::insert(const Graph& graph, const OrderPlanningContext& ctx, const Order& order,
                                     ShortestPathCache* cache) {
    ShortestPathCache local;
    if (!cache) { cache = &local; }
    auto candidate_context = ctx;
    for (const auto& existing : ctx.orders) {
        if (existing.id() == order.id()) { return reject("duplicate order ID"); }
    }
    candidate_context.orders.push_back(order);
    validate(graph, candidate_context);
    InsertionResult best = reject("no feasible insertion found");
    double best_start = std::numeric_limits<double>::infinity();
    double best_score = std::numeric_limits<double>::infinity();
    std::string last_reason;
    for (std::size_t pickup = 0; pickup <= ctx.incumbent.size(); ++pickup) {
        auto with_pickup = ctx.incumbent;
        with_pickup.insert(with_pickup.begin() + static_cast<std::ptrdiff_t>(pickup), {order.id(), OrderActionKind::Pickup});
        for (std::size_t delivery = pickup + 1; delivery <= with_pickup.size(); ++delivery) {
            auto sequence = with_pickup;
            sequence.insert(sequence.begin() + static_cast<std::ptrdiff_t>(delivery), {order.id(), OrderActionKind::Delivery});
            auto candidate = evaluate(graph, candidate_context, sequence, cache);
            if (!candidate.accepted) { last_reason = candidate.reason; continue; }
            double service = 0;
            for (const auto& stop : candidate.route.stops) {
                if (stop.action.order_id == order.id() && stop.action.kind == OrderActionKind::Delivery) { service = stop.service_start; }
            }
            const double value = score(candidate.route, ctx.strategy);
            const bool urgent = order.priority() == OrderPriority::Urgent;
            if (!best.accepted || (urgent ? std::tie(service, value) < std::tie(best_start, best_score) : value < best_score)) {
                best_start = service; best_score = value; best = std::move(candidate);
            }
        }
    }
    if (!best.accepted) { best.reason += "; last constraint failure: " + last_reason; }
    return best;
}
InsertionResult DynamicOrder::initial(const Graph& graph, OrderPlanningContext ctx, ShortestPathCache* cache) {
    ShortestPathCache local;
    if (!cache) { cache = &local; }
    validate(graph, ctx);
    if (ctx.current_load - ctx.capacity > 1e-9) { return reject("initial cargo exceeds capacity"); }
    auto all = ctx.orders;
    std::sort(all.begin(), all.end(), [](const Order& a, const Order& b) {
        if (a.priority() != b.priority()) { return a.priority() == OrderPriority::Urgent; }
        const double ad = a.time_window() ? a.time_window()->latest : std::numeric_limits<double>::infinity();
        const double bd = b.time_window() ? b.time_window()->latest : std::numeric_limits<double>::infinity();
        return std::make_pair(ad, a.id()) < std::make_pair(bd, b.id());
    });
    ctx.orders.clear(); ctx.loaded.clear(); ctx.incumbent.clear();
    InsertionResult result = evaluate(graph, ctx, {}, cache);
    for (const auto& order : all) {
        if (order.warehouse_id() != ctx.start_node || order.release_time() > ctx.start_time) {
            return reject("initial orders must be available at the starting warehouse");
        }
        ctx.orders.push_back(order); ctx.loaded.insert(order.id());
        InsertionResult best = reject("no feasible initial ordering found for " + order.id());
        double best_score = std::numeric_limits<double>::infinity();
        for (std::size_t index = 0; index <= ctx.incumbent.size(); ++index) {
            auto sequence = ctx.incumbent;
            sequence.insert(sequence.begin() + static_cast<std::ptrdiff_t>(index), {order.id(), OrderActionKind::Delivery});
            auto attempt = evaluate(graph, ctx, sequence, cache);
            if (attempt.accepted && (!best.accepted || score(attempt.route, ctx.strategy) < best_score)) {
                best_score = score(attempt.route, ctx.strategy); best = std::move(attempt);
            }
        }
        if (!best.accepted) { return best; }
        ctx.incumbent = actions(best.route);
        result = std::move(best);
    }
    return result;
}
}  // namespace logistics
