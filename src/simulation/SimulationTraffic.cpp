#include "logistics/simulation/Simulation.h"
#include "logistics/detail/checked_math.hpp"
#include "logistics/visualization/Visualizer.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace logistics {
namespace {
std::string join(const std::vector<std::string>& values) {
    std::string result;
    for (const auto& value : values) { if (!result.empty()) { result += " -> "; } result += value; }
    return result;
}
std::string csv(const std::string& value) {
    std::string result = "\"";
    for (char c : value) { if (c == '"') { result += '"'; } result += c; }
    return result + '"';
}
std::ofstream file(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) { throw std::runtime_error("cannot create traffic CSV: " + path.u8string()); }
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out.imbue(std::locale::classic()); out << std::setprecision(std::numeric_limits<double>::max_digits10);
    return out;
}
}  // namespace
OrderRoute Simulation::remaining_order_route() const {
    if (pending_route_) { return *pending_route_; }
    const auto ctx = order_context();
    const auto start = route_index_ + (vehicle_.segment() ? 1 : 0);
    OrderRoute result;
    result.start_time = ctx.start_time;
    result.nodes.assign(route_.node_sequence.begin() + static_cast<std::ptrdiff_t>(start), route_.node_sequence.end());
    result.roads.assign(route_.road_sequence.begin() + static_cast<std::ptrdiff_t>(start), route_.road_sequence.end());
    for (const auto& action : order_actions_) {
        if (action.first < start) { throw std::logic_error("remaining action precedes anchor"); }
        result.stops.push_back({action.second, action.first - start});
    }
    return result;
}
void Simulation::prepare_traffic() {
    if (initial_traffic_graph_) { return; }
    initial_traffic_graph_ = *graph_;
    if (!order_mode_) {
        OrderRoute baseline;
        baseline.nodes = route_.node_sequence; baseline.roads = route_.road_sequence;
        baseline.start_time = route_.departure_time;
        baseline.finish_time = timing_.departures.back();
        baseline.travel = route_.total_travel_minutes;
        baseline.distance = route_.total_distance_km; baseline.cost = route_.total_cost_yuan;
        double load = 0;
        for (const auto& delivery : deliveries_) { load = detail::checked_sum(load, delivery.quantity); }
        for (const auto& entry : delivery_steps_) {
            const OrderAction action{deliveries_[entry.second].order_id, OrderActionKind::Delivery};
            load = std::max(0.0, load - deliveries_[entry.second].quantity);
            baseline.stops.push_back({action, entry.first, timing_.arrivals[entry.first],
                timing_.departures[entry.first] - options_.unload_minutes, timing_.departures[entry.first], load});
            if (!deliveries_[entry.second].delivered_at && (!active_delivery_ || entry.second != *active_delivery_)) {
                order_actions_.emplace_back(entry.first, action);
            }
        }
        revisions_.push_back({revision_, std::move(baseline)});
        order_mode_ = true;
    }
    forecast_ = Replanner::repair(*graph_, order_context(), remaining_order_route(), {}).route;
    record("traffic_enabled", "current road/service locked; affected legs optimize travel time");
    history_.push_back(snapshot());
}
void Simulation::enable_traffic(TrafficOptions options) {
    TrafficSimulator candidate(options);
    const auto next = detail::checked_sum(clock_->now(), options.interval_minutes);
    if (next == clock_->now()) { throw std::invalid_argument("traffic interval below clock precision"); }
    prepare_traffic();
    traffic_ = std::move(candidate); next_traffic_ = next;
}
void Simulation::traffic_tick() {
    apply_traffic(traffic_->sample(*graph_));
    const auto next = detail::checked_sum(next_traffic_, traffic_->options().interval_minutes);
    if (next <= next_traffic_) { throw std::overflow_error("traffic interval below clock precision"); }
    next_traffic_ = next;
}
void Simulation::apply_traffic(const std::vector<TrafficUpdate>& updates) {
    const auto changes = TrafficSimulator::validate(*graph_, clock_->now(), updates);
    prepare_traffic();
    const auto ctx = order_context();
    const auto old = remaining_order_route();
    std::set<RoadId> triggers;
    for (const auto& change : changes) {
        if (change.triggers_replan) { triggers.insert(change.road_id); }
    }
    TrafficSimulator::apply(*graph_, clock_->now(), updates);
    ReplanResult repaired;
    try { repaired = Replanner::repair(*graph_, ctx, old, triggers, &path_cache_); }
    catch (...) {
        // Roll back only the changed edges if evaluating finite route totals fails.
        for (const auto& change : changes) {
            const auto* edge = graph_->find_edge_by_id(change.road_id);
            const auto from = edge->from_id(), to = edge->to_id();
            graph_->update_edge_traffic(from, to, change.before, change.before_status);
        }
        throw;
    }
    commit_traffic(changes, std::move(repaired), old, ctx);
}
void Simulation::commit_traffic(const std::vector<TrafficChange>& changes, ReplanResult repaired,
                                const OrderRoute& old, const OrderPlanningContext& ctx) {
    std::set<RoadId> triggers;
    bool relevant = false;
    for (const auto& change : changes) {
        if (change.triggers_replan) { triggers.insert(change.road_id); }
        if (change.before != change.after && std::find(old.roads.begin(), old.roads.end(), change.road_id) != old.roads.end()) { relevant = true; }
    }
    forecast_ = repaired.route;
    if (relevant) { pending_route_ = repaired.route; }
    for (const auto& change : changes) {
        traffic_changes_.push_back(change);
        std::ostringstream detail;
        detail << change.road_id << " " << change.before << " -> " << change.after << " min "
               << to_string(change.after_status) << " trigger=" << change.triggers_replan;
        record("traffic_change", detail.str());
    }
    if (!triggers.empty()) {
        std::vector<std::string> ids(triggers.begin(), triggers.end());
        const auto reason = "time increase >=20% vs previous value: " + join(ids) +
            (repaired.searched_legs ? "; affected legs searched by time" : "; no affected future leg (current road locked)");
        replanning_records_.push_back({clock_->now(), ctx.start_time, reason, old.nodes, repaired.route.nodes,
                                      repaired.searched_legs, repaired.route.finish_time, repaired.predicted_late});
        record("replan", reason + " searched_legs=" + std::to_string(repaired.searched_legs) +
               " before=" + join(old.nodes) + " after=" + join(repaired.route.nodes));
    }
    if (!repaired.predicted_late.empty()) { record("window_risk", join(repaired.predicted_late)); }
    if (vehicle_.status() == VehicleStatus::Idle && pending_route_) {
        apply_pending_route(); enter_node(); settle_instant_events();
    }
    history_.push_back(snapshot());
}
void Simulation::export_traffic(const std::filesystem::path& directory) const {
    if (!initial_traffic_graph_) { return; }
    Visualizer::export_network_csv(*initial_traffic_graph_, directory);
    auto traffic = file(directory / "traffic.csv");
    traffic << "time,edge_id,before,after,before_status,after_status,trigger\n";
    for (const auto& c : traffic_changes_) {
        traffic << c.at << ',' << csv(c.road_id) << ',' << c.before << ',' << c.after << ','
                << to_string(c.before_status) << ',' << to_string(c.after_status) << ',' << c.triggers_replan << '\n';
    }
    auto replans = file(directory / "replans.csv");
    replans << "time,anchor_time,reason,before,after,searched_legs,estimated_return,predicted_late\n";
    for (const auto& r : replanning_records_) {
        replans << r.time << ',' << r.anchor_time << ',' << csv(r.reason) << ',' << csv(join(r.before)) << ','
                << csv(join(r.after)) << ',' << r.searched_legs << ',' << r.estimated_return << ',' << csv(join(r.predicted_late)) << '\n';
    }
    auto settings = file(directory / "traffic_options.csv");
    settings << "random_enabled,interval_minutes,fraction,min_multiplier,max_multiplier,seed\n";
    if (traffic_) {
        const auto& o = traffic_->options();
        settings << "1," << o.interval_minutes << ',' << o.fraction << ',' << o.min_multiplier << ',' << o.max_multiplier << ',' << o.seed << '\n';
    } else { settings << "0,,,,,\n"; }
    auto future = file(directory / "remaining_routes.csv");
    future << "frame,step,node_id,incoming_edge_id\n";
    for (std::size_t frame = 0; frame < history_.size(); ++frame) {
        const auto& s = history_[frame];
        // Snapshots from before enable_traffic still have a valid active route revision.
        for (std::size_t step = 0; step < s.future_nodes.size(); ++step) {
            future << frame << ',' << step << ',' << csv(s.future_nodes[step]) << ','
                   << csv(step ? s.future_roads[step - 1] : "") << '\n';
        }
    }
    auto traversals = file(directory / "traversals.csv");
    traversals << "time,edge_id\n";
    for (const auto& traversal : traversals_) { traversals << traversal.first << ',' << csv(traversal.second) << '\n'; }
    traffic.flush(); replans.flush(); settings.flush(); future.flush(); traversals.flush();
}
}  // namespace logistics
