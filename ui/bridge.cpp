#include "logistics/config/ConfigLoader.h"
#include "logistics/config/OrderConfigLoader.h"
#include "logistics/simulation/Simulation.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>

using json = nlohmann::json;
using namespace logistics;
template<class T> json nullable(const std::optional<T>& value) { return value ? json(*value) : json(nullptr); }

class SimulationSession {
    Graph graph_;
    std::vector<Order> initial_;
    std::unique_ptr<Simulation> sim_;
    std::map<OrderId, json> requests_;
    std::vector<json> rejected_;
    std::vector<OrderAction> planned_actions_; // Actual stop order returned by the planner.
    std::size_t generated_{0};
public:
    SimulationSession(const std::filesystem::path& network, const std::filesystem::path& scenario)
        : graph_(ConfigLoader::load_network(network)),
          initial_(OrderConfigLoader::load(scenario).initial_orders) { reset(); }
    void reset() {
        SimulationOptions options; options.capacity_kg = 100; options.step_minutes = 1;
        auto candidate = std::make_unique<Simulation>(graph_, "W01", initial_, 480, options);
        candidate->enable_traffic();
        sim_ = std::move(candidate); requests_.clear(); rejected_.clear(); generated_ = 0;
        planned_actions_.clear();
        for (const auto& stop : sim_->route_revisions().back().route.stops) {
            planned_actions_.push_back(stop.action);
        }
        for (const auto& order : initial_) {
            requests_[order.id()] = {{"id",order.id()},{"warehouse_id",order.warehouse_id()},
                {"delivery_node_id",order.delivery_node_id()},{"quantity",order.quantity()},
                {"priority","normal"},{"release_time",480},{"accepted",true},
                {"time_window",{{"earliest",order.time_window()->earliest},{"latest",order.time_window()->latest}}}};
        }
    }
    json state() const {
        const auto f = sim_->snapshot();
        json nodes = json::array(), edges = json::array(), orders = json::array(), events = json::array();
        for (const auto& entry : sim_->network().nodes()) {
            const auto& n = entry.second;
            nodes.push_back({{"id",n.id()},{"type",to_string(n.type())},{"x",n.x()},{"y",n.y()}});
            for (const auto& e : sim_->network().outgoing_edges(n.id())) {
                edges.push_back({{"id",e.id()},{"from_id",e.from_id()},{"to_id",e.to_id()},
                    {"time",e.time()},{"base_time",e.base_time()},{"distance",e.distance()},
                    {"cost",e.cost()},{"traffic_status",to_string(e.traffic_status())}});
            }
        }
        for (const auto& record : sim_->deliveries()) {
            auto o = requests_.at(record.order_id);
            o["completed"] = nullable(record.delivered_at); o["service"] = nullable(record.service_start);
            o["pickup"] = nullable(record.picked_up_at); o["late_minutes"] = record.late_minutes;
            orders.push_back(std::move(o));
        }
        for (const auto& rejected : rejected_) { orders.push_back(rejected); }
        json tasks = json::array();
        for (const auto& action : planned_actions_) {
            const auto found = std::find_if(sim_->deliveries().begin(), sim_->deliveries().end(),
                [&](const DeliveryRecord& delivery) { return delivery.order_id == action.order_id; });
            if (found == sim_->deliveries().end()) { throw std::logic_error("task references an unknown accepted order"); }
            const bool pickup = action.kind == OrderActionKind::Pickup;
            if (found->delivered_at || (pickup && found->picked_up_at)) { continue; }
            tasks.push_back({{"kind",pickup ? "pickup" : "delivery"},{"order_id",action.order_id},
                {"node_id",pickup ? found->warehouse_id : found->node_id}});
        }
        if (!f.finished) { tasks.push_back({{"kind","return"},{"order_id",nullptr},{"node_id","W01"}}); }
        const json current_task = tasks.empty() ? json(nullptr) : tasks.front();
        json traversals = json::array(), last_trip = nullptr;
        std::optional<SimulationSnapshot> departure;
        for (const auto& e : sim_->events()) {
            if (e.kind == "depart") { departure = e.state; }
            if (e.kind == "arrive") {
                traversals.push_back(e.detail);
                if (departure && departure->edge_id && *departure->edge_id == e.detail) {
                    last_trip = {{"road_id",e.detail},{"entered_at",departure->time},
                        {"arrived_at",e.state.time},{"actual_minutes",e.state.time-departure->time}};
                }
            }
            // Keep the display compact without losing manual-order and route events.
            if (e.kind != "traffic_change" && e.kind != "depart") {
                events.push_back({{"time",e.state.time},{"kind",e.kind},{"detail",e.detail}});
            }
        }
        if (events.size() > 16) { events.erase(events.begin(), events.end() - 16); }
        json segment = nullptr;
        if (sim_->vehicle().segment()) {
            const auto& s = *sim_->vehicle().segment();
            segment = {{"id",s.road_id},{"total",s.duration},{"remaining",s.remaining},
                {"entered_at",f.time-(s.duration-s.remaining)},{"arrival_at",f.time+s.remaining}};
        }
        json replan = nullptr;
        for (auto i = sim_->replanning_records().rbegin(); i != sim_->replanning_records().rend(); ++i) {
            if (i->searched_legs) { replan = {{"time",i->time},{"before",i->before},{"after",i->after},{"searched",i->searched_legs}}; break; }
        }
        const auto stats = sim_->path_cache_stats();
        return {{"ok",true},{"app","logistics-route-planner"},{"ui_version",1},{"nodes",nodes},{"edges",edges},{"orders",orders},{"events",events},
            {"traversals",traversals},{"capacity",100},{"segment",segment},{"last_trip",last_trip},{"replan",replan},
            {"current_task",current_task},{"task_path",tasks},
            {"cache",{{"hits",stats.hits},{"misses",stats.misses}}},
            {"frame",{{"time",f.time},{"status",to_string(f.status)},{"node_id",f.node_id.value_or("")},
            {"edge_id",f.edge_id.value_or("")},{"from_id",f.from_id},{"to_id",f.to_id},
            {"remaining_minutes",f.remaining_minutes},{"progress",f.progress},{"x",f.x},{"y",f.y},{"load",f.load},
            {"delivered_count",f.delivered_count},{"late_count",f.late_count},{"penalty_points",f.penalty_points},
            {"finished",f.finished},{"route_revision",f.route_revision},{"estimated_return",nullable(f.estimated_return)},
            {"next_order",f.next_order},{"next_arrival",nullable(f.next_arrival)},{"traffic_version",f.traffic_version},
            {"completed_traversals",traversals.size()},{"future",f.future_roads},{"future_nodes",f.future_nodes}}}};
    }
    json command(const json& input) {
        const auto cmd = input.at("cmd").get<std::string>();
        json insertion = nullptr;
        if (cmd == "reset") { reset(); }
        else if (cmd == "advance") {
            const auto minutes = input.at("minutes").get<double>();
            if (!std::isfinite(minutes) || minutes <= 0 || minutes > 10) { throw std::invalid_argument("advance minutes must be in (0,10]"); }
            if (!sim_->finished()) { sim_->advance(minutes); }
        } else if (cmd == "traffic") {
            // Deterministic hook for checking entry-time locking, also usable in tests.
            const auto id = input.at("road_id").get<std::string>();
            const auto minutes = input.at("minutes").get<double>();
            sim_->apply_traffic({{id,minutes,TrafficStatus::Congested}});
        } else if (cmd == "insert") {
            auto o = input.at("order");
            const auto id = o.contains("id") ? o.at("id").get<std::string>()
                                            : "MANUAL" + std::to_string(++generated_);
            const auto priority = o.value("priority", "urgent");
            if (priority != "urgent" && priority != "normal") { throw std::invalid_argument("unknown priority"); }
            const double quantity = o.at("quantity").get<double>();
            if (!std::isfinite(quantity) || quantity <= 0) { throw std::invalid_argument("quantity must be positive"); }
            const TimeWindow window{o.at("earliest").get<double>(),o.at("latest").get<double>()};
            const Order order(id,o.at("warehouse").get<std::string>(),o.at("destination").get<std::string>(),
                quantity,window,priority=="urgent"?OrderPriority::Urgent:OrderPriority::Normal,sim_->clock().now());
            auto record = json{{"id",id},{"warehouse_id",order.warehouse_id()},
                {"delivery_node_id",order.delivery_node_id()},{"quantity",quantity},{"priority",priority},
                {"release_time",sim_->clock().now()},{"completed",nullptr},{"service",nullptr},{"pickup",nullptr},
                {"time_window",{{"earliest",window.earliest},{"latest",window.latest}}}};
            const auto before = sim_->snapshot();
            const bool locked_delivery = before.status == VehicleStatus::Waiting || before.status == VehicleStatus::Unloading;
            const auto result = sim_->insert_order(order);
            record["accepted"] = result.accepted; record["reason"] = result.reason;
            if (result.accepted) {
                requests_[id] = record;
                planned_actions_.clear();
                // Insertion starts after a committed waiting/unloading phase. Keep that action first.
                if (locked_delivery && !before.next_order.empty()) {
                    planned_actions_.push_back({before.next_order,OrderActionKind::Delivery});
                }
                for (const auto& stop : result.route.stops) { planned_actions_.push_back(stop.action); }
            } else { rejected_.push_back(record); }
            insertion = {{"accepted",result.accepted},{"id",id},{"reason",result.reason},{"path",result.route.nodes}};
        } else if (cmd != "state") { throw std::invalid_argument("unknown command"); }
        auto output = state(); output["insertion"] = insertion; return output;
    }
};
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc != 3) { return 2; }
    try {
        SimulationSession session(argv[1],argv[2]);
        std::string line;
        while (std::getline(std::cin,line)) {
            try { std::cout << session.command(json::parse(line)).dump() << '\n' << std::flush; }
            catch (const std::exception& e) { std::cout << json{{"ok",false},{"error",e.what()}}.dump() << '\n' << std::flush; }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
