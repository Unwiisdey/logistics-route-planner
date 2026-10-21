#include "test_support.hpp"
#include "logistics/config/ConfigLoader.h"
#include "logistics/config/OrderConfigLoader.h"
#include "logistics/simulation/Simulation.h"
#include <algorithm>

using namespace logistics;
namespace {
Simulation run(RouteStrategy strategy, std::size_t cache_capacity, unsigned seed, double step, bool traffic) {
    auto graph = ConfigLoader::load_network("config/network.json", true);
    auto scenario = OrderConfigLoader::load("config/orders.json");
    SimulationOptions options; options.capacity_kg = 100; options.path_cache_capacity = cache_capacity; options.step_minutes = step;
    Simulation sim(graph, "W01", scenario.initial_orders, 480, options, strategy);
    if (traffic) { TrafficOptions settings; settings.seed = seed; sim.enable_traffic(settings); }
    std::size_t next = 0, guard = 0;
    while (!sim.finished() || next < scenario.dynamic_orders.size()) {
        check(++guard < 20000, "integration simulation failed to terminate");
        if (next < scenario.dynamic_orders.size() && scenario.dynamic_orders[next].release_time() <= sim.clock().now()) {
            sim.insert_order(scenario.dynamic_orders[next++]);
        } else if (sim.finished()) { sim.wait_until(scenario.dynamic_orders[next].release_time()); }
        else { sim.advance(next == scenario.dynamic_orders.size() ? step : std::min(step, scenario.dynamic_orders[next].release_time() - sim.clock().now())); }
    }
    return sim;
}
void compare(const Simulation& a, const Simulation& b) {
    check_near(a.clock().now(), b.clock().now());
    check(a.deliveries().size() == b.deliveries().size(), "same accepted orders");
    for (std::size_t i = 0; i < a.deliveries().size(); ++i) {
        const auto& x = a.deliveries()[i]; const auto& y = b.deliveries()[i];
        check(x.order_id == y.order_id && x.delivered_at && y.delivered_at, "order completion");
        check_near(*x.arrival, *y.arrival); check_near(*x.delivered_at, *y.delivered_at);
        check_near(x.penalty_points, y.penalty_points);
    }
    check(a.route_revisions().size() == b.route_revisions().size(), "same revision count");
    for (std::size_t i = 0; i < a.route_revisions().size(); ++i) {
        check(a.route_revisions()[i].route.roads == b.route_revisions()[i].route.roads, "cache changes route");
    }
    check(a.insertions().size() == 3 && b.insertions().size() == 3, "all dynamic requests processed");
    for (std::size_t i = 0; i < 3; ++i) {
        check(a.insertions()[i].accepted == b.insertions()[i].accepted && a.insertions()[i].reason == b.insertions()[i].reason, "same insertion decision");
    }
    check(a.snapshot().delivered_count == 4 && a.vehicle().position().node_id == "W01", "complete tour");
    check_near(a.vehicle().current_load(), 0);
    for (const auto& s : a.history()) { check(s.load >= 0 && s.load <= 100, "capacity throughout run"); }
    check(a.path_cache_stats().hits > 0 && b.path_cache_stats().hits == 0, "cached and disabled paths exercised");
}
void dynamic_orders() {
    for (const auto strategy : {RouteStrategy::ShortestDistance, RouteStrategy::LowestCost}) {
        compare(run(strategy, 256, 42, 5, false), run(strategy, 0, 42, 5, false));
    }
}
void traffic_cache_equivalence() {
    for (const auto seed : {7u, 42u, 2026u}) {
        compare(run(RouteStrategy::ShortestDistance, 256, seed, 5, true), run(RouteStrategy::ShortestDistance, 0, seed, 5, true));
    }
}
void cost_and_step() {
    compare(run(RouteStrategy::LowestCost, 256, 42, .25, true), run(RouteStrategy::LowestCost, 0, 42, 10, true));
}
}  // namespace
int main(int argc, char* argv[]) {
    return run_tests(argc, argv, {{"dynamic_orders", dynamic_orders}, {"traffic_cache_equivalence", traffic_cache_equivalence}, {"cost_and_step", cost_and_step}});
}
