#include "logistics/config/ConfigLoader.h"
#include "logistics/planning/Dijkstra.h"
#include "logistics/planning/Planner.h"
#include "logistics/visualization/Visualizer.h"
#include "test_support.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <set>

using namespace logistics;
namespace {
Graph sample() {
    Graph graph;
    graph.add_node(Node("W", NodeType::Warehouse, 0, 0));
    graph.add_node(Node("A", NodeType::DeliveryPoint, 1, 0, 4, TimeWindow{0, 1}));
    graph.add_node(Node("B", NodeType::DeliveryPoint, 2, 0, 6));
    graph.add_node(Node("T", NodeType::TransferStation, 0, 1));
    for (const auto& edge : std::vector<Edge>{
             Edge("WA", "W", "A", 1, 2, 9), Edge("WT", "W", "T", 2, 4, 1),
             Edge("TA", "T", "A", 2, 5, 1), Edge("AB", "A", "B", 1, 3, 4),
             Edge("WB", "W", "B", 5, 8, 2), Edge("BA", "B", "A", 1, 1, 1),
             Edge("AW", "A", "W", 1, 2, 3), Edge("BW", "B", "W", 4, 6, 1)}) {
        graph.add_edge(edge);
    }
    return graph;
}
PlanningRequest request(RouteStrategy strategy = RouteStrategy::ShortestDistance) {
    PlanningRequest value;
    value.warehouse_id = "W";
    value.strategy = strategy;
    value.departure_time = 480;
    return value;
}
void validate_route(const Graph& graph, const RoutePlan& plan) {
    check(plan.status == PlanStatus::Feasible, "complete tour expected");
    check(plan.node_sequence.front() == plan.start_node_id && plan.node_sequence.back() == plan.warehouse_id,
          "start and return endpoints");
    check(plan.road_sequence.size() + 1 == plan.node_sequence.size(), "route lengths");
    double distance = 0, time = 0, cost = 0;
    for (std::size_t index = 0; index < plan.road_sequence.size(); ++index) {
        const auto* edge = graph.find_edge_by_id(plan.road_sequence[index]);
        check(edge && edge->from_id() == plan.node_sequence[index] &&
              edge->to_id() == plan.node_sequence[index + 1] && edge->traffic_status() != TrafficStatus::Closed,
              "route follows open directed edges");
        distance += edge->distance(); time += edge->time(); cost += edge->cost();
    }
    check_near(distance, plan.total_distance_km);
    check_near(time, plan.total_travel_minutes);
    check_near(cost, plan.total_cost_yuan);
    std::set<NodeId> delivered;
    for (const auto& stop : plan.stops) {
        if (stop.role == StopRole::Delivery) { check(delivered.insert(stop.node_id).second, "deliver once"); }
    }
    check(delivered == std::set<NodeId>(plan.requested_delivery_node_ids.begin(), plan.requested_delivery_node_ids.end()),
          "all requested points delivered");
}
void shortest_strategies() {
    auto graph = sample();
    const auto distance = Dijkstra::shortest_path(graph, "W", "A");
    check(distance.node_sequence == std::vector<NodeId>{"W", "A"}, "distance path");
    check_near(distance.total_distance_km, 1); check_near(distance.total_cost_yuan, 9);
    const auto cost = Dijkstra::shortest_path(graph, "W", "A", WeightMetric::Cost);
    check(cost.node_sequence == std::vector<NodeId>{"W", "T", "A"}, "cost path via transit");
    check_near(cost.total_distance_km, 4); check_near(cost.total_travel_minutes, 9); check_near(cost.total_cost_yuan, 2);
    const auto stale = Dijkstra::shortest_path(graph, "W", "B");
    check(stale.node_sequence == std::vector<NodeId>{"W", "A", "B"}, "heap relaxation replaces longer direct route");
    check_near(stale.total_distance_km, 2);
    const auto snapshot = Dijkstra::single_source(graph, "W");
    graph.remove_node("A");
    check(snapshot.path_to("A").node_sequence == distance.node_sequence, "snapshot survives graph mutation");
}
void shortest_edge_cases() {
    auto graph = sample();
    graph.add_node(Node("U", NodeType::DeliveryPoint, 3, 3));
    const auto same = Dijkstra::shortest_path(graph, "W", "W");
    check(same.reachable && same.node_sequence == std::vector<NodeId>{"W"} && same.road_sequence.empty(), "same endpoint");
    const auto unreachable = Dijkstra::shortest_path(graph, "W", "U");
    check(!unreachable.reachable && unreachable.node_sequence.empty(), "unreachable result");
    expect_throw<std::invalid_argument>([&] { (void)Dijkstra::single_source(graph, "missing"); });
    expect_throw<std::invalid_argument>([&] { (void)Dijkstra::shortest_path(graph, "W", "missing"); });
    expect_throw<std::invalid_argument>([&] { (void)Dijkstra::single_source(graph, "W", static_cast<WeightMetric>(99)); });
    graph.update_edge_traffic("W", "A", 2, TrafficStatus::Closed);
    const auto detour = Dijkstra::shortest_path(graph, "W", "A");
    check(detour.node_sequence == std::vector<NodeId>{"W", "T", "A"}, "closed road skipped");
    graph.update_edge_traffic("W", "T", 20, TrafficStatus::Congested);
    check_near(Dijkstra::shortest_path(graph, "W", "A").total_travel_minutes, 25);
}
void zero_cycles_and_ties() {
    Graph graph;
    for (const auto& id : {"W", "A", "B"}) { graph.add_node(Node(id, NodeType::DeliveryPoint, 0, 0)); }
    graph.add_edge(Edge("WB", "W", "B", 0, 0, 0));
    graph.add_edge(Edge("WA", "W", "A", 0, 0, 0));
    graph.add_edge(Edge("AB", "A", "B", 0, 0, 0));
    graph.add_edge(Edge("BA", "B", "A", 0, 0, 0));
    graph.add_edge(Edge("AW", "A", "W", 0, 0, 0));
    const auto paths = Dijkstra::single_source(graph, "W");
    check(paths.path_to("B").node_sequence == std::vector<NodeId>{"W", "B"}, "zero cycle must not rewrite parents");
    check(paths.path_to("W").node_sequence.size() == 1, "source has no predecessor cycle");
    check_near(*paths.distance_to("A"), 0);
}
void overflow_detour() {
    Graph graph;
    for (const auto& id : {"W", "A", "X", "Y"}) { graph.add_node(Node(id, NodeType::DeliveryPoint, 0, 0)); }
    const double large = std::numeric_limits<double>::max();
    graph.add_edge(Edge("WA", "W", "A", 1, 1, 1));
    graph.add_edge(Edge("WX", "W", "X", large, 1, 1));
    graph.add_edge(Edge("XY", "X", "Y", large, 1, 1));
    const auto paths = Dijkstra::single_source(graph, "W");
    check_near(*paths.distance_to("A"), 1);
    check(paths.distance_to("X").has_value() && !paths.distance_to("Y"), "overflow path not represented as finite");
}
void floyd_comparison() {
    std::mt19937 random(20261017);
    constexpr std::size_t count = 7;
    const double infinity = std::numeric_limits<double>::infinity();
    for (int trial = 0; trial < 20; ++trial) {
        Graph graph;
        for (std::size_t i = 0; i < count; ++i) { graph.add_node(Node(std::to_string(i), NodeType::DeliveryPoint, 0, 0)); }
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t j = 0; j < count; ++j) {
                if (i != j && random() % 3 != 0) {
                    const double distance = random() % 8;
                    const double cost = random() % 8;
                    const auto status = random() % 5 == 0 ? TrafficStatus::Closed : TrafficStatus::Clear;
                    graph.add_edge(Edge(std::to_string(i) + "-" + std::to_string(j), std::to_string(i), std::to_string(j), distance, 1, cost, status));
                }
            }
        }
        for (const auto metric : {WeightMetric::Distance, WeightMetric::Cost}) {
            std::vector<std::vector<double>> oracle(count, std::vector<double>(count, infinity));
            for (std::size_t i = 0; i < count; ++i) {
                oracle[i][i] = 0;
                for (const auto& edge : graph.outgoing_edges(std::to_string(i))) {
                    if (edge.traffic_status() != TrafficStatus::Closed) { oracle[i][std::stoul(edge.to_id())] = edge.weight(metric); }
                }
            }
            for (std::size_t k = 0; k < count; ++k) {
                for (std::size_t i = 0; i < count; ++i) {
                    for (std::size_t j = 0; j < count; ++j) { oracle[i][j] = std::min(oracle[i][j], oracle[i][k] + oracle[k][j]); }
                }
            }
            for (std::size_t i = 0; i < count; ++i) {
                const auto paths = Dijkstra::single_source(graph, std::to_string(i), metric);
                for (std::size_t j = 0; j < count; ++j) {
                    const auto path = paths.path_to(std::to_string(j));
                    check(path.reachable == std::isfinite(oracle[i][j]), "reachability differs from Floyd-Warshall");
                    if (path.reachable) {
                        check_near(metric == WeightMetric::Distance ? path.total_distance_km : path.total_cost_yuan, oracle[i][j]);
                        check(path.node_sequence.size() <= count, "shortest path contains a cycle");
                    }
                }
            }
        }
    }
}
void tours_and_totals() {
    const auto graph = sample();
    const auto distance = Planner::plan(graph, request());
    validate_route(graph, distance);
    check(distance.node_sequence == std::vector<NodeId>{"W", "A", "B", "A", "W"}, "distance tour including return transit");
    check_near(distance.total_distance_km, 4); check_near(distance.total_travel_minutes, 8); check_near(distance.total_cost_yuan, 17);
    const auto cost = Planner::plan(graph, request(RouteStrategy::LowestCost));
    validate_route(graph, cost);
    check(cost.node_sequence == std::vector<NodeId>{"W", "T", "A", "B", "W"}, "cost tour");
    check_near(cost.total_distance_km, 9); check_near(cost.total_travel_minutes, 18); check_near(cost.total_cost_yuan, 7);
    check_near(cost.stops[1].estimated_arrival, 489);
    check_near(cost.stops.back().estimated_arrival, 498);
}
void empty_subset_and_start() {
    const auto graph = sample();
    auto value = request();
    value.delivery_node_ids = std::vector<NodeId>{};
    const auto empty = Planner::plan(graph, value);
    validate_route(graph, empty);
    check(empty.node_sequence == std::vector<NodeId>{"W"} && empty.stops.size() == 2, "empty request stays home");
    value.delivery_node_ids = std::vector<NodeId>{"B"};
    const auto subset = Planner::plan(graph, value);
    validate_route(graph, subset);
    check(subset.stops.size() == 3 && subset.stops[1].node_id == "B", "unrequested transit point is not delivered");
    value.start_node_id = "B";
    const auto start = Planner::plan(graph, value);
    validate_route(graph, start);
    check(start.stops[1].node_id == "B" && start.stops[1].route_node_index == 0, "deliver at current start");
}
void transit_delivery() {
    Graph graph;
    graph.add_node(Node("W", NodeType::Warehouse, 0, 0));
    graph.add_node(Node("A", NodeType::DeliveryPoint, 1, 0));
    graph.add_node(Node("B", NodeType::DeliveryPoint, 0, 1));
    graph.add_edge(Edge("WB", "W", "B", 0, 2, 0));
    graph.add_edge(Edge("BA", "B", "A", 0, 3, 0));
    graph.add_edge(Edge("AW", "A", "W", 1, 4, 1));
    const auto plan = Planner::plan(graph, request());
    validate_route(graph, plan);
    check(plan.stops[1].node_id == "B" && plan.stops[2].node_id == "A", "serve pending point on chosen path");
    check_near(plan.stops[1].estimated_arrival, 482);
}
void invalid_requests_and_failures() {
    auto graph = sample();
    auto value = request();
    value.warehouse_id = "A";
    expect_throw<std::invalid_argument>([&] { (void)Planner::plan(graph, value); });
    value = request(); value.delivery_node_ids = std::vector<NodeId>{"A", "A"};
    expect_throw<std::invalid_argument>([&] { (void)Planner::plan(graph, value); });
    value.delivery_node_ids = std::vector<NodeId>{"T"};
    expect_throw<std::invalid_argument>([&] { (void)Planner::plan(graph, value); });
    value = request(); value.departure_time = std::numeric_limits<double>::quiet_NaN();
    expect_throw<std::invalid_argument>([&] { (void)Planner::plan(graph, value); });
    value = request(); value.capacity_kg = 9;
    const auto over = Planner::plan(graph, value);
    check(over.status == PlanStatus::NoFeasiblePlanFound && over.node_sequence.empty(), "capacity failure has no executable prefix");
    value.capacity_kg = 10;
    validate_route(graph, Planner::plan(graph, value));
    graph.add_node(Node("U", NodeType::DeliveryPoint, 3, 3));
    const auto failed = Planner::plan(graph, request());
    check(failed.status == PlanStatus::NoFeasiblePlanFound && failed.node_sequence.empty() &&
          failed.unassigned_delivery_node_ids.size() == 3, "unreachable delivery reports entire task failure");
    graph.remove_node("U"); graph.remove_edge("A", "W"); graph.remove_edge("B", "W");
    const auto no_return = Planner::plan(graph, request());
    check(no_return.status == PlanStatus::NoFeasiblePlanFound && no_return.diagnostic.find("returns") != std::string::npos,
          "missing return path rejected");
}
std::string read_file(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    check(input.good(), "export file missing");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void csv_export() {
    auto graph = sample();
    graph.remove_edge("W", "A");
    graph.add_edge(Edge("e,\"1", "W", "A", 1, 2, 9));
    const auto plan = Planner::plan(graph, request());
    const auto directory = std::filesystem::path("test_artifacts") / "export";
    Visualizer::export_csv(graph, plan, directory);
    check(read_file(directory / "edges.csv").find("\"e,\"\"1\"") != std::string::npos, "CSV escaping");
    const auto route = read_file(directory / "route.csv");
    check(route.find("4,\"W\",\"AW\",488,4,8,17") != std::string::npos, "exported totals and arrival");
    check(read_file(directory / "summary.csv").find("distance,nearest_neighbor_dijkstra,feasible") != std::string::npos, "summary strategy");
    check(read_file(directory / "stops.csv").find("1,delivery,\"A\",1,482,482") != std::string::npos, "stops identify route positions");
    check(read_file(directory / "nodes.csv").find("\"T\",transfer_station") != std::string::npos, "network includes transfer station");
    const auto before = read_file(directory / "summary.csv");
    graph.update_edge_traffic("W", "A", 100, TrafficStatus::Congested);
    expect_throw<std::invalid_argument>([&] { Visualizer::export_csv(graph, plan, directory); }, "totals");
    check(read_file(directory / "summary.csv") == before, "invalid plan rejected before overwriting export");
}
void export_rejects_invalid_plan() {
    auto graph = sample();
    const auto original = Planner::plan(graph, request());
    const auto directory = std::filesystem::path("test_artifacts") / "invalid_export";
    auto plan = original; plan.stops.clear();
    expect_throw<std::invalid_argument>([&] { Visualizer::export_csv(graph, plan, directory); });
    plan = original; plan.node_sequence[1] = "T";
    expect_throw<std::invalid_argument>([&] { Visualizer::export_csv(graph, plan, directory); });
    plan = original; plan.total_cost_yuan = 0;
    expect_throw<std::invalid_argument>([&] { Visualizer::export_csv(graph, plan, directory); });
    plan = original; plan.stops[1].estimated_delivery = 999;
    expect_throw<std::invalid_argument>([&] { Visualizer::export_csv(graph, plan, directory); });
    plan = original; graph.update_edge_traffic("W", "A", 2, TrafficStatus::Closed);
    expect_throw<std::invalid_argument>([&] { Visualizer::export_csv(graph, plan, directory); });
}
void demo_tours() {
    const auto graph = ConfigLoader::load_network("config/network.json", true);
    for (const auto strategy : {RouteStrategy::ShortestDistance, RouteStrategy::LowestCost}) {
        auto value = request(strategy); value.warehouse_id = "W01";
        const auto plan = Planner::plan(graph, value);
        validate_route(graph, plan);
        check(plan.requested_delivery_node_ids.size() == 25, "demo delivers all 25 points");
        Visualizer::export_csv(graph, plan, std::filesystem::path("test_artifacts") /
            (strategy == RouteStrategy::ShortestDistance ? "demo_distance" : "demo_cost"));
    }
}
}  // namespace
int main(int argc, char* argv[]) {
    return run_tests(argc, argv, {{"shortest_strategies", shortest_strategies}, {"shortest_edge_cases", shortest_edge_cases},
        {"zero_cycles_and_ties", zero_cycles_and_ties}, {"overflow_detour", overflow_detour},
        {"floyd_comparison", floyd_comparison}, {"tours_and_totals", tours_and_totals},
        {"empty_subset_and_start", empty_subset_and_start}, {"transit_delivery", transit_delivery},
        {"invalid_requests_and_failures", invalid_requests_and_failures}, {"csv_export", csv_export},
        {"export_rejects_invalid_plan", export_rejects_invalid_plan}, {"demo_tours", demo_tours}});
}
