#include "test_support.hpp"
#include "logistics/config/OrderConfigLoader.h"
#include "logistics/config/ConfigLoader.h"
#include "logistics/simulation/Simulation.h"
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

using namespace logistics;
namespace {
Graph graph() {
    Graph g;
    g.add_node(Node("W", NodeType::Warehouse, 0, 0));
    g.add_node(Node("A", NodeType::DeliveryPoint, 1, 0, 4));
    g.add_node(Node("B", NodeType::DeliveryPoint, 0, 1, 3));
    g.add_node(Node("C", NodeType::DeliveryPoint, 5, 5, 1)); // Deliberately unreachable.
    for (const auto& from : {"W", "A", "B"}) {
        for (const auto& to : {"W", "A", "B"}) {
            if (std::string(from) != to) { g.add_edge(Edge(std::string(from) + to, from, to, 5, 5, 5)); }
        }
    }
    return g;
}
Order old_order(std::optional<TimeWindow> window = TimeWindow{480, 600}) {
    return Order("old", "W", "A", 4, window);
}
SimulationOptions options() {
    SimulationOptions value; value.capacity_kg = 10; value.unload_minutes = 2;
    return value;
}
void finish(Simulation& sim) {
    int steps = 0;
    while (!sim.finished()) { sim.advance(); check(++steps < 1000, "simulation failed to finish"); }
}
OrderPlanningContext context() {
    OrderPlanningContext ctx;
    ctx.start_node = "W"; ctx.warehouse = "W"; ctx.start_time = 480;
    ctx.capacity = 10; ctx.current_load = 4; ctx.unload_minutes = 2;
    ctx.orders = {old_order()}; ctx.loaded = {"old"};
    ctx.incumbent = {{"old", OrderActionKind::Delivery}};
    return ctx;
}
void windows() {
    TimeWindow window{540, 660};
    auto early = window.evaluate(539.25); check_near(early.waiting_minutes, .75); check(early.on_time(), "early waits");
    check(window.evaluate(660).on_time(), "deadline inclusive");
    check_near(window.evaluate(660.25).late_minutes, .25);
    check_near(window.fixed_penalty(660.25, 10), 10);
    check_near(window.fixed_penalty(800, 10), 10);
    check_near(TimeWindow{1430, 1450}.evaluate(1440).service_start, 1440);
    check(TimeWindow{0, .3}.evaluate(.1 + .2).on_time(), "decimal roundoff is not a late order");
    check(!TimeWindow{0, .3}.evaluate(.30000001).on_time(), "real fractional lateness is preserved");
    expect_throw<std::invalid_argument>([] { TimeWindow{10, 1}.evaluate(5); });
    expect_throw<std::invalid_argument>([&] { window.evaluate(std::numeric_limits<double>::quiet_NaN()); });
}
void order_values() {
    const auto urgent = DynamicOrder::urgent("u", "W", "B", 3, 480);
    check(urgent.priority() == OrderPriority::Urgent, "urgent priority");
    check_near(urgent.time_window()->latest, 540);
    expect_throw<std::invalid_argument>([] { Order bad("", "W", "A", 1, std::nullopt); });
    expect_throw<std::invalid_argument>([] { Order bad("x", "W", "A", -1, std::nullopt); });
    expect_throw<std::invalid_argument>([] { Order bad("x", "W", "A", 1, TimeWindow{1, 2}, OrderPriority::Normal, 3); });
}
void strict_initial() {
    const auto g = graph();
    Simulation sim(g, "W", {Order("a", "W", "A", 2, TimeWindow{500, 500}),
                            Order("b", "W", "A", 2, TimeWindow{502, 510})}, 480, options());
    finish(sim);
    check(sim.deliveries().size() == 2 && sim.snapshot().delivered_count == 2, "two distinct same-node orders");
    check_near(*sim.deliveries()[0].service_start, 500);
    check_near(*sim.deliveries()[1].service_start, 502);
    check_near(sim.vehicle().current_load(), 0);
    auto other = graph();
    other.add_node(Node("T", NodeType::TransferStation, 2, 1));
    other.add_node(Node("W2", NodeType::Warehouse, 3, 1));
    for (const auto& edge : std::vector<Edge>{Edge("BT", "B", "T", 1, 1, 1), Edge("TB", "T", "B", 1, 1, 1),
                                             Edge("TW2", "T", "W2", 1, 1, 1), Edge("W2T", "W2", "T", 1, 1, 1)}) {
        other.add_edge(edge);
    }
    Simulation second(other, "W", {old_order()}, 480, options()); second.advance(1);
    check(second.insert_order(DynamicOrder::urgent("remote", "W2", "B", 2, 481)).accepted, "other warehouse via transfer accepted");
    finish(second);
    bool picked_up = false;
    for (const auto& event : second.events()) {
        if (event.kind == "pickup") { picked_up = event.state.node_id == "W2"; }
    }
    check(picked_up && second.vehicle().position().node_id == "W", "physical pickup at second warehouse and return home");
    expect_throw<std::invalid_argument>([&] { Simulation bad(g, "W", {old_order(TimeWindow{480, 484})}, 480, options()); });
    expect_throw<std::invalid_argument>([&] { Simulation bad(g, "W", {Order("x", "W", "A", 11, std::nullopt)}, 480, options()); });
    auto fractional = options(); fractional.capacity_kg = .3;
    Simulation decimals(g, "W", {Order("x", "W", "A", .1, std::nullopt), Order("y", "W", "A", .2, std::nullopt)}, 480, fractional);
    finish(decimals); check_near(decimals.vehicle().current_load(), 0);
}
void urgent_priority() {
    const auto result = DynamicOrder::insert(graph(), context(), DynamicOrder::urgent("u", "W", "B", 3, 480));
    check(result.accepted, result.reason);
    check(result.route.stops[0].action.kind == OrderActionKind::Pickup, "pickup before delivery");
    check(result.route.stops[1].action.order_id == "u", "urgent served before flexible incumbent");
    check_near(result.route.stops[1].service_start, 485);
    check_near(result.route.stops[0].load_after, 7);
}
void capacity_after_unload() {
    auto ctx = context(); ctx.capacity = 5;
    const auto result = DynamicOrder::insert(graph(), ctx, DynamicOrder::urgent("u", "W", "B", 3, 480));
    check(result.accepted, result.reason);
    check(result.route.stops[0].action.order_id == "old", "must unload old cargo before pickup");
    for (const auto& stop : result.route.stops) { check(stop.load_after <= 5, "capacity throughout route"); }
    check(!DynamicOrder::insert(graph(), ctx, DynamicOrder::urgent("huge", "W", "B", 6, 480)).accepted, "oversized rejected");
}
void preserve_existing_windows() {
    auto ctx = context(); ctx.orders = {old_order(TimeWindow{480, 486})};
    const auto result = DynamicOrder::insert(graph(), ctx, DynamicOrder::urgent("u", "W", "B", 3, 480, 10));
    check(!result.accepted, "urgent cannot sacrifice incumbent deadline");
    check(!DynamicOrder::insert(graph(), context(), DynamicOrder::urgent("u", "W", "C", 3, 480)).accepted, "unreachable rejected");
    auto sequence = ctx.incumbent; sequence.clear();
    check(!DynamicOrder::evaluate(graph(), ctx, sequence).accepted, "accepted tasks cannot disappear");
}
void moving_insertion() {
    Simulation sim(graph(), "W", {old_order()}, 480, options());
    sim.advance(1);
    const auto before = sim.snapshot();
    const auto result = sim.insert_order(DynamicOrder::urgent("u", "W", "B", 3, 481));
    check(result.accepted, result.reason);
    check_near(sim.snapshot().progress, before.progress); check_near(sim.vehicle().current_load(), 4);
    check(sim.snapshot().edge_id == before.edge_id, "does not teleport or reverse current road");
    check(!sim.deliveries()[1].picked_up_at, "cargo not loaded on acceptance");
    sim.advance(4);
    check(sim.snapshot().route_revision == 2, "activate at road endpoint");
    sim.advance(5);
    check_near(*sim.deliveries()[1].picked_up_at, 490);
    check_near(sim.vehicle().current_load(), 7);
    finish(sim);
    check_near(sim.snapshot().penalty_points, 0); check(sim.snapshot().delivered_count == 2, "both tasks delivered");
    check_near(sim.vehicle().current_load(), 0);
}
void locked_service() {
    for (const bool waiting : {false, true}) {
        Simulation sim(graph(), "W", {old_order(waiting ? TimeWindow{500, 600} : TimeWindow{480, 600})}, 480, options());
        sim.advance(6);
        const auto prior = sim.snapshot();
        auto result = sim.insert_order(DynamicOrder::urgent("u", "W", "B", 3, 486));
        check(result.accepted, result.reason);
        check(sim.snapshot().status == prior.status && sim.snapshot().node_id == "A", "service is not interrupted");
        check_near(result.route.start_time, waiting ? 502 : 487);
        finish(sim);
        check_near(*sim.deliveries()[0].arrival, 485);
        check_near(*sim.deliveries()[0].delivered_at, waiting ? 502 : 487);
        check(sim.snapshot().delivered_count == 2, "service not duplicated");
    }
}
void multiple_pending() {
    Simulation sim(graph(), "W", {old_order()}, 480, options());
    sim.advance(1);
    check(sim.insert_order(DynamicOrder::urgent("u1", "W", "B", 2, 481)).accepted, "first accepted");
    sim.advance(1);
    check(sim.insert_order(DynamicOrder::urgent("u2", "W", "B", 2, 482)).accepted, "second before anchor accepted");
    check_near(sim.vehicle().current_load(), 4);
    finish(sim);
    check(sim.snapshot().delivered_count == 3, "pending replacement keeps all accepted tasks");
    for (const auto& d : sim.deliveries()) { check(d.delivered_at && d.late_minutes == 0, "all complete on time"); }
    check(!sim.insert_order(DynamicOrder::urgent("u1", "W", "B", 2, sim.clock().now())).accepted, "completed IDs cannot be reused");
}
void atomic_rejection() {
    Simulation sim(graph(), "W", {old_order()}, 480, options());
    sim.advance(1);
    const auto before = sim.snapshot();
    const auto revisions = sim.route_revisions().size();
    check(!sim.insert_order(DynamicOrder::urgent("bad", "W", "B", 11, 481)).accepted, "overload rejected");
    check(sim.deliveries().size() == 1 && sim.route_revisions().size() == revisions, "rejection preserves task set and route");
    check_near(sim.snapshot().progress, before.progress); check_near(sim.snapshot().load, before.load);
    check(!sim.insert_order(DynamicOrder::urgent("future", "W", "B", 1, 490)).accepted, "not-yet-released rejected");
    finish(sim); check_near(sim.clock().now(), 492);
}
void idle_restart() {
    Simulation sim(graph(), "W", {}, 480, options());
    check(sim.finished(), "empty initial trip");
    sim.wait_until(490);
    check(sim.insert_order(DynamicOrder::urgent("u", "W", "B", 3, 490)).accepted, "idle accepts new trip");
    check_near(*sim.deliveries()[0].picked_up_at, 490);
    finish(sim); check_near(sim.clock().now(), 502);
    auto zero = graph();
    zero.update_edge_traffic("W", "B", 0, TrafficStatus::Clear);
    zero.update_edge_traffic("B", "W", 0, TrafficStatus::Clear);
    auto instant_options = options(); instant_options.unload_minutes = 0;
    Simulation instant(zero, "W", {}, 480, instant_options);
    check(instant.insert_order(DynamicOrder::urgent("instant", "W", "B", 0, 480)).accepted, "zero-duration order accepted");
    check(instant.finished() && instant.snapshot().delivered_count == 1, "instant phases terminate");
    check_near(instant.clock().now(), 480);
}
void legacy_upgrade() {
    auto g = graph();
    PlanningRequest req; req.warehouse_id = "W"; req.departure_time = 480; req.delivery_node_ids = std::vector<NodeId>{"A"};
    Simulation sim(g, Planner::plan(g, req), options()); sim.advance(1);
    check(sim.insert_order(DynamicOrder::urgent("u", "W", "B", 3, 481)).accepted, "legacy route can accept order");
    finish(sim); check(sim.snapshot().delivered_count == 2, "legacy task retained after switching");
    check(sim.route_revisions().front().id == 0, "legacy prefix preserved");
}
void step_consistency() {
    std::vector<double> reference;
    for (const double step : {1., 5., 100.}) {
        auto settings = options(); settings.step_minutes = step;
        Simulation sim(graph(), "W", {old_order()}, 480, settings);
        sim.advance(1); check(sim.insert_order(DynamicOrder::urgent("u", "W", "B", 3, 481)).accepted, "inserted");
        finish(sim);
        std::vector<double> times{sim.clock().now(), *sim.deliveries()[0].delivered_at, *sim.deliveries()[1].delivered_at};
        if (reference.empty()) { reference = times; }
        else { for (std::size_t i = 0; i < times.size(); ++i) { check_near(times[i], reference[i]); } }
    }
}
void config_test() {
    std::istringstream input(R"({"schema_version":1,"initial_orders":[],"dynamic_orders":[
        {"id":"u","warehouse_id":"W","delivery_node_id":"A","quantity":2,"time_window":{"earliest":480,"latest":540},"release_time":480}]})");
    const auto scenario = OrderConfigLoader::read(input);
    check(scenario.dynamic_orders.size() == 1 && scenario.dynamic_orders[0].priority() == OrderPriority::Urgent, "scenario parsed");
    for (const std::string invalid : {
        R"({"schema_version":1,"initial_orders":[],"dynamic_orders":[],"extra":1})",
        R"({"schema_version":1,"schema_version":1,"initial_orders":[],"dynamic_orders":[]})",
        R"({"schema_version":1,"initial_orders":[],"dynamic_orders":[{"id":"x","warehouse_id":"W","delivery_node_id":"A","quantity":true,"time_window":null,"release_time":480}]})"}) {
        std::istringstream broken(invalid);
        expect_throw<ConfigError>([&] { OrderConfigLoader::read(broken); });
    }
}
void revision_export() {
    Simulation sim(graph(), "W", {old_order()}, 480, options()); sim.advance(1);
    sim.insert_order(DynamicOrder::urgent("u", "W", "B", 3, 481)); finish(sim);
    sim.export_csv("test_artifacts/orders");
    for (const auto* file : {"deliveries.csv", "insertions.csv", "route_revisions.csv", "route_stops.csv", "simulation.csv"}) {
        check(std::filesystem::file_size(std::filesystem::path("test_artifacts/orders") / file) > 50, "exported orders and revisions");
    }
    for (const auto& snapshot : sim.history()) {
        bool found = false;
        for (const auto& rev : sim.route_revisions()) { found = found || rev.id == snapshot.route_revision; }
        check(found, "each snapshot retains its route revision");
    }
    std::ostringstream log; sim.write_log(log);
    check(log.str().find("order_accepted") != std::string::npos && log.str().find("Late orders:") != std::string::npos, "log contains decisions and late list");
}
}  // namespace
int main(int argc, char* argv[]) {
    return run_tests(argc, argv, {{"windows", windows}, {"order_values", order_values}, {"strict_initial", strict_initial},
        {"urgent_priority", urgent_priority}, {"capacity_after_unload", capacity_after_unload},
        {"preserve_existing_windows", preserve_existing_windows}, {"moving_insertion", moving_insertion},
        {"locked_service", locked_service}, {"multiple_pending", multiple_pending}, {"atomic_rejection", atomic_rejection},
        {"idle_restart", idle_restart}, {"legacy_upgrade", legacy_upgrade}, {"step_consistency", step_consistency},
        {"scenario_config", config_test}, {"revision_export", revision_export}});
}
