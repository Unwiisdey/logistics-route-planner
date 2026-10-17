#include "logistics/graph/Graph.h"
// Ensure future-facing stage 1 headers still compile together.
#include "logistics/config/configuration.hpp"
#include "logistics/traffic/traffic_event.hpp"
#include "logistics/visualization/network_view.hpp"
#include "test_support.hpp"
#include <limits>
#include <sstream>

using namespace logistics;
namespace {
Graph small_graph() {
    Graph graph;
    graph.add_node(Node("B", NodeType::DeliveryPoint, 2, 0, 5, TimeWindow{480, 600}));
    graph.add_node(Node("A", NodeType::Warehouse, 0, 0));
    graph.add_node(Node("C", NodeType::TransferStation, 4, 0));
    return graph;
}
void nodes() {
    auto graph = small_graph();
    check(graph.node_count() == 3, "node count");
    check(graph.node_count(NodeType::Warehouse) == 1, "warehouse count");
    check_near(graph.find_node("B")->demand(), 5);
    check_near(graph.find_node("B")->time_window()->latest, 600);
    check(!graph.find_node("A")->time_window(), "warehouse window optional");
    expect_throw<std::invalid_argument>([&] { graph.add_node(Node("A", NodeType::Warehouse, 9, 9)); });
    check_near(graph.find_node("A")->x(), 0);
    check(graph.remove_node("B"), "remove node");
    check(!graph.remove_node("B"), "repeat remove node");
    check(!graph.find_node("B"), "node removed");
    graph.add_node(Node("B", NodeType::DeliveryPoint, 1, 1));
    check(graph.outgoing_edges("B").empty(), "re-added node has no edges");
    expect_throw<std::out_of_range>([&] { (void)graph.outgoing_edges("missing"); });
}
void remove_incident_edges() {
    auto graph = small_graph();
    graph.add_edge(Edge("AB", "A", "B", 1, 2, 3));
    graph.add_edge(Edge("BA", "B", "A", 1, 2, 3));
    graph.add_edge(Edge("CB", "C", "B", 1, 2, 3));
    graph.add_edge(Edge("BC", "B", "C", 1, 2, 3));
    graph.add_edge(Edge("AC", "A", "C", 9, 8, 7));
    check(graph.remove_node(graph.find_node("B")->id()), "remove via referenced ID");
    check(graph.edge_count() == 1, "all incoming/outgoing edges removed");
    check(graph.find_edge("A", "C") && !graph.find_edge_by_id("AB"), "unrelated edge retained");
    check(graph.outgoing_edges("C").empty(), "incoming cleanup");
    graph.add_node(Node("B", NodeType::DeliveryPoint, 0, 0));
    graph.add_edge(Edge("AB", "A", "B", 1, 2, 3));
    check(graph.edge_count() == 2, "removed edge id can be reused");
}
void edges() {
    auto graph = small_graph();
    graph.add_edge(Edge("AB", "A", "B", 1, 2, 3));
    check(graph.edge_count() == 1 && !graph.find_edge("B", "A"), "directed edge");
    expect_throw<std::invalid_argument>([&] { graph.add_edge(Edge("other", "A", "B", 4, 5, 6)); });
    expect_throw<std::invalid_argument>([&] { graph.add_edge(Edge("AB", "B", "A", 4, 5, 6)); });
    expect_throw<std::invalid_argument>([&] { graph.add_edge(Edge("bad", "A", "missing", 4, 5, 6)); });
    check(graph.edge_count() == 1, "failed insertion must not mutate graph");
    graph.add_edge(Edge("BA", "B", "A", 4, 5, 6));
    check(graph.remove_edge("A", "B"), "remove directed edge");
    check(graph.find_edge("B", "A") != nullptr, "reverse direction retained");
    check(!graph.remove_edge("A", "B") && !graph.remove_edge("missing", "A"), "missing removal");
    check(graph.remove_edge("B", "A") && graph.edge_count() == 0, "last edge removed");
}
void invalid_values() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    expect_throw<std::invalid_argument>([] { Node("", NodeType::Warehouse, 0, 0); });
    expect_throw<std::invalid_argument>([] { Node("bad id", NodeType::Warehouse, 0, 0); });
    expect_throw<std::invalid_argument>([&] { Node("A", NodeType::Warehouse, nan, 0); });
    expect_throw<std::invalid_argument>([&] { Node("A", NodeType::Warehouse, 0, inf); });
    expect_throw<std::invalid_argument>([] { Node("A", NodeType::DeliveryPoint, 0, 0, -1); });
    expect_throw<std::invalid_argument>([] { Node("A", NodeType::DeliveryPoint, 0, 0, 0, TimeWindow{9, 8}); });
    expect_throw<std::invalid_argument>([] { Node("A", static_cast<NodeType>(99), 0, 0); });
    expect_throw<std::invalid_argument>([] { Edge("AA", "A", "A", 1, 2, 3); });
    expect_throw<std::invalid_argument>([] { Edge("AB", "A", "B", -1, 2, 3); });
    expect_throw<std::invalid_argument>([] { Edge("AB", "A", "B", 1, -2, 3); });
    expect_throw<std::invalid_argument>([] { Edge("AB", "A", "B", 1, 2, -3); });
    expect_throw<std::invalid_argument>([&] { Edge("AB", "A", "B", 1, inf, 3); });
    expect_throw<std::invalid_argument>([] { Edge("AB", "A", "B", 1, 2, 3, static_cast<TrafficStatus>(99)); });
}
void adjacency_list() {
    auto graph = small_graph();
    graph.add_edge(Edge("AC", "A", "C", 0, 0, 0, TrafficStatus::Closed));
    graph.add_edge(Edge("AB", "A", "B", 1.5, 3, 2));
    std::ostringstream output;
    graph.print_adjacency_list(output);
    check(output.str() ==
        "A (warehouse): B [id=AB, distance=1.50, time=3.00, cost=2.00, traffic=clear]"
        " C [id=AC, distance=0.00, time=0.00, cost=0.00, traffic=closed]\n"
        "B (delivery_point): (none)\nC (transfer_station): (none)\n", "adjacency list ordering/weights/status");
}
void adjacency_matrix() {
    auto graph = small_graph();
    graph.add_edge(Edge("AB", "A", "B", 1.5, 3, 2));
    graph.add_edge(Edge("AC", "A", "C", 0, 0, 0));
    const auto matrix = graph.adjacency_matrix();
    check(matrix.node_ids == std::vector<NodeId>{"A", "B", "C"}, "matrix labels");
    check_near(*matrix.weights[0][1], 1.5);
    check(matrix.weights[0][2].has_value() && !matrix.weights[2][0], "zero vs absent");
    check_near(*matrix.weights[0][2], 0);
    check_near(*graph.adjacency_matrix(WeightMetric::Time).weights[0][1], 3);
    check_near(*graph.adjacency_matrix(WeightMetric::Cost).weights[0][1], 2);
    std::ostringstream output;
    graph.print_adjacency_matrix(output);
    check(output.str() == "id\tA\tB\tC\nA\t0.00\t1.50\t0.00\nB\tINF\t0.00\tINF\nC\tINF\tINF\t0.00\n", "matrix text");
    graph.remove_node("B");
    check(graph.adjacency_matrix().node_ids == std::vector<NodeId>{"A", "C"}, "matrix after removal");
}
void traffic_data() {
    auto graph = small_graph();
    graph.add_edge(Edge("AB", "A", "B", 1, 10, 3));
    graph.update_edge_traffic("A", "B", 12, TrafficStatus::Congested);
    graph.update_edge_traffic("A", "B", 15, TrafficStatus::Closed);
    const auto* edge = graph.find_edge("A", "B");
    check_near(edge->base_time(), 10);
    check_near(edge->time(), 15);
    check(edge->traffic_status() == TrafficStatus::Closed, "status changed");
    check_near(*graph.adjacency_matrix(WeightMetric::Time).weights[0][1], 15);
    expect_throw<std::invalid_argument>([&] { graph.update_edge_traffic("A", "B", -1, TrafficStatus::Clear); });
    check_near(graph.find_edge("A", "B")->time(), 15);
    expect_throw<std::out_of_range>([&] { graph.update_edge_traffic("B", "A", 1, TrafficStatus::Clear); });
}
void empty_and_copy() {
    Graph empty;
    std::ostringstream list, matrix;
    empty.print_adjacency_list(list);
    empty.print_adjacency_matrix(matrix);
    check(list.str().empty() && matrix.str() == "id\n", "empty graph output");
    check(empty.adjacency_matrix().weights.empty(), "empty matrix");
    auto original = small_graph();
    original.add_edge(Edge("AB", "A", "B", 1, 2, 3));
    auto copy = original;
    copy.remove_node("B");
    check(original.node_count() == 3 && original.edge_count() == 1, "copy independence");
}
}  // namespace
int main(int argc, char* argv[]) {
    return run_tests(argc, argv, {{"nodes", nodes}, {"remove_incident_edges", remove_incident_edges},
        {"edges", edges}, {"invalid_values", invalid_values}, {"adjacency_list", adjacency_list},
        {"adjacency_matrix", adjacency_matrix}, {"traffic_data", traffic_data}, {"empty_and_copy", empty_and_copy}});
}
