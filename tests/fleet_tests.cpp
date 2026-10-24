#include "test_support.hpp"
#include "logistics/simulation/FleetSimulation.h"
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
using namespace logistics;
namespace {
Graph network() {
    Graph g;
    g.add_node(Node("W", NodeType::Warehouse, 0, 0));
    g.add_node(Node("A", NodeType::DeliveryPoint, 2, 0));
    g.add_node(Node("B", NodeType::DeliveryPoint, 0, 1));
    g.add_node(Node("T", NodeType::TransferStation, 1, 0));
    for (const auto& edge : std::vector<Edge>{Edge("WA", "W", "A", 1, 20, 1), Edge("WT", "W", "T", 3, 5, 3),
            Edge("TA", "T", "A", 3, 5, 3), Edge("AW", "A", "W", 1, 1, 1),
            Edge("WB", "W", "B", 1, 1, 1), Edge("BW", "B", "W", 1, 1, 1)}) { g.add_edge(edge); }
    return g;
}
SimulationOptions options() { SimulationOptions o; o.capacity_kg = 5; o.unload_minutes = 2; return o; }
std::vector<FleetVehicleSpec> specs() { return {{"V01", "W", 5}, {"V02", "W", 5}}; }
std::vector<Order> orders() { return {Order("a", "W", "A", 4, TimeWindow{480, 600}), Order("b", "W", "B", 4, TimeWindow{480, 600})}; }
void finish(FleetSimulation& fleet, double step = 1) {
    int ticks = 0;
    while (!fleet.finished()) { fleet.advance(step); check(++ticks < 2000, "fleet must finish"); }
}
void fallback() {
    const auto g = network();
    for (const auto strategy : {RouteStrategy::ShortestDistance, RouteStrategy::LowestCost}) {
        const Order tight("tight", "W", "A", 1, TimeWindow{480, 495});
        Simulation initial(g, "W", {tight}, 480, options(), strategy);
        check(initial.route_revisions().front().route.time_fallback, "initial must try faster roads");
        check(initial.vehicle().segment()->road_id == "WT", "longer but faster detour");
        while (!initial.finished()) { initial.advance(1); }
        check_near(*initial.deliveries()[0].service_start, 490);
        Simulation dynamic(g, "W", {}, 480, options(), strategy);
        const auto before = dynamic.events().size();
        const auto preview = dynamic.preview_order(tight);
        check(preview.accepted && preview.route.time_fallback, "dynamic preview falls back");
        check(dynamic.events().size() == before && dynamic.deliveries().empty(), "preview is read only");
        check(dynamic.insert_order(tight).accepted, "dynamic fallback accepted");
        while (!dynamic.finished()) { dynamic.advance(1); }
        check_near(*dynamic.deliveries()[0].service_start, 490);
        OrderPlanningContext ctx;
        ctx.start_node = ctx.warehouse = "W"; ctx.start_time = 480; ctx.capacity = 5;
        ctx.current_load = 1; ctx.unload_minutes = 2; ctx.strategy = strategy;
        ctx.orders = {Order("old", "W", "B", 1, TimeWindow{480, 482})};
        ctx.loaded = {"old"}; ctx.incumbent = {{"old", OrderActionKind::Delivery}};
        const auto preserved = DynamicOrder::insert(g, ctx, tight);
        check(preserved.accepted && preserved.route.time_fallback, "fallback preserves existing narrow deadline");
        for (const auto& stop : preserved.route.stops) {
            if (stop.action.order_id == "old") { check_near(stop.service_start, 481); }
        }
        check(!DynamicOrder::insert(g, ctx, Order("too_soon", "W", "A", 1, TimeWindow{480, 490})).accepted,
              "faster route cannot violate an old deadline to accept new order");
        check(!DynamicOrder::insert(g, ctx, Order("overload", "W", "A", 6, TimeWindow{480, 495})).accepted,
              "faster route cannot bypass capacity checks");
        Simulation impossible(g, "W", {}, 480, options(), strategy);
        check(!impossible.insert_order(Order("bad", "W", "A", 1, TimeWindow{480, 489})).accepted, "even fastest misses deadline");
    }
}
void preferred_paths() {
    Simulation sim(network(), "W", {Order("a", "W", "A", 1, TimeWindow{480, 600})}, 480, options());
    check(!sim.route_revisions().front().route.time_fallback, "keep preferred feasible route");
    check(sim.vehicle().segment()->road_id == "WA", "do not use faster expensive road unnecessarily");
}
void shared_runtime() {
    FleetSimulation fleet(network(), specs(), orders(), 480, options());
    check(&fleet.vehicle("V01").network() == &fleet.network() && &fleet.vehicle("V02").network() == &fleet.network(), "actual shared graph object");
    check(&fleet.vehicle("V01").clock() == &fleet.clock() && &fleet.vehicle("V02").clock() == &fleet.clock(), "actual shared clock object");
    fleet.advance(.5);
    check_near(fleet.clock().now(), 480.5);
    check_near(fleet.vehicle("V01").vehicle().segment()->remaining, 19.5);
    check_near(fleet.vehicle("V02").vehicle().segment()->remaining, .5);
    fleet.advance(5);
    check(fleet.vehicle("V02").finished(), "early return is idle on common timeline");
    check_near(fleet.vehicle("V02").clock().now(), 485.5);
    check_near(*fleet.vehicle("V02").completed_at(), 484);
    check_near(*fleet.vehicle("V02").snapshot().estimated_return, 484);
}
void separate_warehouses() {
    auto g = network(); g.add_node(Node("W2", NodeType::Warehouse, 0, 2));
    g.add_edge(Edge("W2B", "W2", "B", 1, 1, 1)); g.add_edge(Edge("BW2", "B", "W2", 1, 1, 1));
    auto initial = orders(); initial[1] = Order("b", "W2", "B", 4, TimeWindow{480, 600});
    FleetSimulation fleet(g, {{"V01", "W", 5}, {"V02", "W2", 7}}, initial, 480, options());
    check(fleet.ownership().at("a") == "V01" && fleet.ownership().at("b") == "V02", "cargo allocated at its actual warehouse");
    finish(fleet);
    check(fleet.vehicle("V01").vehicle().position().node_id == "W" && fleet.vehicle("V02").vehicle().position().node_id == "W2", "return to each home warehouse");
}
void ownership() {
    FleetSimulation fleet(network(), specs(), orders(), 480, options());
    check(fleet.ownership().at("a") == "V01" && fleet.ownership().at("b") == "V02", "capacity splits initial cargo");
    const auto original = fleet.ownership();
    finish(fleet);
    check(fleet.ownership() == original, "ownership retained after delivery");
    check(!fleet.insert_order(orders()[0]).accepted, "completed ID cannot be reassigned");
    check_near(fleet.vehicle("V01").vehicle().current_load(), 0);
    check_near(fleet.vehicle("V02").vehicle().current_load(), 0);
}
void urgent_assignment() {
    FleetSimulation fleet(network(), specs(), orders(), 480, options());
    fleet.advance(1);
    const auto rejected_events = fleet.vehicle("V01").events().size();
    const auto assignment = fleet.insert_order(Order("u", "W", "B", 1, TimeWindow{481, 488}, OrderPriority::Urgent, 481));
    check(assignment.accepted && assignment.vehicle_id == "V02", "urgent goes to feasible earlier vehicle");
    check(fleet.vehicle("V01").events().size() == rejected_events && fleet.vehicle("V01").deliveries().size() == 1, "losing trial has no mutation");
    finish(fleet);
    check(fleet.vehicle("V02").snapshot().delivered_count == 2, "winner delivered emergency and original");
    check(fleet.ownership().at("a") == "V01" && fleet.ownership().at("b") == "V02", "no existing task migration");
    FleetSimulation both(network(), specs(), orders(), 480, options()); both.advance(1);
    const Order flexible("flexible", "W", "B", 1, TimeWindow{481, 600}, OrderPriority::Urgent, 481);
    check(both.vehicle("V01").preview_order(flexible).accepted && both.vehicle("V02").preview_order(flexible).accepted, "both feasible");
    check(both.insert_order(flexible).vehicle_id == "V02", "earliest delivery wins when both vehicles can accept");
    FleetSimulation tie(network(), specs(), {}, 480, options());
    check(tie.insert_order(Order("t", "W", "B", 1, TimeWindow{480, 600}, OrderPriority::Urgent)).vehicle_id == "V01", "stable ID tie break");
}
void rejection() {
    FleetSimulation fleet(network(), specs(), orders(), 480, options());
    fleet.advance(1);
    const auto before = fleet.ownership();
    const auto r1 = fleet.vehicle("V01").route_revisions().size(), r2 = fleet.vehicle("V02").route_revisions().size();
    const auto result = fleet.insert_order(Order("huge", "W", "B", 6, TimeWindow{481, 600}, OrderPriority::Urgent, 481));
    check(!result.accepted && result.reason.find("V01") != std::string::npos && result.reason.find("V02") != std::string::npos, "both failures reported");
    check(fleet.ownership() == before && fleet.vehicle("V01").route_revisions().size() == r1 && fleet.vehicle("V02").route_revisions().size() == r2, "rejection is atomic");
    check(!fleet.insert_order(Order("future", "W", "B", 1, TimeWindow{490, 600}, OrderPriority::Urgent, 490)).accepted, "future release rejected");
    check(!fleet.insert_order(Order("late", "W", "B", 1, TimeWindow{481, 481}, OrderPriority::Urgent, 481)).accepted, "deadline impossible for both");
}
void traffic() {
    FleetSimulation fleet(network(), specs(), orders(), 480, options());
    const auto owners = fleet.ownership();
    fleet.advance(.5);
    fleet.apply_traffic({{"WA", 40, TrafficStatus::Congested}, {"BW", 8, TrafficStatus::Congested}});
    check_near(fleet.network().find_edge_by_id("WA")->time(), 40);
    check_near(fleet.vehicle("V01").vehicle().segment()->duration, 20); // Entered before change.
    check_near(fleet.vehicle("V02").vehicle().segment()->duration, 1);
    check(fleet.vehicle("V01").traffic_changes().size() == 2 && fleet.vehicle("V02").traffic_changes().size() == 2, "one batch broadcast to both");
    check(fleet.vehicle("V02").replanning_records().back().searched_legs == 1, "affected future leg recomputed");
    fleet.apply_traffic({{"BW", 1, TrafficStatus::Clear}});
    check(fleet.ownership() == owners, "traffic cannot migrate cargo");
    const auto old_time = fleet.network().find_edge_by_id("WA")->time();
    expect_throw<std::invalid_argument>([&] { fleet.apply_traffic({{"WA", 60, TrafficStatus::Congested}, {"missing", 2, TrafficStatus::Clear}}); });
    check_near(fleet.network().find_edge_by_id("WA")->time(), old_time);
    finish(fleet);
    check(fleet.vehicle("V01").snapshot().delivered_count + fleet.vehicle("V02").snapshot().delivered_count == 2, "both finish under traffic");
    TrafficOptions o; o.seed = 9; TrafficSimulator expected(o); auto g = network();
    const auto batch = expected.sample(g); TrafficSimulator::apply(g, 480.5, batch);
    FleetSimulation sampled(network(), specs(), {}, 480, options()); sampled.enable_traffic(o); sampled.advance(.5);
    for (const auto& update : batch) { check_near(sampled.network().find_edge_by_id(update.road_id)->time(), g.find_edge_by_id(update.road_id)->time()); }
    check(sampled.vehicle("V01").traffic_changes().size() == batch.size() && sampled.vehicle("V02").traffic_changes().size() == batch.size(), "sample once, publish once to both");
}
void boundary() {
    FleetSimulation fleet(network(), specs(), orders(), 480, options());
    fleet.advance(1); // V02 starts unloading, V01 still driving.
    const auto assigned = fleet.insert_order(Order("same-time", "W", "B", 1, TimeWindow{481, 488}, OrderPriority::Urgent, 481));
    check(assigned.accepted, "insert while another vehicle unloads");
    finish(fleet);
    fleet.advance(100);
    const auto restarted = fleet.insert_order(Order("restart", "W", "A", 1, TimeWindow{fleet.clock().now(), fleet.clock().now()+15}, OrderPriority::Urgent, fleet.clock().now()));
    check(restarted.accepted, "finished fleet can restart and use fastest fallback");
    finish(fleet);
}
void zero_time() {
    auto g = network();
    g.update_edge_traffic("W", "A", 0, TrafficStatus::Clear); g.update_edge_traffic("A", "W", 0, TrafficStatus::Clear);
    g.update_edge_traffic("W", "B", 0, TrafficStatus::Clear); g.update_edge_traffic("B", "W", 0, TrafficStatus::Clear);
    auto o = options(); o.unload_minutes = 0;
    FleetSimulation fleet(g, specs(), orders(), 480, o);
    check(fleet.finished(), "all zero-time events settle during initialization");
    check_near(fleet.clock().now(), 480);
    check(fleet.vehicle("V01").snapshot().delivered_count + fleet.vehicle("V02").snapshot().delivered_count == 2, "simultaneous completion");
    fleet.advance(.5); check_near(fleet.clock().now(), 480.5);
}
void step_consistency() {
    std::optional<Minutes> arrival;
    std::optional<std::map<OrderId, std::string>> owners;
    for (const auto cache_capacity : {std::size_t{0}, std::size_t{256}}) {
      for (const auto step : {.1, 1., 5., 100.}) {
        auto o = options(); o.path_cache_capacity = cache_capacity;
        FleetSimulation fleet(network(), specs(), orders(), 480, o);
        TrafficOptions traffic_options; traffic_options.seed = 17;
        fleet.enable_traffic(traffic_options);
        fleet.advance(1);
        const auto assignment = fleet.insert_order(Order("u", "W", "B", 1, TimeWindow{481, 600}, OrderPriority::Urgent, 481));
        check(assignment.accepted, "urgent under random traffic");
        finish(fleet, step);
        const auto& delivery = fleet.vehicle(fleet.ownership().at("u")).deliveries().back();
        if (arrival) { check_near(*delivery.service_start, *arrival); check(fleet.ownership() == *owners, "step independent assignment"); }
        else { arrival = delivery.service_start; owners = fleet.ownership(); }
        check_near(fleet.vehicle("V01").clock().now(), fleet.vehicle("V02").clock().now());
        if (cache_capacity == 0) {
            check(fleet.vehicle("V01").path_cache_stats().hits == 0 && fleet.vehicle("V02").path_cache_stats().hits == 0, "disabled cache stays disabled");
        }
      }
    }
}
void invalid_inputs() {
    expect_throw<std::invalid_argument>([] { FleetSimulation f(network(), {}, {}, 480, options()); });
    expect_throw<std::invalid_argument>([] { FleetSimulation f(network(), {{"V01", "W", 5}, {"V01", "W", 5}}, {}, 480, options()); });
    expect_throw<std::invalid_argument>([] { FleetSimulation f(network(), {{"../bad", "W", 5}}, {}, 480, options()); });
    expect_throw<std::invalid_argument>([] { FleetSimulation f(network(), {{"V01", "A", 5}}, {}, 480, options()); });
    expect_throw<std::invalid_argument>([] { FleetSimulation f(network(), specs(), {orders()[0], orders()[0]}, 480, options()); });
    FleetSimulation fleet(network(), specs(), {}, 480, options());
    expect_throw<std::invalid_argument>([&] { fleet.advance(0); });
    expect_throw<std::invalid_argument>([&] { fleet.advance(std::numeric_limits<double>::quiet_NaN()); });
    expect_throw<std::invalid_argument>([&] { fleet.vehicle("missing"); });
    const Order impossible("too_big", "W", "A", 11, TimeWindow{480, 600});
    FleetSimulation rejected(network(), specs(), {impossible}, 480, options());
    check(rejected.ownership().empty() && !rejected.assignments()[0].accepted, "infeasible initial order explicitly reported");
}
void export_files() {
    FleetSimulation fleet(network(), specs(), orders(), 480, options()); fleet.enable_traffic(); finish(fleet);
    const std::filesystem::path directory = "test_artifacts/fleet_export";
    fleet.export_csv(directory);
    check(std::filesystem::exists(directory / "V01" / "deliveries.csv") && std::filesystem::exists(directory / "V02" / "deliveries.csv"), "per vehicle export");
    std::ifstream settings(directory / "traffic_options.csv"); std::stringstream options_text; options_text << settings.rdbuf();
    check(options_text.str().find("1,0.5,") != std::string::npos, "fleet stores central traffic parameters");
    std::ifstream input(directory / "assignments.csv"); std::stringstream text; text << input.rdbuf();
    check(text.str().find("vehicle_id,accepted") != std::string::npos && text.str().find("V01") != std::string::npos && text.str().find("V02") != std::string::npos, "assignment CSV");
    std::ostringstream log; fleet.write_log(log);
    check(log.str().find("Fleet complete: delivered=2 late=0") != std::string::npos, "aggregate completion");
}
}  // namespace
int main(int argc, char* argv[]) {
    return run_tests(argc, argv, {{"fallback", fallback}, {"preferred_paths", preferred_paths}, {"shared_runtime", [] { shared_runtime(); separate_warehouses(); }},
        {"ownership", ownership}, {"urgent_assignment", urgent_assignment}, {"rejection", rejection}, {"traffic", traffic},
        {"boundary", [] { boundary(); zero_time(); }}, {"step_consistency", step_consistency}, {"invalid_inputs", invalid_inputs}, {"export", export_files}});
}
