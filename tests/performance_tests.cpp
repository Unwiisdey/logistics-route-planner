#include "test_support.hpp"
#include "logistics/planning/DynamicOrder.h"
#include <algorithm>
#include <chrono>
#include <iomanip>

using namespace logistics;
namespace {
Graph network(std::size_t count) {
    Graph g;
    for (std::size_t i = 0; i < count; ++i) {
        g.add_node(Node(std::to_string(i), i ? NodeType::DeliveryPoint : NodeType::Warehouse,
                        static_cast<double>(i % 20), static_cast<double>(i / 20)));
    }
    for (std::size_t i = 0; i < count; ++i) {
        for (const auto delta : {std::size_t(1), count - 1, std::size_t(7), count - 7}) {
            const auto to = (i + delta) % count;
            const auto id = std::to_string(i) + "-" + std::to_string(to);
            g.add_edge(Edge(id, std::to_string(i), std::to_string(to), 1 + static_cast<double>(i % 5),
                           1 + static_cast<double>((i + to) % 9), 1 + static_cast<double>(to % 7)));
        }
    }
    return g;
}
double elapsed(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
void row(const char* workload, const Graph& g, std::size_t capacity, const ShortestPathCache& cache, double ms, double checksum) {
    const auto& stats = cache.stats();
    std::cout << workload << ',' << g.node_count() << ',' << g.edge_count() << ',' << capacity << ','
              << stats.hits + stats.misses << ',' << stats.hits << ',' << stats.misses << ',' << ms << ',' << checksum << '\n';
}
void repeated_paths(std::size_t count, int repetitions) {
    const auto g = network(count);
    const auto pairs = std::min(count, std::size_t(32));
    std::vector<PathResult> expected;
    for (std::size_t i = 0; i < pairs; ++i) {
        expected.push_back(Dijkstra::shortest_path(g, std::to_string(i), std::to_string((i * 7 + count / 2) % count), WeightMetric::Time));
    }
    double reference = 0;
    for (auto capacity : {std::size_t(0), std::size_t(256)}) {
        ShortestPathCache cache(capacity); double checksum = 0;
        const auto start = std::chrono::steady_clock::now();
        for (int run = 0; run < repetitions; ++run) {
            for (std::size_t i = 0; i < pairs; ++i) {
                const auto result = cache.query(g, std::to_string(i), std::to_string((i * 7 + count / 2) % count), WeightMetric::Time);
                check(result.reachable && result.road_sequence == expected[i].road_sequence, "benchmark route changed");
                check_near(result.total_travel_minutes, expected[i].total_travel_minutes);
                checksum += result.total_distance_km + result.total_travel_minutes + result.total_cost_yuan;
            }
        }
        const auto ms = elapsed(start);
        if (!capacity) { reference = checksum; }
        else {
            check_near(checksum, reference);
            check(cache.stats().misses == pairs && cache.stats().hits == pairs * (repetitions - 1), "expected cache search reduction");
        }
        row("repeated_paths", g, capacity, cache, ms, checksum);
    }
}
void order_candidates(std::size_t count, std::size_t order_count) {
    const auto g = network(count);
    OrderPlanningContext ctx;
    ctx.start_node = ctx.warehouse = "0"; ctx.capacity = 100; ctx.current_load = static_cast<double>(order_count);
    for (std::size_t i = 1; i <= order_count; ++i) {
        ctx.orders.emplace_back("O" + std::to_string(i), "0", std::to_string(i * 9 % count), 1, std::nullopt);
    }
    std::vector<RoadId> reference;
    for (auto capacity : {std::size_t(0), std::size_t(256)}) {
        ShortestPathCache cache(capacity);
        const auto start = std::chrono::steady_clock::now();
        const auto result = DynamicOrder::initial(g, ctx, &cache);
        const auto ms = elapsed(start);
        check(result.accepted, "benchmark initial plan failed");
        if (!capacity) { reference = result.route.roads; }
        else {
            check(result.route.roads == reference, "candidate cache changed selected plan");
            check(cache.stats().hits > 0, "candidate cache must avoid repeated searches");
        }
        row("order_candidates", g, capacity, cache, ms, result.route.distance + result.route.travel + result.route.cost);
    }
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        const bool full = argc == 2 && std::string(argv[1]) == "--full";
        if (argc > 2 || (argc == 2 && !full)) { throw std::invalid_argument("usage: performance_tests [--full]"); }
        std::cout << std::fixed << std::setprecision(3)
                  << "workload,nodes,edges,cache_capacity,queries,hits,searches,elapsed_ms,checksum\n";
        repeated_paths(30, full ? 15 : 3);
        if (full) { repeated_paths(300, 15); repeated_paths(1000, 15); }
        order_candidates(full ? 300 : 30, full ? 12 : 4);
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
