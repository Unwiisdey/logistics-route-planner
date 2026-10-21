#include "test_support.hpp"
#include "logistics/planning/ShortestPathCache.h"

using namespace logistics;
namespace {
Graph network() {
    Graph g;
    g.add_node(Node("W", NodeType::Warehouse, 0, 0));
    g.add_node(Node("A", NodeType::DeliveryPoint, 1, 0));
    g.add_node(Node("B", NodeType::DeliveryPoint, 0, 1));
    g.add_node(Node("Z", NodeType::DeliveryPoint, 3, 3));
    for (const auto& e : std::vector<Edge>{Edge("WA", "W", "A", 1, 5, 4), Edge("WB", "W", "B", 3, 1, 1),
        Edge("BA", "B", "A", 1, 1, 1), Edge("AW", "A", "W", 1, 1, 1)}) { g.add_edge(e); }
    return g;
}
void hits_and_metrics() {
    auto g = network(); ShortestPathCache cache;
    auto first = cache.query(g, "W", "A"); first.road_sequence.clear();
    check(cache.query(g, "W", "A").road_sequence == std::vector<RoadId>{"WA"}, "returned copy independent");
    check(cache.stats().hits == 1 && cache.stats().misses == 1, "hit avoids search");
    for (auto metric : {WeightMetric::Time, WeightMetric::Cost}) {
        check(cache.query(g, "W", "A", metric).road_sequence == std::vector<RoadId>({"WB", "BA"}), "metric isolated");
    }
    check(cache.stats().misses == 3, "three metric keys");
    check(cache.query(g, "A", "W").road_sequence == std::vector<RoadId>{"AW"}, "direction isolated");
}
void traffic_invalidation() {
    auto g = network(); ShortestPathCache cache;
    cache.query(g, "W", "A", WeightMetric::Time);
    g.update_edge_traffic("W", "A", .5, TrafficStatus::Clear); // Decrease on a non-cached road creates a better route.
    check(cache.query(g, "W", "A", WeightMetric::Time).road_sequence == std::vector<RoadId>{"WA"}, "decrease outside old route invalidates");
    g.update_edge_traffic("W", "A", 10, TrafficStatus::Congested);
    check_near(cache.query(g, "W", "A").total_travel_minutes, 10); // Distance result must also refresh travel totals.
    g.update_edge_traffic("W", "A", 10, TrafficStatus::Closed);
    check(cache.query(g, "W", "A").road_sequence == std::vector<RoadId>({"WB", "BA"}), "closed edge excluded");
    g.update_edge_traffic("W", "A", 5, TrafficStatus::Clear);
    check(cache.query(g, "W", "A").road_sequence == std::vector<RoadId>{"WA"}, "reopening invalidates");
    check(cache.stats().invalidations == 4, "each graph revision invalidated once");
}
void topology_and_unreachable() {
    auto g = network(); ShortestPathCache cache;
    check(!cache.query(g, "W", "Z").reachable, "unreachable result");
    check(!cache.query(g, "W", "Z").reachable && cache.stats().hits == 1, "negative result cached");
    g.add_edge(Edge("AZ", "A", "Z", 1, 1, 1));
    check(cache.query(g, "W", "Z").reachable, "new edge invalidates negative result");
    g.remove_edge("A", "Z"); check(!cache.query(g, "W", "Z").reachable, "removed edge invalidates");
    g.remove_node("Z"); expect_throw<std::invalid_argument>([&] { cache.query(g, "W", "Z"); });
    g.add_node(Node("Z", NodeType::DeliveryPoint, 0, 0));
    check(!cache.query(g, "W", "Z").reachable, "re-added ID cannot reuse old path");
}
void graph_identity() {
    auto first = network(); auto copied = first; ShortestPathCache cache;
    cache.query(first, "W", "A"); cache.query(copied, "W", "A");
    check(cache.stats().hits == 1, "unchanged copy shares state token");
    copied.update_edge_traffic("W", "A", 8, TrafficStatus::Congested);
    check_near(cache.query(copied, "W", "A").total_travel_minutes, 8);
    check_near(cache.query(first, "W", "A").total_travel_minutes, 5);
    first = copied; check_near(cache.query(first, "W", "A").total_travel_minutes, 8);
    first = network(); check_near(cache.query(first, "W", "A").total_travel_minutes, 5);
    auto moved = std::move(first); check_near(cache.query(moved, "W", "A").total_travel_minutes, 5);
}
void lru_and_disabled() {
    const auto g = network(); ShortestPathCache cache(2);
    cache.query(g, "W", "A"); cache.query(g, "W", "B"); cache.query(g, "W", "A");
    cache.query(g, "A", "W"); // WB is least recently used.
    cache.query(g, "W", "A"); check(cache.stats().hits == 2, "hot entry retained");
    cache.query(g, "W", "B");
    check(cache.size() == 2 && cache.stats().evictions == 2, "bounded least recently used eviction");
    ShortestPathCache disabled(0);
    disabled.query(g, "W", "A"); disabled.query(g, "W", "A");
    check(disabled.size() == 0 && disabled.stats().hits == 0 && disabled.stats().misses == 2, "disabled means actual searches");
    cache.clear(); check(cache.size() == 0 && cache.stats().hits == 0, "explicit reset");
}
void invalid_and_noop() {
    auto g = network(); ShortestPathCache cache; cache.query(g, "W", "A");
    const auto version = g.revision();
    g.update_edge_traffic("W", "A", 5, TrafficStatus::Clear);
    check(!g.remove_node("missing") && !g.remove_edge("W", "Z"), "failed removals");
    expect_throw<std::invalid_argument>([&] { g.update_edge_traffic("W", "A", -1, TrafficStatus::Clear); });
    check(version == g.revision(), "no-op and rejected mutation keep version");
    expect_throw<std::invalid_argument>([&] { cache.query(g, "bad", "A"); });
    expect_throw<std::invalid_argument>([&] { cache.query(g, "W", "A", static_cast<WeightMetric>(99)); });
    cache.query(g, "W", "A"); check(cache.stats().hits == 1, "invalid query does not poison cache");
    check(cache.query(g, "W", "W").road_sequence.empty(), "same node path");
}
}  // namespace
int main(int argc, char* argv[]) {
    return run_tests(argc, argv, {{"hits_and_metrics", hits_and_metrics}, {"traffic_invalidation", traffic_invalidation},
        {"topology_and_unreachable", topology_and_unreachable}, {"graph_identity", graph_identity},
        {"lru_and_disabled", lru_and_disabled}, {"invalid_and_noop", invalid_and_noop}});
}
