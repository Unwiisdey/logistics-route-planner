#include "test_support.hpp"
#include "logistics/simulation/Simulation.h"
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

using namespace logistics;
namespace {
Graph network() {
    Graph g;
    g.add_node(Node("W", NodeType::Warehouse, 0, 0));
    g.add_node(Node("A", NodeType::DeliveryPoint, 4, 0, 2, TimeWindow{0, 50}));
    g.add_node(Node("B", NodeType::DeliveryPoint, 4, 4, 2, TimeWindow{0, 60}));
    g.add_node(Node("T", NodeType::TransferStation, 7, 2));
    for (const auto& edge : std::vector<Edge>{
        Edge("WA", "W", "A", 1, 4, 1), Edge("AB", "A", "B", 1, 5, 1),
        Edge("BW", "B", "W", 1, 4, 1), Edge("AT", "A", "T", 4, 3, 4),
        Edge("TB", "T", "B", 4, 3, 4), Edge("BT", "B", "T", 5, .5, 5),
        Edge("TW", "T", "W", 5, .5, 5)}) { g.add_edge(edge); }
    return g;
}
SimulationOptions options() {
    SimulationOptions result; result.capacity_kg = 10; result.unload_minutes = 2; return result;
}
std::vector<Order> orders(double earliest = 0) {
    return {Order("a", "W", "A", 2, TimeWindow{earliest, 50}), Order("b", "W", "B", 2, TimeWindow{0, 60})};
}
void finish(Simulation& s, double step = 5) {
    int guard = 0;
    while (!s.finished()) { s.advance(step); check(++guard < 2000, "simulation did not terminate"); }
}
void threshold() {
    check(!TrafficSimulator::significant_increase(10, 11.999), "below threshold");
    check(TrafficSimulator::significant_increase(10, 12), "inclusive threshold");
    check(TrafficSimulator::significant_increase(.1, .12), "decimal threshold");
    check(!TrafficSimulator::significant_increase(12, 12), "no repeated trigger");
    check(!TrafficSimulator::significant_increase(12, 10), "recovery not increase");
    check(TrafficSimulator::significant_increase(0, 1), "zero to positive");
    check(!TrafficSimulator::significant_increase(0, 0), "zero to zero");
    expect_throw<std::invalid_argument>([] { TrafficSimulator::significant_increase(-1, 1); });
}
void sampling() {
    auto g = network(); TrafficSimulator a, b;
    auto first = a.sample(g), same = b.sample(g);
    check(first.size() == 1 && first[0].road_id == same[0].road_id, "ceil 10% and reproducible selection");
    check_near(first[0].time, same[0].time);
    TrafficOptions all; all.fraction = 1; all.min_multiplier = all.max_multiplier = 1.2;
    TrafficSimulator both(all);
    TrafficSimulator::apply(g, .5, both.sample(g));
    for (const auto& node : g.nodes()) {
        for (const auto& edge : g.outgoing_edges(node.first)) { check_near(edge.time(), edge.base_time() * 1.2); }
    }
    TrafficSimulator::apply(g, 1, both.sample(g));
    for (const auto& node : g.nodes()) {
        for (const auto& edge : g.outgoing_edges(node.first)) { check_near(edge.time(), edge.base_time()); }
    }
    g.add_node(Node("Z", NodeType::TransferStation, 8, 8));
    g.add_edge(Edge("zero", "W", "Z", 1, 0, 1));
    g.add_edge(Edge("closed", "Z", "W", 1, 1, 1, TrafficStatus::Closed));
    check(both.sample(g).size() == 7, "zero and closed excluded");
    check(both.sample(Graph{}).empty(), "empty graph");
}
void invalid_batch() {
    auto g = network();
    expect_throw<std::invalid_argument>([&] { TrafficSimulator::apply(g, 0, {{"AB", 10, TrafficStatus::Congested}, {"bad", 1, TrafficStatus::Clear}}); });
    check_near(g.find_edge_by_id("AB")->time(), 5);
    expect_throw<std::invalid_argument>([&] { TrafficSimulator::apply(g, 0, {{"AB", 10, TrafficStatus::Congested}, {"AB", 5, TrafficStatus::Clear}}); });
    expect_throw<std::invalid_argument>([&] { TrafficSimulator::apply(g, 0, {{"AB", -1, TrafficStatus::Congested}}); });
    expect_throw<std::invalid_argument>([&] { TrafficSimulator::apply(g, 0, {{"AB", 1, TrafficStatus::Closed}}); });
    TrafficOptions invalid; invalid.interval_minutes = 0;
    expect_throw<std::invalid_argument>([&] { TrafficSimulator sim(invalid); });
    invalid.interval_minutes = .5; invalid.fraction = std::numeric_limits<double>::quiet_NaN();
    expect_throw<std::invalid_argument>([&] { TrafficSimulator sim(invalid); });
    Simulation sim(g, "W", orders(), 0, options()); const auto events = sim.events().size();
    expect_throw<std::invalid_argument>([&] { sim.apply_traffic({{"AB", -1, TrafficStatus::Congested}}); });
    check(sim.events().size() == events && sim.traffic_changes().empty(), "invalid batch has no side effects");
}
void local_repair() {
    Simulation sim(network(), "W", orders(), 0, options()); sim.advance(1);
    const auto position = sim.snapshot();
    sim.apply_traffic({{"AB", 10, TrafficStatus::Congested}});
    const auto& r = sim.replanning_records().back();
    check(r.before == std::vector<NodeId>({"A", "B", "W"}), "original remaining route");
    check(r.after == std::vector<NodeId>({"A", "T", "B", "W"}), "affected leg replaced, faster unrelated BW detour NOT searched");
    check(r.searched_legs == 1, "one affected leg search");
    check_near(sim.snapshot().progress, position.progress); check_near(sim.vehicle().segment()->remaining, 3);
    check_near(*sim.snapshot().next_arrival, 4); check_near(*sim.snapshot().estimated_return, 18);
    sim.advance(3); check(sim.snapshot().node_id == "A", "activate at actual anchor");
    check(sim.route_revisions().back().route.roads == std::vector<RoadId>({"AT", "TB", "BW"}), "retain untouched return");
    finish(sim); check_near(sim.clock().now(), 18); check(sim.snapshot().delivered_count == 2, "all delivered");
}
void unrelated_and_current() {
    Simulation sim(network(), "W", orders(), 0, options()); sim.advance(1);
    sim.apply_traffic({{"WA", 40, TrafficStatus::Congested}, {"BT", 20, TrafficStatus::Congested}});
    check(sim.replanning_records().back().searched_legs == 0, "no future route intersection");
    check_near(sim.vehicle().segment()->remaining, 3);
    check(sim.route_revisions().size() == 1, "no irrelevant revision");
    finish(sim); check_near(sim.clock().now(), 17); check_near(*sim.deliveries()[0].arrival, 4);
}
void recovery_and_small_change() {
    Simulation sim(network(), "W", orders(), 0, options()); sim.advance(1);
    sim.apply_traffic({{"AB", 5.9, TrafficStatus::Congested}});
    check(sim.replanning_records().empty(), "subthreshold only updates ETA");
    check_near(*sim.snapshot().estimated_return, 17.9);
    sim.apply_traffic({{"AB", 5, TrafficStatus::Clear}});
    check_near(*sim.snapshot().estimated_return, 17);
    finish(sim); check_near(sim.clock().now(), 17);
}
void waiting_anchor() {
    Simulation sim(network(), "W", orders(10), 0, options()); sim.advance(5);
    check(sim.vehicle().status() == VehicleStatus::Waiting, "waiting for window");
    sim.apply_traffic({{"AB", 10, TrafficStatus::Congested}});
    check_near(sim.replanning_records().back().anchor_time, 12);
    sim.advance(6); check(sim.vehicle().status() == VehicleStatus::Unloading, "service not interrupted");
    sim.apply_traffic({{"AT", 6, TrafficStatus::Congested}});
    check_near(sim.replanning_records().back().anchor_time, 12);
    finish(sim); check_near(*sim.deliveries()[0].delivered_at, 12); check(sim.snapshot().delivered_count == 2, "no duplicate service");
}
void unavoidable_late() {
    auto g = network(); g.remove_edge("A", "T");
    Simulation sim(g, "W", orders(), 0, options()); sim.advance(1);
    sim.apply_traffic({{"AB", 100.25, TrafficStatus::Congested}});
    check(sim.replanning_records().back().predicted_late == std::vector<OrderId>{"b"}, "late forecast reported");
    check(!sim.insert_order(DynamicOrder::urgent("new", "W", "A", 1, 1, 200)).accepted, "cannot accept while old deadline fails");
    finish(sim); check(sim.snapshot().late_count == 1, "one late order");
    check_near(sim.deliveries()[1].late_minutes, 46.25);
    check_near(sim.snapshot().penalty_points, 47); // Ceil actual lateness after an accepted order is delayed.
}
void pending_insertion() {
    Simulation sim(network(), "W", {Order("a", "W", "A", 2, TimeWindow{0, 500}),
                                    Order("b", "W", "B", 2, TimeWindow{0, 600})}, 0, options()); sim.advance(1);
    check(sim.insert_order(DynamicOrder::urgent("new", "W", "B", 1, 1, 200)).accepted, "insert before traffic");
    sim.apply_traffic({{"AB", 10, TrafficStatus::Congested}, {"BW", 8, TrafficStatus::Congested}});
    const auto second = sim.insert_order(DynamicOrder::urgent("second", "W", "A", 1, 1, 200));
    check(second.accepted, "insert after traffic: " + second.reason);
    sim.apply_traffic({{"AB", 12, TrafficStatus::Congested}});
    finish(sim); check(sim.snapshot().delivered_count == 4, "pending orders retained");
    for (const auto& task : sim.deliveries()) { check(task.delivered_at.has_value(), "every task completed"); }
    check_near(sim.vehicle().current_load(), 0);
}
void step_invariance() {
    Simulation a(network(), "W", orders(), 0, options()), b(network(), "W", orders(), 0, options());
    a.enable_traffic(); b.enable_traffic(); finish(a, .25); finish(b, 10);
    check(a.traffic_changes().size() == b.traffic_changes().size(), "same traffic count");
    for (std::size_t i = 0; i < a.traffic_changes().size(); ++i) {
        const auto& x = a.traffic_changes()[i]; const auto& y = b.traffic_changes()[i];
        check_near(x.at, .5 * (i + 1)); check_near(x.at, y.at); check_near(x.after, y.after);
        check(x.road_id == y.road_id, "same random road");
    }
    check_near(a.clock().now(), b.clock().now());
    for (std::size_t i = 0; i < a.deliveries().size(); ++i) { check_near(*a.deliveries()[i].arrival, *b.deliveries()[i].arrival); }
    a.wait_until(100); check_near(a.clock().now(), 100); check_near(a.traffic_changes().back().at, 100);
    check(a.insert_order(DynamicOrder::urgent("restart", "W", "A", 1, 100)).accepted, "idle traffic permits restart");
    finish(a);
}
void legacy_and_export() {
    const auto g = network(); PlanningRequest request; request.warehouse_id = "W";
    Simulation sim(g, Planner::plan(g, request), options()); sim.advance(1);
    sim.apply_traffic({{"AB", 10, TrafficStatus::Congested}}); finish(sim);
    check(sim.snapshot().delivered_count == 2, "legacy converted safely");
    check_near(sim.route_revisions().front().route.stops.front().arrival, 4);
    check_near(sim.route_revisions().front().route.stops.front().departure, 6);
    const auto directory = std::filesystem::path("test_artifacts/traffic"); sim.export_csv(directory);
    for (const auto& name : {"nodes.csv", "edges.csv", "traffic.csv", "replans.csv", "remaining_routes.csv", "route_revisions.csv"}) {
        check(std::filesystem::file_size(directory / name) > 20, "traffic export missing");
    }
    std::ostringstream log; sim.write_log(log);
    check(log.str().find("before=A -> B -> W after=A -> T -> B -> W") != std::string::npos, "route comparison log");
    check(log.str().find("return_eta=") != std::string::npos, "ETA log");
}
}  // namespace
int main(int argc, char* argv[]) {
    return run_tests(argc, argv, {{"threshold", threshold}, {"sampling", sampling}, {"invalid_batch", invalid_batch},
        {"local_repair", local_repair}, {"unrelated_and_current", unrelated_and_current},
        {"recovery_and_small_change", recovery_and_small_change}, {"waiting_anchor", waiting_anchor},
        {"unavoidable_late", unavoidable_late}, {"pending_insertion", pending_insertion},
        {"step_invariance", step_invariance}, {"legacy_and_export", legacy_and_export}});
}
