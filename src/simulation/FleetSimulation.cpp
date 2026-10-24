#include "logistics/simulation/FleetSimulation.h"
#include "logistics/detail/checked_math.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace logistics {
namespace {
double score(const OrderRoute& route, RouteStrategy strategy) {
    return strategy == RouteStrategy::ShortestDistance ? route.distance : route.cost;
}
double delivery_start(const OrderRoute& route, const OrderId& id) {
    for (const auto& stop : route.stops) {
        if (stop.action.order_id == id && stop.action.kind == OrderActionKind::Delivery) { return stop.service_start; }
    }
    throw std::logic_error("accepted route omits assigned order");
}
bool safe_id(const std::string& id) {
    return !id.empty() && std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}
std::string csv(const std::string& value) {
    std::string out = "\"";
    for (const auto c : value) { if (c == '"') { out += c; } out += c; }
    return out + '"';
}
}  // namespace

FleetSimulation::FleetSimulation(Graph graph, std::vector<FleetVehicleSpec> vehicles, std::vector<Order> orders,
                                 Minutes start, SimulationOptions options, RouteStrategy strategy)
    : graph_(std::make_shared<Graph>(std::move(graph))), clock_(std::make_shared<SimClock>(start, options.step_minutes)),
      specs_(std::move(vehicles)), options_(std::move(options)), strategy_(strategy) {
    if (specs_.empty()) { throw std::invalid_argument("fleet requires at least one vehicle"); }
    std::set<std::string> ids;
    for (const auto& spec : specs_) {
        const auto* warehouse = graph_->find_node(spec.warehouse);
        if (!safe_id(spec.id) || !ids.insert(spec.id).second || !warehouse || warehouse->type() != NodeType::Warehouse ||
            !std::isfinite(spec.capacity_kg) || spec.capacity_kg < 0) {
            throw std::invalid_argument("invalid/duplicate fleet vehicle, warehouse or capacity");
        }
    }
    std::set<OrderId> order_ids;
    for (const auto& order : orders) {
        if (!order_ids.insert(order.id()).second) { throw std::invalid_argument("duplicate initial order ID"); }
        if (order.release_time() > start) { throw std::invalid_argument("initial order has not been released"); }
    }
    // Urgency, then deadline, then ID: no input-container iteration-order dependence.
    std::sort(orders.begin(), orders.end(), [](const Order& a, const Order& b) {
        if (a.priority() != b.priority()) { return a.priority() == OrderPriority::Urgent; }
        const auto deadline = [](const Order& o) { return o.time_window() ? o.time_window()->latest : std::numeric_limits<double>::infinity(); };
        return std::make_pair(deadline(a), a.id()) < std::make_pair(deadline(b), b.id());
    });
    std::vector<std::vector<Order>> allocated(specs_.size());
    std::vector<OrderRoute> plans(specs_.size());
    for (const auto& order : orders) {
        std::optional<std::size_t> selected;
        InsertionResult best;
        std::tuple<double, double, std::string> best_key;
        std::string failures;
        for (std::size_t i = 0; i < specs_.size(); ++i) {
            if (order.warehouse_id() != specs_[i].warehouse) { failures += specs_[i].id + ": initial cargo at a different warehouse; "; continue; }
            auto candidate_orders = allocated[i]; candidate_orders.push_back(order);
            OrderPlanningContext ctx;
            ctx.start_node = ctx.warehouse = specs_[i].warehouse; ctx.start_time = start;
            ctx.capacity = specs_[i].capacity_kg; ctx.unload_minutes = options_.unload_minutes; ctx.strategy = strategy_;
            ctx.orders = candidate_orders;
            for (const auto& cargo : candidate_orders) { ctx.current_load = detail::checked_sum(ctx.current_load, cargo.quantity()); }
            ShortestPathCache cache(options_.path_cache_capacity);
            auto candidate = DynamicOrder::initial(*graph_, ctx, &cache);
            if (!candidate.accepted) { failures += specs_[i].id + ": " + candidate.reason + "; "; continue; }
            const auto extra = score(candidate.route, strategy_) - score(plans[i], strategy_);
            const auto key = std::make_tuple(order.priority() == OrderPriority::Urgent ? delivery_start(candidate.route, order.id()) : extra,
                                             order.priority() == OrderPriority::Urgent ? extra : candidate.route.finish_time, specs_[i].id);
            if (!selected || key < best_key) { selected = i; best_key = key; best = std::move(candidate); }
        }
        if (selected) {
            allocated[*selected].push_back(order); plans[*selected] = best.route;
            ownership_.emplace(order.id(), specs_[*selected].id);
            assignments_.push_back({start, order.id(), specs_[*selected].id, true, "initial assignment: " + best.reason, best.route.nodes});
        } else { assignments_.push_back({start, order.id(), std::nullopt, false, failures, {}}); }
    }
    for (std::size_t i = 0; i < specs_.size(); ++i) {
        auto vehicle_options = options_; vehicle_options.vehicle_id = specs_[i].id; vehicle_options.capacity_kg = specs_[i].capacity_kg;
        vehicles_.emplace_back(new Simulation(graph_, clock_, specs_[i].warehouse, std::move(allocated[i]), vehicle_options, strategy_));
        vehicles_.back()->prepare_traffic();  // Expose remaining paths even before traffic is enabled.
    }
}
const Simulation& FleetSimulation::vehicle(const std::string& id) const {
    for (const auto& vehicle : vehicles_) { if (vehicle->vehicle().id() == id) { return *vehicle; } }
    throw std::invalid_argument("unknown fleet vehicle: " + id);
}
bool FleetSimulation::finished() const noexcept {
    return std::all_of(vehicles_.begin(), vehicles_.end(), [](const auto& vehicle) { return vehicle->finished(); });
}
FleetAssignment FleetSimulation::insert_order(const Order& order) {
    FleetAssignment assignment{clock_->now(), order.id(), std::nullopt, false, "", {}};
    if (ownership_.count(order.id())) { assignment.reason = "duplicate assigned order ID (including completed orders)"; }
    else {
        std::optional<std::size_t> selected;
        std::tuple<double, double, std::string> best_key;
        for (std::size_t i = 0; i < vehicles_.size(); ++i) {
            const auto& vehicle = *vehicles_[i];
            const auto candidate = vehicle.preview_order(order);
            if (!candidate.accepted) { assignment.reason += specs_[i].id + ": " + candidate.reason + "; "; continue; }
            const auto extra = score(candidate.route, strategy_) - score(vehicle.remaining_order_route(), strategy_);
            const auto key = std::make_tuple(order.priority() == OrderPriority::Urgent ? delivery_start(candidate.route, order.id()) : extra,
                                             order.priority() == OrderPriority::Urgent ? extra : candidate.route.finish_time, specs_[i].id);
            if (!selected || key < best_key) { selected = i; best_key = key; }
        }
        if (selected) {
            auto accepted = vehicles_[*selected]->insert_order(order);
            if (!accepted.accepted) { throw std::logic_error("fleet insertion changed after read-only feasibility check"); }
            assignment.accepted = true; assignment.vehicle_id = specs_[*selected].id;
            assignment.reason = accepted.reason; assignment.path = std::move(accepted.route.nodes);
            ownership_.emplace(order.id(), *assignment.vehicle_id);
        }
    }
    assignments_.push_back(assignment);
    return assignment;
}
void FleetSimulation::enable_traffic(TrafficOptions options) {
    TrafficSimulator candidate(options);
    const auto next = detail::checked_sum(clock_->now(), options.interval_minutes);
    if (next <= clock_->now()) { throw std::invalid_argument("traffic interval below clock precision"); }
    traffic_ = std::move(candidate); next_traffic_ = next;
}
void FleetSimulation::apply_traffic(const std::vector<TrafficUpdate>& updates) {
    const auto changes = TrafficSimulator::validate(*graph_, clock_->now(), updates);
    std::set<RoadId> triggers;
    for (const auto& change : changes) { if (change.triggers_replan) { triggers.insert(change.road_id); } }
    auto candidate = *graph_;
    TrafficSimulator::apply(candidate, clock_->now(), updates);
    std::vector<OrderPlanningContext> contexts;
    std::vector<OrderRoute> old;
    std::vector<ReplanResult> repaired;
    for (const auto& vehicle : vehicles_) {
        contexts.push_back(vehicle->order_context()); old.push_back(vehicle->remaining_order_route());
        repaired.push_back(Replanner::repair(candidate, contexts.back(), old.back(), triggers, &vehicle->path_cache_));
    }
    // All routes have been checked before publishing a single shared graph update.
    *graph_ = std::move(candidate);
    for (std::size_t i = 0; i < vehicles_.size(); ++i) {
        vehicles_[i]->commit_traffic(changes, std::move(repaired[i]), old[i], contexts[i]);
    }
}
void FleetSimulation::advance() { advance(clock_->step()); }
void FleetSimulation::advance(Minutes minutes) {
    if (!std::isfinite(minutes) || minutes <= 0) { throw std::invalid_argument("advance must be finite and positive"); }
    const auto target = detail::checked_sum(clock_->now(), minutes);
    if (target <= clock_->now()) { throw std::overflow_error("step is below clock precision"); }
    // Idle vehicles remain on this timeline and can accept later released orders.
    while (clock_->now() < target) {
        auto next = traffic_ ? std::min(target, next_traffic_) : target;
        for (const auto& vehicle : vehicles_) { if (!vehicle->finished_) { next = std::min(next, vehicle->phase_end_); } }
        clock_->advance(next - clock_->now());
        for (const auto& vehicle : vehicles_) {
            vehicle->update_travel_remaining();
        }
        if (traffic_ && next == next_traffic_) {
            apply_traffic(traffic_->sample(*graph_));
            next_traffic_ = detail::checked_sum(next_traffic_, traffic_->options().interval_minutes);
            if (next_traffic_ <= next) { throw std::overflow_error("traffic interval below clock precision"); }
        }
        // Every vehicle observes road changes before departing at the same timestamp.
        for (const auto& vehicle : vehicles_) {
            if (!vehicle->finished_ && next == vehicle->phase_end_) { vehicle->finish_phase(); vehicle->settle_instant_events(); }
        }
    }
    for (const auto& vehicle : vehicles_) { vehicle->record("fleet_tick"); vehicle->history_.push_back(vehicle->snapshot()); }
}
void FleetSimulation::write_log(std::ostream& out) const {
    for (const auto& assignment : assignments_) {
        out << '[' << SimClock::format(assignment.time) << "] order=" << assignment.order_id
            << " accepted=" << assignment.accepted << " vehicle=" << assignment.vehicle_id.value_or("none")
            << " reason=" << assignment.reason << " path=";
        for (const auto& node : assignment.path) { out << node << ' '; }
        out << '\n';
    }
    std::size_t delivered = 0, late = 0; double penalty = 0;
    for (const auto& vehicle : vehicles_) {
        out << "--- Fleet vehicle " << vehicle->vehicle().id() << " ---\n";
        vehicle->write_log(out);
        const auto snapshot = vehicle->snapshot();
        delivered += snapshot.delivered_count; late += snapshot.late_count;
        penalty = detail::checked_sum(penalty, snapshot.penalty_points);
    }
    out << "Fleet complete: delivered=" << delivered << " late=" << late << " penalty=" << penalty << '\n';
}
void FleetSimulation::export_csv(const std::filesystem::path& directory) const {
    std::filesystem::create_directories(directory);
    for (const auto& vehicle : vehicles_) { vehicle->export_csv(directory / vehicle->vehicle().id()); }
    std::ofstream settings(directory / "traffic_options.csv", std::ios::binary);
    if (!settings) { throw std::runtime_error("cannot create fleet traffic options CSV"); }
    settings.exceptions(std::ios::badbit | std::ios::failbit); settings.imbue(std::locale::classic());
    settings << std::setprecision(std::numeric_limits<double>::max_digits10);
    settings << "random_enabled,interval_minutes,fraction,min_multiplier,max_multiplier,seed\n";
    if (traffic_) {
        const auto& o = traffic_->options();
        settings << "1," << o.interval_minutes << ',' << o.fraction << ',' << o.min_multiplier << ',' << o.max_multiplier << ',' << o.seed << '\n';
    } else { settings << "0,,,,,\n"; }
    settings.flush();
    std::ofstream out(directory / "assignments.csv", std::ios::binary);
    if (!out) { throw std::runtime_error("cannot create fleet assignments CSV"); }
    out.exceptions(std::ios::badbit | std::ios::failbit); out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << "time,order_id,vehicle_id,accepted,reason,path\n";
    for (const auto& assignment : assignments_) {
        std::string path;
        for (const auto& node : assignment.path) { if (!path.empty()) { path += " -> "; } path += node; }
        out << assignment.time << ',' << csv(assignment.order_id) << ',' << csv(assignment.vehicle_id.value_or(""))
            << ',' << assignment.accepted << ',' << csv(assignment.reason) << ',' << csv(path) << '\n';
    }
    out.flush();
}
}  // namespace logistics
