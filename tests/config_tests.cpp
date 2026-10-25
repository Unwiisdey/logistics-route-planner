#include "logistics/config/ConfigLoader.h"
#include "test_support.hpp"
#include <set>
#include <sstream>

using namespace logistics;
namespace {
const std::string valid_json = R"({
  "schema_version": 1,
  "nodes": [
    {"id":"A","type":"warehouse","x":0,"y":0},
    {"id":"B","type":"delivery_point","x":2,"y":-1,"demand":5,
     "time_window":{"earliest":480,"latest":600}},
    {"id":"C","type":"transfer_station","x":4,"y":0,"time_window":null}
  ],
  "edges": [
    {"id":"AB","from_id":"A","to_id":"B","distance":1.5,"time":3,"cost":2,"traffic_status":"congested"},
    {"from_id":"B","to_id":"A","distance":0,"time":0,"cost":0}
  ]
})";

Graph parse(const std::string& text, bool course_size = false) {
    std::istringstream input(text);
    return ConfigLoader::read_network(input, course_size);
}
std::string changed(const std::string& from, const std::string& to) {
    auto text = valid_json;
    const auto index = text.find(from);
    check(index != std::string::npos, "test replacement must match");
    text.replace(index, from.size(), to);
    return text;
}
void valid_and_defaults() {
    const auto graph = parse(valid_json);
    check(graph.node_count() == 3 && graph.edge_count() == 2, "loaded counts");
    check_near(graph.find_node("A")->demand(), 0);
    check(!graph.find_node("A")->time_window() && !graph.find_node("C")->time_window(), "optional windows");
    check_near(graph.find_node("B")->y(), -1);
    check_near(graph.find_node("B")->time_window()->earliest, 480);
    check(graph.find_edge("A", "B")->traffic_status() == TrafficStatus::Congested, "loaded status");
    check(graph.find_edge_by_id("B->A") != nullptr, "generated edge ID");
    check(graph.find_edge("B", "A")->traffic_status() == TrafficStatus::Clear, "default traffic");
    check_near(graph.find_edge("B", "A")->cost(), 0);
    check(parse("{\"schema_version\":1,\"nodes\":[],\"edges\":[]}").node_count() == 0, "empty small network");
}
void malformed_json() {
    for (const auto& text : std::vector<std::string>{"", "{", valid_json + " garbage", "[]", "null"}) {
        expect_throw<ConfigError>([&] { (void)parse(text); });
    }
    expect_throw<ConfigError>([] { (void)parse(changed("\"x\":0", "\"x\":0,\"x\":9")); }, "duplicate JSON key");
    expect_throw<ConfigError>([] { (void)parse(changed("1.5", "1e999")); }, "invalid JSON");
}
void invalid_schema() {
    const std::vector<std::pair<std::string, std::string>> cases{
        {"\"schema_version\": 1", "\"schema_version\": 2"},
        {"\"schema_version\": 1", "\"schema_version\": 1.0"},
        {"\"schema_version\": 1", "\"schema_version\": true"},
        {"\"schema_version\": 1,", ""},
        {"\"nodes\": [", "\"nodez\": ["},
        {"\"x\":0", "\"x\":false"},
        {"\"x\":0", "\"x\":\"0\""},
        {"\"x\":0", "\"x\":null"},
        {"\"x\":0,", ""},
        {"\"x\":0", "\"xx\":0"},
        {"\"time_window\":null", "\"time_window\":[]"},
        {"\"cost\":2", "\"cost\":true"},
        {"\"cost\":2", "\"cost\":\"2\""},
        {"\"cost\":2,", ""},
        {"\"demand\":5", "\"demand\":null"}
    };
    for (const auto& item : cases) {
        expect_throw<ConfigError>([&] { (void)parse(changed(item.first, item.second)); });
    }
    expect_throw<ConfigError>([] { (void)parse("{\"schema_version\":1,\"nodes\":{},\"edges\":[]}"); }, "arrays");
}
void invalid_nodes() {
    const std::vector<std::pair<std::string, std::string>> cases{
        {"\"id\":\"C\"", "\"id\":\"A\""},
        {"\"warehouse\"", "\"unknown\""},
        {"\"demand\":5", "\"demand\":-1"},
        {"\"earliest\":480", "\"earliest\":601"},
        {"\"earliest\":480", "\"earliest\":-1"},
        {"\"id\":\"A\"", "\"id\":\"\""}
    };
    for (const auto& item : cases) {
        expect_throw<ConfigError>([&] { (void)parse(changed(item.first, item.second)); }, "nodes[");
    }
}
void invalid_edges() {
    const std::vector<std::pair<std::string, std::string>> cases{
        {"\"to_id\":\"B\"", "\"to_id\":\"missing\""},
        {"\"to_id\":\"B\"", "\"to_id\":\"A\""},
        {"\"distance\":1.5", "\"distance\":-1"},
        {"\"time\":3", "\"time\":-3"},
        {"\"cost\":2", "\"cost\":-2"},
        {"\"congested\"", "\"unknown\""},
        {"{\"from_id\":\"B\"", "{\"id\":\"AB\",\"from_id\":\"B\""},
        {"\"from_id\":\"B\",\"to_id\":\"A\"", "\"from_id\":\"A\",\"to_id\":\"B\""}
    };
    for (const auto& item : cases) {
        expect_throw<ConfigError>([&] { (void)parse(changed(item.first, item.second)); }, "edges[");
    }
    auto existing = parse(valid_json);
    expect_throw<ConfigError>([&] { existing = parse(changed("\"cost\":2", "\"cost\":-1")); });
    check(existing.edge_count() == 2, "failed reload must leave existing graph unchanged");
}
void file_and_size_errors() {
    expect_throw<ConfigError>([] { (void)ConfigLoader::load_network("tests/no_such_network_file.json"); }, "cannot open");
    expect_throw<ConfigError>([] { (void)parse(valid_json, true); }, "course network");
    const auto source = ConfigLoader::load_network("config/network.json", true);
    for (const auto type : {NodeType::Warehouse, NodeType::DeliveryPoint, NodeType::TransferStation}) {
        auto graph = source;
        std::vector<NodeId> ids;
        for (const auto& item : graph.nodes()) { if (item.second.type() == type) { ids.push_back(item.first); } }
        for (const auto& id : ids) {
            graph.remove_node(id);
            graph.add_node(Node(id, type == NodeType::Warehouse ? NodeType::DeliveryPoint : NodeType::Warehouse, 0, 0));
        }
        // Keep edge count high enough to isolate type-count validation.
        std::vector<NodeId> all_ids;
        for (const auto& item : graph.nodes()) { all_ids.push_back(item.first); }
        int number = 0;
        for (const auto& from : all_ids) {
            for (const auto& to : all_ids) {
                if (from != to && !graph.find_edge(from, to)) {
                    graph.add_edge(Edge("test" + std::to_string(++number), from, to, 1, 1, 1));
                }
            }
        }
        expect_throw<ConfigError>([&] { ConfigLoader::validate_course_size(graph); }, "course network");
    }
    auto no_edges = source;
    for (const auto& item : source.nodes()) {
        for (const auto& edge : source.outgoing_edges(item.first)) { no_edges.remove_edge(edge.from_id(), edge.to_id()); }
    }
    expect_throw<ConfigError>([&] { ConfigLoader::validate_course_size(no_edges); }, "course network");
}
void demo_network() {
    const auto graph = ConfigLoader::load_network("config/network.json", true);
    check(graph.node_count() == 30 && graph.edge_count() == 82, "demo totals");
    check(graph.node_count(NodeType::Warehouse) == 2 && graph.node_count(NodeType::DeliveryPoint) == 25 &&
          graph.node_count(NodeType::TransferStation) == 3, "demo types");
    // From every node, all nodes are reachable: delivery and return are possible.
    for (const auto& start : graph.nodes()) {
        std::set<NodeId> visited{start.first};
        std::vector<NodeId> pending{start.first};
        while (!pending.empty()) {
            const auto id = pending.back();
            pending.pop_back();
            for (const auto& edge : graph.outgoing_edges(id)) {
                if (visited.insert(edge.to_id()).second) { pending.push_back(edge.to_id()); }
            }
        }
        check(visited.size() == graph.node_count(), "demo must be strongly connected");
    }
}
}  // namespace
int main(int argc, char* argv[]) {
    return run_tests(argc, argv, {{"valid_and_defaults", valid_and_defaults}, {"malformed_json", malformed_json},
        {"invalid_schema", invalid_schema}, {"invalid_nodes", invalid_nodes}, {"invalid_edges", invalid_edges},
        {"file_and_size_errors", file_and_size_errors}, {"demo_network", demo_network}});
}
