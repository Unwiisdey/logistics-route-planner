#include "logistics/simulation/Simulation.h"
#include "logistics/detail/checked_math.hpp"
#include "logistics/visualization/Visualizer.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <stdexcept>
#include <utility>

namespace logistics {
namespace {
double initial_load(const Graph& graph, const RoutePlan& route) {
    double total = 0;
    for (const auto& id : route.requested_delivery_node_ids) {
        const auto* node = graph.find_node(id);
        if (!node) { throw std::invalid_argument("unknown delivery node"); }
        total = detail::checked_sum(total, node->demand());
    }
    return total;
}
std::string csv(const std::string& value) {
    std::string text = "\"";
    for (char c : value) { if (c == '"') { text += '"'; } text += c; }
    return text + '"';
}
std::ofstream open_csv(const std::filesystem::path& file) {
    std::ofstream stream(file, std::ios::binary);
    if (!stream) { throw std::runtime_error("cannot create simulation CSV: " + file.u8string()); }
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<double>::max_digits10);
    return stream;
}
void state_header(std::ostream& out) {
    out << "time,clock,status,node_id,edge_id,from_id,to_id,remaining_minutes,progress,x,y,load,completed_edges,delivered_count,late_count,penalty_points,finished";
}
void state_row(std::ostream& out, const SimulationSnapshot& s) {
    out << s.time << ',' << csv(SimClock::format(s.time)) << ',' << to_string(s.status) << ','
        << csv(s.node_id.value_or("")) << ',' << csv(s.edge_id.value_or("")) << ',' << csv(s.from_id) << ',' << csv(s.to_id)
        << ',' << s.remaining_minutes << ',' << s.progress << ',' << s.x << ',' << s.y << ',' << s.load
        << ',' << s.completed_edges << ',' << s.delivered_count << ',' << s.late_count << ',' << s.penalty_points << ',' << s.finished;
}
}  // namespace

Simulation::Simulation(Graph graph, RoutePlan route, SimulationOptions options)
    : graph_(std::move(graph)), route_(std::move(route)), options_(std::move(options)),
      timing_(Planner::schedule(graph_, route_, options_.unload_minutes)),
      clock_(route_.departure_time, options_.step_minutes),
      vehicle_(options_.vehicle_id, options_.capacity_kg, initial_load(graph_, route_), route_.warehouse_id) {
    if (route_.start_node_id != route_.warehouse_id) {
        throw std::invalid_argument("initial simulation must start at its warehouse");
    }
    if (!std::isfinite(options_.late_penalty_points) || options_.late_penalty_points < 0) {
        throw std::invalid_argument("fixed late penalty must be finite and nonnegative");
    }
    if (!route_.requested_delivery_node_ids.empty() && options_.late_penalty_points >
        std::numeric_limits<double>::max() / static_cast<double>(route_.requested_delivery_node_ids.size())) {
        throw std::overflow_error("total late penalty could overflow");
    }
    for (const auto& stop : route_.stops) {
        if (stop.role != StopRole::Delivery) { continue; }
        const auto* node = graph_.find_node(stop.node_id);
        DeliveryRecord delivery;
        delivery.order_id = "order:" + node->id();
        delivery.node_id = node->id();
        delivery.quantity = node->demand();
        delivery.window = node->time_window();
        delivery_steps_.emplace(stop.route_node_index, deliveries_.size());
        deliveries_.push_back(std::move(delivery));
    }
    record("start");
    enter_node();
    settle_instant_events();
    history_.push_back(snapshot());
}

