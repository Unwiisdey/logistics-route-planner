#include "logistics/config/ConfigLoader.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <set>
#include <string>
#include <vector>

namespace logistics {
namespace {
using Json = nlohmann::json;

void check_keys(const Json& object, std::initializer_list<std::string> allowed,
                const std::string& context) {
    if (!object.is_object()) { throw ConfigError(context + ": expected an object"); }
    for (auto item = object.begin(); item != object.end(); ++item) {
        if (std::find(allowed.begin(), allowed.end(), item.key()) == allowed.end()) {
            throw ConfigError(context + ": unknown field '" + item.key() + "'");
        }
    }
}

const Json& field(const Json& object, const std::string& key, const std::string& context) {
    const auto found = object.find(key);
    if (found == object.end()) { throw ConfigError(context + ": missing field '" + key + "'"); }
    return *found;
}

std::string string_field(const Json& object, const std::string& key, const std::string& context) {
    const auto& value = field(object, key, context);
    if (!value.is_string()) { throw ConfigError(context + "." + key + ": expected a string"); }
    return value.get<std::string>();
}

double number_field(const Json& object, const std::string& key, const std::string& context) {
    const auto& value = field(object, key, context);
    if (!value.is_number()) { throw ConfigError(context + "." + key + ": expected a number"); }
    const auto result = value.get<double>();
    if (!std::isfinite(result)) { throw ConfigError(context + "." + key + ": expected a finite number"); }
    return result;
}

NodeType node_type(const std::string& name) {
    if (name == "warehouse") { return NodeType::Warehouse; }
    if (name == "delivery_point") { return NodeType::DeliveryPoint; }
    if (name == "transfer_station") { return NodeType::TransferStation; }
    throw std::invalid_argument("unknown node type: " + name);
}

TrafficStatus traffic_status(const std::string& name) {
    if (name == "clear") { return TrafficStatus::Clear; }
    if (name == "congested") { return TrafficStatus::Congested; }
    if (name == "closed") { return TrafficStatus::Closed; }
    throw std::invalid_argument("unknown traffic_status: " + name);
}
}  // namespace

Graph ConfigLoader::load_network(const std::filesystem::path& path, bool require_course_size) {
    std::ifstream input(path);
    if (!input) { throw ConfigError("cannot open network file: " + path.u8string()); }
    try {
        return read_network(input, require_course_size);
    } catch (const ConfigError& error) {
        throw ConfigError(path.u8string() + ": " + error.what());
    }
}

Graph ConfigLoader::read_network(std::istream& input, bool require_course_size) {
    try {
        // Reject duplicate JSON keys rather than silently using the last value.
        std::vector<std::set<std::string>> object_keys;
        const auto callback = [&](int, Json::parse_event_t event, Json& parsed) {
            if (event == Json::parse_event_t::object_start) { object_keys.emplace_back(); }
            if (event == Json::parse_event_t::key &&
                !object_keys.back().insert(parsed.get<std::string>()).second) {
                throw ConfigError("duplicate JSON key: " + parsed.get<std::string>());
            }
            if (event == Json::parse_event_t::object_end) { object_keys.pop_back(); }
            return true;
        };
        const Json root = Json::parse(input, callback);
        if (input.bad()) { throw ConfigError("network input read failed"); }
        check_keys(root, {"schema_version", "nodes", "edges"}, "root");
        const auto& version = field(root, "schema_version", "root");
        if (!version.is_number_integer() || version != 1) {
            throw ConfigError("schema_version must be integer 1");
        }
        const auto& nodes = field(root, "nodes", "root");
        const auto& edges = field(root, "edges", "root");
        if (!nodes.is_array() || !edges.is_array()) {
            throw ConfigError("nodes and edges must be arrays");
        }
        Graph graph;
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            const auto context = "nodes[" + std::to_string(index) + "]";
            const auto& item = nodes[index];
            check_keys(item, {"id", "type", "x", "y", "demand", "time_window"}, context);
            try {
                std::optional<TimeWindow> window;
                if (item.contains("time_window") && !item.at("time_window").is_null()) {
                    const auto& value = item.at("time_window");
                    const auto window_context = context + ".time_window";
                    check_keys(value, {"earliest", "latest"}, window_context);
                    window = TimeWindow{number_field(value, "earliest", window_context),
                                        number_field(value, "latest", window_context)};
                }
                graph.add_node(Node(string_field(item, "id", context),
                                    node_type(string_field(item, "type", context)),
                                    number_field(item, "x", context), number_field(item, "y", context),
                                    item.contains("demand") ? number_field(item, "demand", context) : 0.0,
                                    window));
            } catch (const std::invalid_argument& error) {
                throw ConfigError(context + ": " + error.what());
            }
        }
        for (std::size_t index = 0; index < edges.size(); ++index) {
            const auto context = "edges[" + std::to_string(index) + "]";
            const auto& item = edges[index];
            check_keys(item, {"id", "from_id", "to_id", "distance", "time", "cost", "traffic_status"}, context);
            try {
                const auto from = string_field(item, "from_id", context);
                const auto to = string_field(item, "to_id", context);
                const auto id = item.contains("id") ? string_field(item, "id", context) : from + "->" + to;
                const auto status = item.contains("traffic_status")
                    ? traffic_status(string_field(item, "traffic_status", context)) : TrafficStatus::Clear;
                graph.add_edge(Edge(id, from, to, number_field(item, "distance", context),
                                    number_field(item, "time", context), number_field(item, "cost", context), status));
            } catch (const std::invalid_argument& error) {
                throw ConfigError(context + ": " + error.what());
            }
        }
        if (require_course_size) { validate_course_size(graph); }
        return graph;
    } catch (const Json::exception& error) {
        throw ConfigError(std::string("invalid JSON: ") + error.what());
    } catch (const std::ios_base::failure& error) {
        throw ConfigError(std::string("network input read failed: ") + error.what());
    }
}

void ConfigLoader::validate_course_size(const Graph& graph) {
    if (graph.node_count() < 30 || graph.node_count(NodeType::Warehouse) < 2 ||
        graph.node_count(NodeType::DeliveryPoint) < 15 ||
        graph.node_count(NodeType::TransferStation) < 3 || graph.edge_count() < 50) {
        throw ConfigError("course network requires >=30 nodes, >=2 warehouses, >=15 delivery points, "
                          ">=3 transfer stations and >=50 directed edges");
    }
}
}  // namespace logistics
