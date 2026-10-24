#include "test_support.hpp"
#include "logistics/config/ConfigLoader.h"
#include "logistics/config/configuration.hpp"
#include "logistics/simulation/Simulation.h"
#include "logistics/visualization/network_view.hpp"
#include <filesystem>
#include <fstream>
#include <limits>
#include <cmath>
#include <sstream>

using namespace logistics;
namespace {
Graph network() {
    Graph graph;
    graph.add_node(Node("W", NodeType::Warehouse, 0, 0));
    graph.add_node(Node("A", NodeType::DeliveryPoint, 10, 0, 3, TimeWindow{485, 485}));
    graph.add_node(Node("B", NodeType::DeliveryPoint, 10, 10, 4, TimeWindow{480, 489}));
    graph.add_edge(Edge("WA", "W", "A", 1, 2.5, 2));
    graph.add_edge(Edge("AB", "A", "B", 1, 3, 2));
    graph.add_edge(Edge("BW", "B", "W", 1, 4.5, 2));
    return graph;
}
RoutePlan route(const Graph& graph, double departure = 480) {
    PlanningRequest request;
    request.warehouse_id = "W"; request.departure_time = departure;
    return Planner::plan(graph, request);
}
void finish(Simulation& run) {
    std::size_t steps = 0;
    while (!run.finished()) { run.advance(); check(++steps < 10000, "simulation did not terminate"); }
}
void clock_test() {
    SimClock clock;
    check(clock.format() == "08:00", "default time");
    clock.advance(); check_near(clock.now(), 485);
    clock.advance(90); check(clock.format() == "09:35", "formatted time");
    check(SimClock::format(1440) == "D+1 00:00", "day rollover");
    check(SimClock::format(575.9) == "09:35", "display truncates fractional minute");
    expect_throw<std::invalid_argument>([] { SimClock bad(0, 0); });
    expect_throw<std::invalid_argument>([] { SimClock bad(-1, 1); });
    expect_throw<std::invalid_argument>([&] { clock.advance(-1); });
    expect_throw<std::invalid_argument>([&] { clock.advance(std::numeric_limits<double>::infinity()); });
    check_near(clock.now(), 575);
}
void vehicle_test() {
    Vehicle vehicle("V", 7, 7, "W");
    check(vehicle.status() == VehicleStatus::Idle && vehicle.position().node_id == "W", "initialized vehicle");
    check(!vehicle.segment() && !vehicle.position().road_id, "node and edge positions exclusive");
    expect_throw<std::invalid_argument>([] { Vehicle bad("V", 6, 7, "W"); });
    expect_throw<std::invalid_argument>([] { Vehicle bad("", 10, 0, "W"); });
    expect_throw<std::invalid_argument>([] { Vehicle bad("V", 10, -1, "W"); });
}
void motion_test() {
    const auto graph = network(); Simulation run(graph, route(graph));
    check(run.vehicle().status() == VehicleStatus::Driving, "departs at start");
    run.advance(1);
    auto s = run.snapshot();
    check(!s.node_id && s.edge_id == "WA", "moving is not located at a node");
    check_near(s.progress, .4); check_near(s.x, 4); check_near(s.y, 0); check_near(s.remaining_minutes, 1.5);
    check_near(run.vehicle().segment()->remaining, 1.5); check_near(s.load, 7);
    run.advance(1.5);
    s = run.snapshot();
    check(s.node_id == "A" && !s.edge_id && s.status == VehicleStatus::Waiting, "arrival enters waiting");
    check_near(s.remaining_minutes, 2.5);
    run.advance(2.5);
    check(run.vehicle().status() == VehicleStatus::Unloading, "earliest starts unloading");
    check_near(run.vehicle().current_load(), 7);
    run.advance(2);
    check_near(run.vehicle().current_load(), 4);
    check(run.snapshot().edge_id == "AB", "next road starts after unloading");
    check(run.remaining_deliveries() == std::vector<NodeId>{"B"}, "remaining task interface");
}
void windows_test() {
    const auto graph = network(); Simulation run(graph, route(graph)); finish(run);
    const auto& a = run.deliveries()[0]; const auto& b = run.deliveries()[1];
    check_near(*a.arrival, 482.5); check_near(*a.service_start, 485); check_near(*a.delivered_at, 487);
    check_near(a.penalty_points, 0); // Starting exactly at deadline is on time, even if unloading ends later.
    check_near(*b.arrival, 490); check_near(b.late_minutes, 1); check_near(b.penalty_points, 1);
    check_near(run.clock().now(), 496.5); check_near(run.vehicle().current_load(), 0);
    check(run.vehicle().position().node_id == "W" && run.vehicle().status() == VehicleStatus::Idle, "returns to warehouse");
    const auto& timing = run.expected_timing();
    check_near(timing.total_wait_minutes, 2.5); check_near(timing.total_unload_minutes, 4);
    check_near(timing.arrivals.back(), run.clock().now());
    const auto events = run.events().size(); run.advance(100);
    check(run.events().size() == events && run.remaining_deliveries().empty(), "completed simulation is stable");
}
void penalty_test() {
    const auto graph = network(); SimulationOptions options; options.late_penalty_per_minute = 7;
    Simulation first(graph, route(graph), options); finish(first);
    Simulation later(graph, route(graph, 600), options); finish(later);
    check_near(first.deliveries()[1].penalty_points, 7);
    check_near(later.deliveries()[0].late_minutes, 117.5);
    check_near(later.deliveries()[0].penalty_points, 118 * 7);
    check_near(later.deliveries()[1].penalty_points, 119 * 7);
    check(later.deliveries()[1].late_minutes > first.deliveries()[1].late_minutes, "different lateness durations");
    check_near(later.snapshot().penalty_points, (118 + 119) * 7);
    Simulation fractional(graph, route(graph, 483.25), options); finish(fractional);
    check_near(fractional.deliveries()[0].late_minutes, .75);
    check_near(fractional.deliveries()[0].penalty_points, 7);
    check_near(fractional.deliveries()[1].late_minutes, 1.75);
    check_near(fractional.deliveries()[1].penalty_points, 14);
    const auto paid = fractional.snapshot().penalty_points;
    fractional.advance(100); check_near(fractional.snapshot().penalty_points, paid);
    options.late_penalty_per_minute = 0;
    Simulation free(graph, route(graph), options); finish(free);
    check(free.snapshot().late_count == 1, "zero charge still records late order"); check_near(free.snapshot().penalty_points, 0);

    Graph extreme;
    extreme.add_node(Node("W", NodeType::Warehouse, 0, 0));
    extreme.add_node(Node("A", NodeType::DeliveryPoint, 1, 0, 0, TimeWindow{0, 0}));
    extreme.add_edge(Edge("WA", "W", "A", 1, 2, 1));
    extreme.add_edge(Edge("AW", "A", "W", 1, 1, 1));
    options.unload_minutes = 0; options.late_penalty_per_minute = std::numeric_limits<double>::max();
    Simulation overflow(extreme, route(extreme, 0), options);
    expect_throw<std::overflow_error>([&] { overflow.advance(2); });
    check(!overflow.deliveries()[0].delivered_at && overflow.snapshot().delivered_count == 0,
          "overflow must not mark a delivery as completed");
    check_near(overflow.snapshot().penalty_points, 0);

    extreme.remove_edge("W", "A"); extreme.add_edge(Edge("WA2", "W", "A", 1, 1, 1));
    extreme.add_node(Node("B", NodeType::DeliveryPoint, 2, 0, 0, TimeWindow{0, 0}));
    extreme.add_edge(Edge("AB", "A", "B", 1, 0, 1));
    extreme.add_edge(Edge("BW", "B", "W", 1, 1, 1));
    options.late_penalty_per_minute = std::numeric_limits<double>::max() * .75;
    Simulation total_overflow(extreme, route(extreme, 0), options);
    expect_throw<std::overflow_error>([&] { total_overflow.advance(1); });
    check(total_overflow.snapshot().delivered_count == 1 && !total_overflow.deliveries()[1].delivered_at,
          "accumulated overflow must not commit the next delivery");
    check(std::isfinite(total_overflow.snapshot().penalty_points), "recorded total remains finite");
}
void step_test() {
    const auto graph = network();
    for (const double step : {.1, .5, 1., 5., 100.}) {
        SimulationOptions options; options.step_minutes = step;
        Simulation run(graph, route(graph), options); finish(run);
        check_near(run.clock().now(), 496.5); check_near(*run.deliveries()[1].arrival, 490);
        check_near(run.snapshot().penalty_points, 1); check(run.snapshot().completed_edges == 3, "all edges consumed");
        Simulation fractional(graph, route(graph, 483.25), options); finish(fractional);
        check_near(fractional.snapshot().penalty_points, 3); // ceil(.75) + ceil(1.75), every step size.
    }
}
void zero_test() {
    Graph graph; graph.add_node(Node("W", NodeType::Warehouse, 0, 0));
    Simulation empty(graph, route(graph)); check(empty.finished(), "empty route immediately completes");
    graph.add_node(Node("A", NodeType::DeliveryPoint, 1, 0, 0));
    graph.add_edge(Edge("WA", "W", "A", 0, 0, 0)); graph.add_edge(Edge("AW", "A", "W", 0, 0, 0));
    SimulationOptions options; options.unload_minutes = 0;
    Simulation zero(graph, route(graph), options);
    check(zero.finished() && zero.deliveries().size() == 1, "zero phases terminate during initialization");
    check_near(*zero.deliveries()[0].delivered_at, 480); check_near(zero.snapshot().penalty_points, 0);
}
void repeated_test() {
    Graph graph; graph.add_node(Node("W", NodeType::Warehouse, 0, 0));
    graph.add_node(Node("A", NodeType::DeliveryPoint, 1, 0, 3));
    graph.add_node(Node("B", NodeType::DeliveryPoint, 2, 0, 4));
    graph.add_edge(Edge("WA", "W", "A", 1, 1, 1)); graph.add_edge(Edge("AW", "A", "W", 1, 1, 1));
    graph.add_edge(Edge("AB", "A", "B", 1, 1, 1)); graph.add_edge(Edge("BA", "B", "A", 1, 1, 1));
    Simulation run(graph, route(graph)); finish(run);
    check(run.snapshot().completed_edges == 4 && run.snapshot().delivered_count == 2, "passing A again does not unload twice");
    check_near(run.clock().now(), 488); check_near(run.vehicle().current_load(), 0);
}
void invalid_test() {
    const auto graph = network(); const auto plan = route(graph);
    SimulationOptions options; options.capacity_kg = 6;
    expect_throw<std::invalid_argument>([&] { Simulation bad(graph, plan, options); });
    options.capacity_kg = 7; options.step_minutes = 0;
    expect_throw<std::invalid_argument>([&] { Simulation bad(graph, plan, options); });
    options.step_minutes = 1; options.unload_minutes = -1;
    expect_throw<std::invalid_argument>([&] { Simulation bad(graph, plan, options); });
    options.unload_minutes = 0; options.late_penalty_per_minute = std::numeric_limits<double>::quiet_NaN();
    expect_throw<std::invalid_argument>([&] { Simulation bad(graph, plan, options); });
    auto broken = plan; broken.road_sequence[0] = "missing";
    expect_throw<std::invalid_argument>([&] { Simulation bad(graph, broken); });
    broken = plan; broken.stops[1].route_node_index = 0;
    expect_throw<std::invalid_argument>([&] { Simulation bad(graph, broken); });
    broken = plan; broken.stops.front().node_id = "A";
    expect_throw<std::invalid_argument>([&] { Simulation bad(graph, broken); });
    auto changed = graph; changed.update_edge_traffic("W", "A", 7, TrafficStatus::Congested);
    expect_throw<std::invalid_argument>([&] { Simulation bad(changed, plan); }, "stale");
    changed.update_edge_traffic("W", "A", 2.5, TrafficStatus::Closed);
    expect_throw<std::invalid_argument>([&] { Simulation bad(changed, plan); }, "closed");
    Simulation run(graph, plan);
    expect_throw<std::invalid_argument>([&] { run.advance(0); }); check_near(run.clock().now(), 480);
}
void export_test() {
    auto graph = network(); Simulation run(graph, route(graph));
    graph.remove_node("A"); // Running simulation owns its network snapshot.
    run.advance(1);
    const auto directory = std::filesystem::path("test_artifacts/simulation");
    run.export_csv(directory);
    std::ifstream options_file(directory / "simulation_options.csv");
    std::string header; std::getline(options_file, header);
    check(header.find("late_penalty_per_minute,penalty_rounding") != std::string::npos, "export names rate and rounding");
    for (const auto& name : {"simulation.csv", "events.csv", "deliveries.csv", "nodes.csv", "route.csv"}) {
        check(std::filesystem::file_size(directory / name) > 50, "CSV file missing/empty");
    }
    finish(run);
    std::ostringstream log; run.write_log(log);
    check(log.str().find("delivered_late") != std::string::npos && log.str().find("Simulation complete") != std::string::npos, "log events and completion");
}
void demo_test() {
    const auto graph = ConfigLoader::load_network("config/network.json");
    PlanningRequest request; request.warehouse_id = "W01"; request.departure_time = 480;
    for (const auto strategy : {RouteStrategy::ShortestDistance, RouteStrategy::LowestCost}) {
        request.strategy = strategy;
        Simulation run(graph, Planner::plan(graph, request)); finish(run);
        check(run.snapshot().delivered_count == 25, "demo delivery coverage");
        check_near(run.clock().now(), run.expected_timing().arrivals.back());
        check_near(run.vehicle().current_load(), 0);
        double expected = 0;
        for (const auto& delivery : run.deliveries()) { expected += std::ceil(delivery.late_minutes); }
        check_near(run.snapshot().penalty_points, expected);
    }
}
}  // namespace
int main(int argc, char* argv[]) {
    return run_tests(argc, argv, {{"clock", clock_test}, {"vehicle", vehicle_test}, {"motion", motion_test},
        {"windows", windows_test}, {"minute_penalty", penalty_test}, {"step_invariance", step_test},
        {"zero_time_and_empty", zero_test}, {"repeated_nodes", repeated_test}, {"invalid_inputs", invalid_test},
        {"export_and_snapshot", export_test}, {"demo_network", demo_test}});
}
