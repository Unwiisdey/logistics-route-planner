#pragma once
#include "logistics/simulation/Simulation.h"

namespace logistics {
struct FleetVehicleSpec {
    std::string id;
    NodeId warehouse;
    double capacity_kg{1000};
};
struct FleetAssignment {
    Minutes time;
    OrderId order_id;
    std::optional<std::string> vehicle_id;
    bool accepted;
    std::string reason;
    std::vector<NodeId> path;
};

// One logical timeline and one mutable road network, independently owned cargo/routes.
// Discrete-event execution is sequential and deterministic, not one thread per vehicle.
class FleetSimulation {
public:
    FleetSimulation(Graph graph, std::vector<FleetVehicleSpec> vehicles, std::vector<Order> initial_orders,
                    Minutes start = 480, SimulationOptions options = {},
                    RouteStrategy strategy = RouteStrategy::ShortestDistance);
    FleetAssignment insert_order(const Order& order);
    void enable_traffic(TrafficOptions options = {});
    void apply_traffic(const std::vector<TrafficUpdate>& updates);
    void advance();
    void advance(Minutes minutes);
    bool finished() const noexcept;
    const Graph& network() const noexcept { return *graph_; }
    const SimClock& clock() const noexcept { return *clock_; }
    const Simulation& vehicle(const std::string& id) const;
    const std::vector<FleetVehicleSpec>& vehicle_specs() const noexcept { return specs_; }
    const std::map<OrderId, std::string>& ownership() const noexcept { return ownership_; }
    const std::vector<FleetAssignment>& assignments() const noexcept { return assignments_; }
    std::vector<OrderId> waiting_pickups(const NodeId& pickup_node) const;
    void write_log(std::ostream& output) const;
    void export_csv(const std::filesystem::path& directory) const;
private:
    std::shared_ptr<Graph> graph_;
    std::shared_ptr<SimClock> clock_;
    std::vector<FleetVehicleSpec> specs_;
    SimulationOptions options_;
    RouteStrategy strategy_;
    std::vector<std::unique_ptr<Simulation>> vehicles_;
    std::map<OrderId, std::string> ownership_;
    std::vector<FleetAssignment> assignments_;
    std::optional<TrafficSimulator> traffic_;
    Minutes next_traffic_{0};
};
}  // namespace logistics
