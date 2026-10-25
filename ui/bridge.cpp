#include "logistics/config/ConfigLoader.h"
#include "logistics/config/OrderConfigLoader.h"
#include "logistics/simulation/FleetSimulation.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

using namespace logistics;
using json = nlohmann::json;
template<class T> json nullable(const std::optional<T>& v) { return v ? json(*v) : json(nullptr); }
json action_json(const OrderAction& action, const DeliveryRecord& record) {
    const bool pickup = action.kind == OrderActionKind::Pickup;
    return {{"kind", pickup ? "pickup" : "delivery"}, {"order_id", record.order_id},
            {"node_id", pickup ? record.pickup_node_id : record.node_id}};
}
class FleetSession {
    Graph initial_graph_;
    std::vector<Order> initial_orders_;
    std::unique_ptr<FleetSimulation> fleet_;
    std::vector<json> rejected_;
    std::set<std::string> requested_;
    std::size_t generated_{0};
public:
    FleetSession(const std::filesystem::path& network, const std::filesystem::path& orders)
        : initial_graph_(ConfigLoader::load_network(network)), initial_orders_(OrderConfigLoader::load(orders).initial_orders) { reset(); }
    void reset() {
        SimulationOptions o; o.step_minutes = 1; o.capacity_kg = 40;
        auto candidate = std::make_unique<FleetSimulation>(initial_graph_,
            std::vector<FleetVehicleSpec>{{"V01", "W01", 40}, {"V02", "W01", 40}}, initial_orders_, 480, o);
        for (const auto& assignment : candidate->assignments()) {
            if (!assignment.accepted) { throw std::invalid_argument("initial order cannot be assigned: " + assignment.order_id + ": " + assignment.reason); }
        }
        candidate->enable_traffic(); fleet_ = std::move(candidate);
        latest_insertion_ = nullptr; rejected_.clear(); requested_.clear(); generated_ = 0;
        for (const auto& order : initial_orders_) { requested_.insert(order.id()); }
    }
    json state() const {
        json nodes = json::array(), edges = json::array(), vehicles = json::array(), orders = json::array(), events = json::array();
        std::vector<NodeId> sorted;
        for (const auto& entry : fleet_->network().nodes()) { sorted.push_back(entry.first); }
        std::sort(sorted.begin(), sorted.end());
        for (const auto& id : sorted) {
            const auto& n = *fleet_->network().find_node(id);
            nodes.push_back({{"id", id}, {"type", to_string(n.type())}, {"x", n.x()}, {"y", n.y()}});
            for (const auto& e : fleet_->network().outgoing_edges(id)) {
                edges.push_back({{"id", e.id()}, {"from_id", e.from_id()}, {"to_id", e.to_id()}, {"time", e.time()},
                    {"base_time", e.base_time()}, {"distance", e.distance()}, {"cost", e.cost()}, {"traffic_status", to_string(e.traffic_status())}});
            }
        }
        for (const auto& spec : fleet_->vehicle_specs()) {
            const auto& sim = fleet_->vehicle(spec.id); const auto f = sim.snapshot();
            json tasks = json::array(), traversals = json::array(), replan = nullptr;
            for (const auto& action : sim.remaining_actions()) {
                const auto found = std::find_if(sim.deliveries().begin(), sim.deliveries().end(), [&](const DeliveryRecord& d) { return d.order_id == action.order_id; });
                if (found == sim.deliveries().end()) { throw std::logic_error("unknown task"); }
                if (found->delivered_at || (action.kind == OrderActionKind::Pickup && found->picked_up_at)) { continue; }
                tasks.push_back(action_json(action, *found));
            }
            if (!f.finished) { tasks.push_back({{"kind", "return"}, {"order_id", nullptr}, {"node_id", spec.warehouse}}); }
            for (const auto& d : sim.deliveries()) {
                json window = d.window ? json{{"earliest", d.window->earliest}, {"latest", d.window->latest}} : json(nullptr);
                orders.push_back({{"id", d.order_id}, {"vehicle_id", spec.id}, {"destination", d.node_id}, {"warehouse", d.pickup_node_id}, {"pickup_node_id", d.pickup_node_id},
                    {"quantity", d.quantity}, {"priority", d.priority == OrderPriority::Urgent ? "urgent" : "normal"}, {"accepted", true},
                    {"window", window}, {"completed", nullable(d.delivered_at)}, {"pickup", nullable(d.picked_up_at)},
                    {"arrival", nullable(d.arrival)}, {"service", nullable(d.service_start)}, {"late_minutes", d.late_minutes}, {"penalty", d.penalty_points}});
            }
            for (const auto& e : sim.events()) {
                if (e.kind == "arrive") { traversals.push_back(e.detail); }
                if (e.kind == "pickup" || e.kind == "delivered_on_time" || e.kind == "delivered_late" || e.kind == "returned" || e.kind == "window_risk") {
                    events.push_back({{"time", e.state.time}, {"vehicle_id", spec.id}, {"kind", e.kind}, {"detail", e.detail}});
                }
            }
            for (auto i = sim.replanning_records().rbegin(); i != sim.replanning_records().rend(); ++i) {
                if (i->searched_legs) {
                    replan = {{"time", i->time}, {"before", i->before}, {"after", i->after}, {"searched", i->searched_legs}, {"reason", i->reason}};
                    break;
                }
            }
            json segment = nullptr;
            if (sim.vehicle().segment()) {
                const auto& s = *sim.vehicle().segment();
                segment = {{"id", s.road_id}, {"total", s.duration}, {"remaining", s.remaining}, {"arrival", f.time + s.remaining}};
            }
            vehicles.push_back({{"id", spec.id}, {"capacity", spec.capacity_kg}, {"load", f.load}, {"status", to_string(f.status)},
                {"node_id", f.node_id.value_or("")}, {"from_id", f.from_id}, {"to_id", f.to_id}, {"edge_id", nullable(f.edge_id)},
                {"x", f.x}, {"y", f.y}, {"progress", f.progress}, {"remaining", f.remaining_minutes}, {"segment", segment},
                {"finished", f.finished}, {"completed_at", nullable(sim.completed_at())}, {"next_order", f.next_order}, {"next_arrival", nullable(f.next_arrival)},
                {"return_eta", nullable(f.estimated_return)}, {"delivered", f.delivered_count}, {"late", f.late_count}, {"penalty", f.penalty_points},
                {"total_orders", sim.deliveries().size()}, {"future_nodes", f.future_nodes}, {"future_roads", f.future_roads},
                {"traversals", traversals}, {"tasks", tasks}, {"current_task", tasks.empty() ? json(nullptr) : tasks.front()},
                {"replan", replan}, {"revision", f.route_revision}});
        }
        for (const auto& o : rejected_) { orders.push_back(o); }
        for (const auto& a : fleet_->assignments()) {
            events.push_back({{"time", a.time}, {"vehicle_id", a.vehicle_id.value_or("")}, {"kind", a.accepted ? "assigned" : "rejected"},
                {"order_id", a.order_id}, {"detail", a.reason}, {"path", a.path}});
        }
        const auto& reference = fleet_->vehicle("V01").traffic_changes();
        std::map<Minutes, std::size_t> batches;
        for (const auto& c : reference) { if (c.before != c.after) { ++batches[c.at]; } }
        for (const auto& b : batches) { events.push_back({{"time", b.first}, {"vehicle_id", ""}, {"kind", "traffic"}, {"count", b.second}, {"detail", "shared road update"}}); }
        std::stable_sort(events.begin(), events.end(), [](const json& a, const json& b) { return a.at("time").get<double>() > b.at("time").get<double>(); });
        if (events.size() > 240) { events.erase(events.begin() + 240, events.end()); }
        json pickup_points = json::array();
        for (const auto& id : sorted) {
            if (fleet_->network().find_node(id)->type() == NodeType::TransferStation) {
                pickup_points.push_back({{"node_id", id}, {"waiting_orders", fleet_->waiting_pickups(id)}});
            }
        }
        return {{"ok", true}, {"time", fleet_->clock().now()}, {"finished", fleet_->finished()}, {"nodes", nodes}, {"edges", edges},
            {"vehicles", vehicles}, {"orders", orders}, {"events", events}, {"traffic_version", reference.size()},
            {"app", "logistics-route-planner"}, {"ui_version", 2}, {"demo", false}, {"engine", "C++17 FleetSimulation"},
            {"latest_insertion", latest_insertion_}, {"pickup_points", pickup_points},
            {"penalty_rule", {{"rate_per_minute", SimulationOptions{}.late_penalty_per_minute}, {"rounding", "ceil"}, {"based_on", "service_start"}}}};
    }
    json command(const json& input) {
        const auto cmd = input.at("cmd").get<std::string>(); json insertion = nullptr;
        if (cmd == "reset") { reset(); }
        else if (cmd == "advance") {
            const auto minutes = input.at("minutes").get<double>();
            if (!std::isfinite(minutes) || minutes <= 0 || minutes > 10) { throw std::invalid_argument("advance must be in (0,10]"); }
            fleet_->advance(minutes);
        } else if (cmd == "traffic") {
            const auto road = input.at("road_id").get<std::string>();
            const auto* edge = fleet_->network().find_edge_by_id(road);
            if (!edge) { throw std::invalid_argument("unknown road"); }
            const auto duration = input.contains("minutes") ? input.at("minutes").get<double>() : edge->time() * 1.8;
            fleet_->apply_traffic({{road, duration, TrafficStatus::Congested}});
        } else if (cmd == "insert") {
            const auto& o = input.at("order");
            auto id = o.value("id", std::string{});
            if (id.empty()) { do { id = "MANUAL" + std::to_string(++generated_); } while (requested_.count(id)); }
            if (id.size() > 40) { throw std::invalid_argument("order ID too long"); }
            const auto quantity = o.at("quantity").get<double>();
            if (!std::isfinite(quantity) || quantity <= 0) { throw std::invalid_argument("quantity must be positive"); }
            const auto priority = o.value("priority", "urgent");
            if (priority != "urgent" && priority != "normal") { throw std::invalid_argument("invalid priority"); }
            const TimeWindow window{o.at("earliest").get<double>(), o.at("latest").get<double>()};
            if (o.contains("pickup_node_id") && o.contains("warehouse")) { throw std::invalid_argument("ambiguous pickup source"); }
            const Order order(id, o.at(o.contains("pickup_node_id") ? "pickup_node_id" : "warehouse").get<std::string>(), o.at("destination").get<std::string>(), quantity, window,
                priority == "urgent" ? OrderPriority::Urgent : OrderPriority::Normal, fleet_->clock().now());
            std::map<std::string, std::vector<NodeId>> before;
            for (const auto& spec : fleet_->vehicle_specs()) { before.emplace(spec.id, fleet_->vehicle(spec.id).snapshot().future_nodes); }
            const auto result = fleet_->insert_order(order); requested_.insert(id);
            if (result.accepted) {
                latest_insertion_ = {{"time", fleet_->clock().now()}, {"id", id}, {"vehicle", *result.vehicle_id},
                    {"before", before.at(*result.vehicle_id)}, {"after", result.path}};
            } else {
                rejected_.push_back({{"id", id}, {"vehicle_id", nullptr}, {"destination", order.delivery_node_id()}, {"warehouse", order.pickup_node_id()}, {"pickup_node_id", order.pickup_node_id()},
                    {"quantity", quantity}, {"priority", priority}, {"window", {{"earliest", window.earliest}, {"latest", window.latest}}},
                    {"accepted", false}, {"reason", result.reason}, {"completed", nullptr}, {"pickup", nullptr}, {"late_minutes", 0}, {"penalty", 0}});
            }
            insertion = {{"accepted", result.accepted}, {"id", id}, {"vehicle_id", nullable(result.vehicle_id)}, {"reason", result.reason}, {"path", result.path}};
        } else if (cmd != "state") { throw std::invalid_argument("unknown command"); }
        auto output = state(); output["insertion"] = insertion; return output;
    }
private:
    json latest_insertion_ = nullptr;
};
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc != 3) { return 2; }
    try {
        FleetSession session(argv[1], argv[2]); std::string line;
        while (std::getline(std::cin, line)) {
            try { std::cout << session.command(json::parse(line)).dump() << '\n' << std::flush; }
            catch (const std::exception& error) { std::cout << json{{"ok", false}, {"error", error.what()}}.dump() << '\n' << std::flush; }
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
