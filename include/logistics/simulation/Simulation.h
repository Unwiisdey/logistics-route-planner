#pragma once
#include "logistics/planning/Planner.h"
#include "logistics/simulation/SimClock.h"
#include "logistics/planning/DynamicOrder.h"
#include "logistics/planning/Replanner.h"
#include "logistics/traffic/TrafficSimulator.h"
#include <deque>
#include <filesystem>
#include <iosfwd>
#include <map>

namespace logistics {
struct SimulationOptions {
    Minutes step_minutes{5.0};
    Minutes unload_minutes{2.0};
    double late_penalty_points{10.0};  // Fixed charge per late delivery, NOT per minute.
    std::string vehicle_id{"V01"};
    double capacity_kg{1000.0};
    std::size_t path_cache_capacity{256};
};
struct DeliveryRecord {
    OrderId order_id;
    NodeId node_id;
    double quantity{0.0};
    std::optional<TimeWindow> window;
    std::optional<Minutes> arrival;
    std::optional<Minutes> service_start;
    std::optional<Minutes> delivered_at;
    Minutes late_minutes{0.0};
    double penalty_points{0.0};
    NodeId warehouse_id;
    OrderPriority priority{OrderPriority::Normal};
    bool loaded{true};
    std::optional<Minutes> picked_up_at;
};
struct SimulationSnapshot {
    Minutes time{0.0};
    VehicleStatus status{VehicleStatus::Idle};
    std::optional<NodeId> node_id;
    std::optional<RoadId> edge_id;
    NodeId from_id;
    NodeId to_id;
    Minutes remaining_minutes{0.0};  // Current travel, waiting or unloading phase.
    double progress{0.0};
    double x{0.0};
    double y{0.0};
    double load{0.0};
    std::size_t completed_edges{0};
    std::size_t delivered_count{0};
    std::size_t late_count{0};
    double penalty_points{0.0};
    bool finished{false};
    std::size_t route_revision{0};
    std::optional<Minutes> estimated_return;
    std::optional<Minutes> next_arrival;
    OrderId next_order;
    std::size_t traffic_version{0};
    std::size_t completed_traversals{0};
    std::vector<NodeId> future_nodes;
    std::vector<RoadId> future_roads;
};
struct SimulationEvent {
    SimulationSnapshot state;
    std::string kind;
    std::string detail;
};
struct OrderInsertionRecord {
    Minutes time;
    OrderId order_id;
    bool accepted;
    std::string reason;
    Minutes anchor_time;
    std::vector<NodeId> new_path;
};
struct RouteRevision {
    std::size_t id;
    OrderRoute route;
};
struct ReplanningRecord {
    Minutes time;
    Minutes anchor_time;
    std::string reason;
    std::vector<NodeId> before;
    std::vector<NodeId> after;
    std::size_t searched_legs;
    Minutes estimated_return;
    std::vector<OrderId> predicted_late;
};

class Simulation {
public:
    // Own a network/route copy; later traffic changes enter only through Simulation.
    Simulation(Graph graph, RoutePlan route, SimulationOptions options = {});
    // Strict initial planning with independent order IDs (including several orders at one node).
    Simulation(Graph graph, const NodeId& warehouse, std::vector<Order> orders, Minutes start,
               SimulationOptions options = {}, RouteStrategy strategy = RouteStrategy::ShortestDistance);
    InsertionResult insert_order(const Order& order);
    void enable_traffic(TrafficOptions options = {});
    void apply_traffic(const std::vector<TrafficUpdate>& updates);
    const Graph& network() const noexcept { return graph_; }
    const PathCacheStats& path_cache_stats() const noexcept { return path_cache_.stats(); }
    const std::vector<TrafficChange>& traffic_changes() const noexcept { return traffic_changes_; }
    const std::vector<ReplanningRecord>& replanning_records() const noexcept { return replanning_records_; }
    void advance();
    void advance(Minutes minutes);
    void wait_until(Minutes time);  // Only after returning, for future released orders.
    bool finished() const noexcept { return finished_; }
    const SimClock& clock() const noexcept { return clock_; }
    const Vehicle& vehicle() const noexcept { return vehicle_; }
    const RouteTiming& expected_timing() const noexcept { return timing_; }
    const std::vector<DeliveryRecord>& deliveries() const noexcept { return deliveries_; }
    const std::vector<SimulationEvent>& events() const noexcept { return events_; }
    const std::vector<SimulationSnapshot>& history() const noexcept { return history_; }
    const std::vector<OrderInsertionRecord>& insertions() const noexcept { return insertions_; }
    const std::vector<RouteRevision>& route_revisions() const noexcept { return revisions_; }
    SimulationSnapshot snapshot() const;
    std::vector<NodeId> remaining_deliveries() const;
    void write_log(std::ostream& output) const;
    // Also exports the static route/network CSVs understood by plot_network.py.
    void export_csv(const std::filesystem::path& directory) const;
private:
    void enter_node();
    void leave_node();
    void begin_unloading();
    void finish_phase();
    void settle_instant_events();
    void record(const std::string& kind, const std::string& detail = "");
    OrderPlanningContext order_context() const;
    void apply_order_route(OrderRoute route);
    bool apply_pending_route();
    std::size_t order_index(const OrderId& id) const;
    void add_order_record(const Order& order, bool loaded);
    void prepare_traffic();
    void traffic_tick();
    OrderRoute remaining_order_route() const;
    void export_traffic(const std::filesystem::path& directory) const;
    Graph graph_;
    RoutePlan route_;
    SimulationOptions options_;
    ShortestPathCache path_cache_;
    RouteTiming timing_;
    SimClock clock_;
    Vehicle vehicle_;
    std::vector<DeliveryRecord> deliveries_;
    std::map<std::size_t, std::size_t> delivery_steps_;
    std::optional<std::size_t> active_delivery_;
    std::size_t route_index_{0};
    Minutes phase_end_{0.0};
    bool finished_{false};
    std::vector<SimulationEvent> events_;
    std::vector<SimulationSnapshot> history_;
    std::vector<Order> orders_;
    bool order_mode_{false};
    std::deque<std::pair<std::size_t, OrderAction>> order_actions_;
    std::optional<OrderRoute> pending_route_;
    std::size_t revision_{0};
    std::vector<RouteRevision> revisions_;
    std::vector<OrderInsertionRecord> insertions_;
    std::optional<TrafficSimulator> traffic_;
    Minutes next_traffic_{0};
    std::optional<Graph> initial_traffic_graph_;
    std::optional<OrderRoute> forecast_;
    std::vector<TrafficChange> traffic_changes_;
    std::vector<ReplanningRecord> replanning_records_;
    std::vector<std::pair<Minutes, RoadId>> traversals_;
};
}  // namespace logistics