void Simulation::record(const std::string& kind, const std::string& detail) {
    events_.push_back({snapshot(), kind, detail});
}
void Simulation::enter_node() {
    const auto delivery = delivery_steps_.find(route_index_);
    if (delivery == delivery_steps_.end()) { leave_node(); return; }
    active_delivery_ = delivery->second;
    auto& task = deliveries_[*active_delivery_];
    task.arrival = clock_.now();
    if (task.window && clock_.now() < task.window->earliest) {
        vehicle_.status_ = VehicleStatus::Waiting;
        phase_end_ = task.window->earliest;
        record("wait", task.order_id);
    } else { begin_unloading(); }
}
void Simulation::begin_unloading() {
    auto& task = deliveries_[*active_delivery_];
    task.service_start = clock_.now();
    task.late_minutes = task.window ? std::max(0.0, clock_.now() - task.window->latest) : 0;
    vehicle_.status_ = VehicleStatus::Unloading;
    phase_end_ = detail::checked_sum(clock_.now(), options_.unload_minutes);
    record(task.late_minutes > 0 ? "unload_late" : "unload_on_time", task.order_id);
}
void Simulation::leave_node() {
    if (route_index_ == route_.road_sequence.size()) {
        finished_ = true;
        vehicle_.status_ = VehicleStatus::Idle;
        phase_end_ = clock_.now();
        record("returned");
        return;
    }
    const auto* edge = graph_.find_edge_by_id(route_.road_sequence[route_index_]);
    vehicle_.depart(edge->id(), edge->from_id(), edge->to_id(), edge->time());
    phase_end_ = detail::checked_sum(clock_.now(), edge->time());
    record("depart");
}
void Simulation::finish_phase() {
    if (vehicle_.status() == VehicleStatus::Driving) {
        const auto road = vehicle_.segment()->road_id;
        vehicle_.arrive();
        ++route_index_;
        record("arrive", road);
        enter_node();
    } else if (vehicle_.status() == VehicleStatus::Waiting) {
        begin_unloading();
    } else if (vehicle_.status() == VehicleStatus::Unloading) {
        auto& task = deliveries_[*active_delivery_];
        task.delivered_at = clock_.now();
        task.penalty_points = task.late_minutes > 0 ? options_.late_penalty_points : 0;
        vehicle_.unload(task.quantity);
        vehicle_.status_ = VehicleStatus::Idle;
        record(task.late_minutes > 0 ? "delivered_late" : "delivered_on_time", task.order_id);
        active_delivery_.reset();
        leave_node();
    } else { throw std::logic_error("unfinished simulation has no active phase"); }
}
void Simulation::settle_instant_events() {
    // Each transition consumes a road, a wait or an unloading event: zero-time roads terminate.
    while (!finished_ && phase_end_ == clock_.now()) { finish_phase(); }
}
void Simulation::advance() { advance(clock_.step()); }
void Simulation::advance(Minutes minutes) {
    if (!std::isfinite(minutes) || minutes <= 0) { throw std::invalid_argument("advance must be finite and positive"); }
    if (finished_) { return; }
    const double target = detail::checked_sum(clock_.now(), minutes);
    if (target == clock_.now()) { throw std::overflow_error("step is below numeric precision"); }
    while (!finished_ && clock_.now() < target) {
        const double next = std::min(target, phase_end_);
        clock_.advance(next - clock_.now());
        if (vehicle_.status() == VehicleStatus::Driving) {
            vehicle_.update_remaining(std::min(vehicle_.segment()->duration, std::max(0.0, phase_end_ - clock_.now())));
        }
        if (clock_.now() == phase_end_) { finish_phase(); settle_instant_events(); }
    }
    if (!finished_) { record("tick"); }
    history_.push_back(snapshot());
}

