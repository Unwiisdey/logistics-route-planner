#include "logistics/config/OrderConfigLoader.h"
#include "logistics/config/ConfigLoader.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>

namespace logistics {
namespace {
using Json = nlohmann::json;
void keys(const Json& item, const std::set<std::string>& allowed) {
    if (!item.is_object()) { throw ConfigError("expected object"); }
    for (auto it = item.begin(); it != item.end(); ++it) {
        if (!allowed.count(it.key())) { throw ConfigError("unknown field: " + it.key()); }
    }
}
double number(const Json& item, const char* name) {
    if (!item.at(name).is_number()) { throw ConfigError(std::string(name) + " must be a number"); }
    return item.at(name).get<double>();
}
Order read_order(const Json& item, bool dynamic) {
    keys(item, {"id", "warehouse_id", "delivery_node_id", "quantity", "time_window", "priority", "release_time"});
    std::optional<TimeWindow> window;
    if (!item.at("time_window").is_null()) {
        keys(item.at("time_window"), {"earliest", "latest"});
        window = TimeWindow{number(item.at("time_window"), "earliest"), number(item.at("time_window"), "latest")};
    }
    const auto priority = item.value("priority", dynamic ? "urgent" : "normal");
    if (priority != "urgent" && priority != "normal") { throw ConfigError("priority must be urgent or normal"); }
    const auto release = dynamic || item.contains("release_time") ? number(item, "release_time") : 0;
    return Order(item.at("id").get<std::string>(), item.at("warehouse_id").get<std::string>(),
                 item.at("delivery_node_id").get<std::string>(), number(item, "quantity"), window,
                 priority == "urgent" ? OrderPriority::Urgent : OrderPriority::Normal, release);
}
}  // namespace
OrderScenario OrderConfigLoader::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) { throw ConfigError("cannot open order scenario: " + path.u8string()); }
    return read(input);
}
OrderScenario OrderConfigLoader::read(std::istream& input) {
    try {
        std::vector<std::set<std::string>> object_keys;
        const auto callback = [&](int, Json::parse_event_t event, Json& value) {
            if (event == Json::parse_event_t::object_start) { object_keys.emplace_back(); }
            if (event == Json::parse_event_t::key && !object_keys.back().insert(value.get<std::string>()).second) {
                throw ConfigError("duplicate JSON key: " + value.get<std::string>());
            }
            if (event == Json::parse_event_t::object_end) { object_keys.pop_back(); }
            return true;
        };
        const auto root = Json::parse(input, callback);
        if (input.bad()) { throw ConfigError("order scenario read failed"); }
        keys(root, {"schema_version", "initial_orders", "dynamic_orders"});
        if (!root.at("schema_version").is_number_integer() || root.at("schema_version") != 1) { throw ConfigError("schema_version must be integer 1"); }
        OrderScenario scenario;
        std::set<OrderId> ids;
        for (const auto* group : {"initial_orders", "dynamic_orders"}) {
            if (!root.at(group).is_array()) { throw ConfigError(std::string(group) + " must be an array"); }
            const bool dynamic = std::string(group) == "dynamic_orders";
            std::size_t index = 0;
            for (const auto& item : root.at(group)) {
                try {
                    auto order = read_order(item, dynamic);
                    if (!ids.insert(order.id()).second) { throw ConfigError("duplicate order ID: " + order.id()); }
                    (dynamic ? scenario.dynamic_orders : scenario.initial_orders).push_back(std::move(order));
                } catch (const std::exception& error) {
                    throw ConfigError(std::string(group) + "[" + std::to_string(index) + "]: " + error.what());
                }
                ++index;
            }
        }
        std::stable_sort(scenario.dynamic_orders.begin(), scenario.dynamic_orders.end(),
                         [](const Order& a, const Order& b) { return a.release_time() < b.release_time(); });
        return scenario;
    } catch (const ConfigError&) { throw; }
    catch (const std::exception& error) { throw ConfigError(std::string("invalid order scenario: ") + error.what()); }
}
}  // namespace logistics
