#include "logistics/simulation/Simulation.h"
#include "logistics/detail/checked_math.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace logistics {
namespace {
double cargo_sum(const std::vector<Order>& orders) {
    double result = 0;
    for (const auto& order : orders) { result = detail::checked_sum(result, order.quantity()); }
    return result;
}
}  // namespace

void Simulation::wait_until(Minutes time) {
    if (!finished_ || vehicle_.status() != VehicleStatus::Idle || !std::isfinite(time) || time < clock_.now()) {
        throw std::invalid_argument("wait_until requires a completed idle vehicle and a future time");
    }
    clock_.advance(time - clock_.now());
    record("idle_wait"); history_.push_back(snapshot());
}

Simulation::Simulation(Graph graph, const NodeId& warehouse, std::vector<Order> orders, Minutes start,
                       SimulationOptions options, RouteStrategy strategy)
    : graph_(std::move(graph)), options_(std::move(options)), clock_(start, options_.step_minutes),
      vehicle_(options_.vehicle_id, options_.capacity_kg, cargo_sum(orders), warehouse), order_mode_(true) {
    if (!std::isfinite(options_.late_penalty_points) || options_.late_penalty_points < 0 ||
        (!orders.empty() && options_.late_penalty_points > std::numeric_limits<double>::max() / static_cast<double>(orders.size()))) {
        throw std::invalid_argument("invalid fixed penalty or penalty total");
    }
    route_.warehouse_id = warehouse;
    route_.departure_time = start;
    route_.strategy = strategy;
    OrderPlanningContext ctx;
    ctx.start_node = warehouse; ctx.warehouse = warehouse; ctx.start_time = start;
    ctx.capacity = vehicle_.capacity(); ctx.current_load = vehicle_.current_load();
    ctx.unload_minutes = options_.unload_minutes; ctx.strategy = strategy; ctx.orders = orders;
    for (const auto& order : orders) { ctx.loaded.insert(order.id()); }
    auto planned = DynamicOrder::initial(graph_, ctx);
    if (!planned.accepted) { throw std::invalid_argument("hard-window initial planning failed: " + planned.reason); }
    for (const auto& order : orders) { add_order_record(order, true); }
    apply_order_route(std::move(planned.route));
    record("start");
    enter_node();
    settle_instant_events();
    history_.push_back(snapshot());
}
void Simulation::add_order_record(const Order& order, bool loaded) {
    orders_.push_back(order);
    DeliveryRecord task;
    task.order_id = order.id(); task.node_id = order.delivery_node_id(); task.quantity = order.quantity();
    task.window = order.time_window(); task.warehouse_id = order.warehouse_id(); task.priority = order.priority();
    task.loaded = loaded;
    if (loaded) { task.picked_up_at = clock_.now(); }
    deliveries_.push_back(std::move(task));
}
std::size_t Simulation::order_index(const OrderId& id) const {
    for (std::size_t i = 0; i < deliveries_.size(); ++i) {
        if (deliveries_[i].order_id == id) { return i; }
    }
    throw std::logic_error("unknown runtime order: " + id);
}
OrderPlanningContext Simulation::order_context() const {
    OrderPlanningContext ctx;
    ctx.warehouse = route_.warehouse_id;
    ctx.capacity = vehicle_.capacity(); ctx.current_load = vehicle_.current_load();
    ctx.unload_minutes = options_.unload_minutes; ctx.strategy = route_.strategy;
    ctx.start_time = clock_.now();
    if (vehicle_.segment()) {
        ctx.start_node = vehicle_.segment()->to_id;
        ctx.start_time = phase_end_;
    } else {
        ctx.start_node = *vehicle_.position().node_id;
        if (active_delivery_) {
            ctx.start_time = vehicle_.status() == VehicleStatus::Waiting
                ? detail::checked_sum(phase_end_, options_.unload_minutes) : phase_end_;
            ctx.current_load = std::max(0.0, ctx.current_load - deliveries_[*active_delivery_].quantity);
        }
    }
    for (std::size_t i = 0; i < orders_.size(); ++i) {
        if (deliveries_[i].delivered_at || (active_delivery_ && *active_delivery_ == i)) { continue; }
        ctx.orders.push_back(orders_[i]);
        if (deliveries_[i].loaded) { ctx.loaded.insert(orders_[i].id()); }
    }
    if (pending_route_) {
        for (const auto& stop : pending_route_->stops) { ctx.incumbent.push_back(stop.action); }
    } else if (order_mode_) {
        for (const auto& action : order_actions_) { ctx.incumbent.push_back(action.second); }
    } else {
        for (const auto& entry : delivery_steps_) {
            if (!deliveries_[entry.second].delivered_at && (!active_delivery_ || entry.second != *active_delivery_)) {
                ctx.incumbent.push_back({deliveries_[entry.second].order_id, OrderActionKind::Delivery});
            }
        }
    }
    return ctx;
}
InsertionResult Simulation::insert_order(const Order& order) {
    InsertionResult result;
    const auto ctx = order_context();
    for (const auto& existing : orders_) {
        if (existing.id() == order.id()) { result.reason = "duplicate order ID (including completed orders)"; }
    }
    if (order.release_time() > clock_.now()) { result.reason = "order has not been released"; }
    if (active_delivery_) {
        const auto& locked = deliveries_[*active_delivery_];
        const auto start = locked.service_start.value_or(phase_end_);
        if (locked.window && !locked.window->evaluate(start).on_time()) {
            result.reason = "committed current delivery is already late";
        }
    }
    if (options_.late_penalty_points > std::numeric_limits<double>::max() / static_cast<double>(orders_.size() + 1)) {
        result.reason = "penalty total could overflow";
    }
    if (result.reason.empty()) {
        try { result = DynamicOrder::insert(graph_, ctx, order); }
        catch (const std::invalid_argument& error) { result.reason = error.what(); }
        catch (const std::overflow_error& error) { result.reason = error.what(); }
    }
    insertions_.push_back({clock_.now(), order.id(), result.accepted, result.reason, ctx.start_time, result.route.nodes});
    if (!result.accepted) {
        record("order_rejected", order.id() + ": " + result.reason);
        return result;  // No vehicle, plan, accepted task or cargo mutation.
    }
    if (!order_mode_ && revisions_.empty()) {
        OrderRoute baseline;
        baseline.nodes = route_.node_sequence; baseline.roads = route_.road_sequence;
        baseline.start_time = route_.departure_time;
        revisions_.push_back({0, std::move(baseline)});
    }
    add_order_record(order, false);
    order_mode_ = true;
    pending_route_ = result.route;
    std::ostringstream description;
    description << order.id() << " anchor=" << SimClock::format(ctx.start_time) << " path=";
    for (std::size_t i = 0; i < result.route.nodes.size(); ++i) {
        if (i) { description << " -> "; }
        description << result.route.nodes[i];
    }
    record("order_accepted", description.str());
    if (vehicle_.status() == VehicleStatus::Idle) {
        apply_pending_route(); enter_node(); settle_instant_events();
    }
    history_.push_back(snapshot());
    return result;
}
void Simulation::apply_order_route(OrderRoute route) {
    if (!vehicle_.position().node_id || *vehicle_.position().node_id != route.nodes.front() ||
        std::abs(clock_.now() - route.start_time) > 1e-8) {
        throw std::logic_error("route activation does not match vehicle anchor");
    }
    route_.node_sequence = route.nodes; route_.road_sequence = route.roads;
    route_.start_node_id = route.nodes.front();
    route_index_ = 0; finished_ = false;
    order_actions_.clear();
    for (const auto& stop : route.stops) { order_actions_.emplace_back(stop.node_index, stop.action); }
    ++revision_;
    timing_ = {};
    double now = route.start_time;
    std::size_t stop = 0;
    for (std::size_t node = 0; node < route.nodes.size(); ++node) {
        timing_.arrivals.push_back(now);
        while (stop < route.stops.size() && route.stops[stop].node_index == node) {
            const auto& action = route.stops[stop++];
            timing_.total_wait_minutes = detail::checked_sum(timing_.total_wait_minutes, action.service_start - action.arrival);
            if (action.action.kind == OrderActionKind::Delivery) {
                timing_.total_unload_minutes = detail::checked_sum(timing_.total_unload_minutes, options_.unload_minutes);
            }
            now = action.departure;
        }
        timing_.departures.push_back(now);
        if (node < route.roads.size()) { now = detail::checked_sum(now, graph_.find_edge_by_id(route.roads[node])->time()); }
    }
    revisions_.push_back({revision_, std::move(route)});
}
bool Simulation::apply_pending_route() {
    if (!pending_route_) { return false; }
    auto next = std::move(*pending_route_); pending_route_.reset();
    apply_order_route(std::move(next));
    record("route_activated", "revision=" + std::to_string(revision_));
    return true;
}
}  // namespace logistics