SimulationSnapshot Simulation::snapshot() const {
    SimulationSnapshot state;
    state.time = clock_.now();
    state.status = vehicle_.status();
    state.node_id = vehicle_.position().node_id;
    state.edge_id = vehicle_.position().road_id;
    state.progress = vehicle_.position().edge_progress;
    state.load = vehicle_.current_load();
    state.completed_edges = route_index_;
    state.finished = finished_;
    state.remaining_minutes = state.status == VehicleStatus::Idle ? 0 : std::max(0.0, phase_end_ - clock_.now());
    if (vehicle_.segment()) {
        state.from_id = vehicle_.segment()->from_id;
        state.to_id = vehicle_.segment()->to_id;
        const auto* from = graph_.find_node(state.from_id);
        const auto* to = graph_.find_node(state.to_id);
        state.x = (1 - state.progress) * from->x() + state.progress * to->x();
        state.y = (1 - state.progress) * from->y() + state.progress * to->y();
    } else {
        const auto* node = graph_.find_node(*state.node_id);
        state.x = node->x(); state.y = node->y();
    }
    for (const auto& task : deliveries_) {
        if (task.delivered_at) {
            ++state.delivered_count;
            if (task.late_minutes > 0) { ++state.late_count; }
            state.penalty_points = detail::checked_sum(state.penalty_points, task.penalty_points);
        }
    }
    return state;
}
std::vector<NodeId> Simulation::remaining_deliveries() const {
    std::vector<NodeId> result;
    for (const auto& task : deliveries_) { if (!task.delivered_at) { result.push_back(task.node_id); } }
    return result;
}
void Simulation::write_log(std::ostream& out) const {
    out << std::fixed << std::setprecision(2);
    out << "Vehicle " << vehicle_.id() << " capacity=" << vehicle_.capacity() << " kg; step=" << clock_.step()
        << " min; unload=" << options_.unload_minutes << " min; fixed late penalty=" << options_.late_penalty_points << " points/order\n";
    for (const auto& event : events_) {
        const auto& s = event.state;
        out << '[' << SimClock::format(s.time) << " | " << s.time << " min] " << event.kind
            << " status=" << to_string(s.status) << " location=";
        if (s.node_id) { out << *s.node_id; }
        else { out << s.from_id << "->" << s.to_id << " edge=" << *s.edge_id << " progress=" << s.progress; }
        out << " remaining=" << s.remaining_minutes << " load=" << s.load
            << " delivered=" << s.delivered_count << " late=" << s.late_count << " penalty=" << s.penalty_points;
        if (!event.detail.empty()) { out << " detail=" << event.detail; }
        out << '\n';
    }
    const auto s = snapshot();
    out << "Simulation " << (finished_ ? "complete" : "in progress") << ": delivered=" << s.delivered_count
        << " late=" << s.late_count << " penalty=" << s.penalty_points << " points; finish=" << SimClock::format(s.time)
        << " elapsed=" << s.time - route_.departure_time << " min\n";
}
void Simulation::export_csv(const std::filesystem::path& directory) const {
    Visualizer::export_csv(graph_, route_, directory);
    auto settings = open_csv(directory / "simulation_options.csv");
    settings << "vehicle_id,capacity_kg,start_minutes,step_minutes,unload_minutes,fixed_late_penalty\n"
             << csv(vehicle_.id()) << ',' << vehicle_.capacity() << ',' << route_.departure_time << ',' << clock_.step()
             << ',' << options_.unload_minutes << ',' << options_.late_penalty_points << '\n';
    auto states = open_csv(directory / "simulation.csv");
    state_header(states); states << '\n';
    for (const auto& state : history_) { state_row(states, state); states << '\n'; }
    auto events = open_csv(directory / "events.csv");
    state_header(events); events << ",event,detail\n";
    for (const auto& event : events_) { state_row(events, event.state); events << ',' << csv(event.kind) << ',' << csv(event.detail) << '\n'; }
    auto deliveries = open_csv(directory / "deliveries.csv");
    deliveries << "order_id,node_id,quantity,earliest,latest,arrival,service_start,delivered_at,late_minutes,penalty_points,status\n";
    for (const auto& task : deliveries_) {
        deliveries << csv(task.order_id) << ',' << csv(task.node_id) << ',' << task.quantity << ',';
        if (task.window) { deliveries << task.window->earliest; } deliveries << ',';
        if (task.window) { deliveries << task.window->latest; } deliveries << ',';
        if (task.arrival) { deliveries << *task.arrival; } deliveries << ',';
        if (task.service_start) { deliveries << *task.service_start; } deliveries << ',';
        if (task.delivered_at) { deliveries << *task.delivered_at; }
        deliveries << ',' << task.late_minutes << ',' << task.penalty_points << ','
                   << (task.delivered_at ? (task.late_minutes > 0 ? "late" : "delivered") : "pending") << '\n';
    }
    states.flush(); events.flush(); deliveries.flush(); settings.flush();
}
}  // namespace logistics
