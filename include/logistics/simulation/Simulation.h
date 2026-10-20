#pragma once
#include "logistics/planning/Planner.h"
#include "logistics/simulation/SimClock.h"
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
};
struct DeliveryRecord {
    OrderId order_id;  // Stage 4: one synthetic order per requested delivery node.
    NodeId node_id;
    double quantity{0.0};
    std::optional<TimeWindow> window;
    std::optional<Minutes> arrival;
    std::optional<Minutes> service_start;
    std::optional<Minutes> delivered_at;
    Minutes late_minutes{0.0};
    double penalty_points{0.0};
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
};
struct SimulationEvent {
    SimulationSnapshot state;
    std::string kind;
    std::string detail;
};

class Simulation {
public:
    // Capture an immutable network/route snapshot; initial load includes every requested task.
    Simulation(Graph graph, RoutePlan route, SimulationOptions options = {});
    void advance();
    void advance(Minutes minutes);
    bool finished() const noexcept { return finished_; }
    const SimClock& clock() const noexcept { return clock_; }
    const Vehicle& vehicle() const noexcept { return vehicle_; }
    const RouteTiming& expected_timing() const noexcept { return timing_; }
    const std::vector<DeliveryRecord>& deliveries() const noexcept { return deliveries_; }
    const std::vector<SimulationEvent>& events() const noexcept { return events_; }
    const std::vector<SimulationSnapshot>& history() const noexcept { return history_; }
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
    Graph graph_;
    RoutePlan route_;
    SimulationOptions options_;
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
};
}  // namespace logistics
