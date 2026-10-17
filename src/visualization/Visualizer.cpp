#include "logistics/visualization/Visualizer.h"
#include "logistics/detail/checked_math.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace logistics {
namespace {

std::string csv_string(const std::string& value) {
    std::string escaped = "\"";
    for (const char character : value) {
        if (character == '"') { escaped += '"'; }
        escaped += character;
    }
    return escaped + '"';
}

std::ofstream csv_file(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary);
    if (!output) { throw std::runtime_error("cannot create CSV file: " + path.u8string()); }
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    return output;
}

bool near(double left, double right) {
    return std::isfinite(left) && std::isfinite(right) &&
           std::abs(left - right) <= 1e-9 * std::max({1.0, std::abs(left), std::abs(right)});
}

const char* role_name(StopRole role) {
    switch (role) {
    case StopRole::Start: return "start";
    case StopRole::Delivery: return "delivery";
    case StopRole::Return: return "return";
    }
    throw std::invalid_argument("invalid stop role");
}

struct Totals {
    double distance{0.0};
    double time{0.0};
    double cost{0.0};
};

std::vector<Totals> validate_plan(const Graph& graph, const RoutePlan& plan) {
    if (plan.status != PlanStatus::Feasible || plan.node_sequence.empty() ||
        plan.node_sequence.size() != plan.road_sequence.size() + 1 ||
        plan.node_sequence.front() != plan.start_node_id || plan.node_sequence.back() != plan.warehouse_id ||
        !std::isfinite(plan.departure_time) || plan.departure_time < 0.0) {
        throw std::invalid_argument("export requires a complete feasible route");
    }
    const auto* home = graph.find_node(plan.warehouse_id);
    if (!home || home->type() != NodeType::Warehouse || !graph.find_node(plan.start_node_id)) {
        throw std::invalid_argument("route endpoints no longer exist or warehouse type changed");
    }
    if ((plan.strategy != RouteStrategy::ShortestDistance && plan.strategy != RouteStrategy::LowestCost) ||
        plan.stops.size() < 2 || plan.stops.front().role != StopRole::Start ||
        plan.stops.front().route_node_index != 0 || plan.stops.back().role != StopRole::Return ||
        plan.stops.back().route_node_index != plan.node_sequence.size() - 1 ||
        !plan.unassigned_delivery_node_ids.empty() || !plan.unassigned_order_ids.empty()) {
        throw std::invalid_argument("export requires valid strategy, start/return stops and no unassigned deliveries");
    }
    std::vector<Totals> totals(1);
    for (std::size_t index = 0; index < plan.road_sequence.size(); ++index) {
        const auto* edge = graph.find_edge_by_id(plan.road_sequence[index]);
        if (!edge || edge->from_id() != plan.node_sequence[index] ||
            edge->to_id() != plan.node_sequence[index + 1] || edge->traffic_status() == TrafficStatus::Closed) {
            throw std::invalid_argument("route contains a missing, closed or disconnected road");
        }
        const auto previous = totals.back();
        totals.push_back({detail::checked_sum(previous.distance, edge->distance()),
                          detail::checked_sum(previous.time, edge->time()),
                          detail::checked_sum(previous.cost, edge->cost())});
    }
    if (!near(totals.back().distance, plan.total_distance_km) ||
        !near(totals.back().time, plan.total_travel_minutes) || !near(totals.back().cost, plan.total_cost_yuan)) {
        throw std::invalid_argument("route totals do not match the current network; replan before export");
    }
    std::set<NodeId> delivered;
    std::size_t previous_step = 0;
    for (const auto& stop : plan.stops) {
        if (stop.route_node_index >= plan.node_sequence.size() ||
            stop.route_node_index < previous_step ||
            stop.node_id != plan.node_sequence[stop.route_node_index] ||
            !near(stop.estimated_arrival, detail::checked_sum(plan.departure_time, totals[stop.route_node_index].time)) ||
            !near(stop.estimated_delivery, stop.estimated_arrival)) {
            throw std::invalid_argument("stop does not match the traversed route");
        }
        previous_step = stop.route_node_index;
        (void)role_name(stop.role);
        if (stop.role == StopRole::Delivery &&
            (graph.find_node(stop.node_id)->type() != NodeType::DeliveryPoint ||
             !delivered.insert(stop.node_id).second)) {
            throw std::invalid_argument("invalid or duplicate delivery stop");
        }
    }
    const std::set<NodeId> requested(plan.requested_delivery_node_ids.begin(), plan.requested_delivery_node_ids.end());
    if (requested.size() != plan.requested_delivery_node_ids.size() || requested != delivered) {
        throw std::invalid_argument("export route does not cover the requested deliveries");
    }
    return totals;
}

}  // namespace

void Visualizer::export_csv(const Graph& graph, const RoutePlan& plan, const std::filesystem::path& directory) {
    const auto totals = validate_plan(graph, plan);
    std::filesystem::create_directories(directory);
    std::vector<NodeId> node_ids;
    for (const auto& entry : graph.nodes()) { node_ids.push_back(entry.first); }
    std::sort(node_ids.begin(), node_ids.end());

    auto nodes = csv_file(directory / "nodes.csv");
    nodes << "id,type,x,y,demand,earliest,latest\n";
    auto edges = csv_file(directory / "edges.csv");
    edges << "id,from_id,to_id,distance,time,cost,traffic_status\n";
    for (const auto& id : node_ids) {
        const auto* node = graph.find_node(id);
        nodes << csv_string(id) << ',' << to_string(node->type()) << ',' << node->x() << ',' << node->y() << ',' << node->demand() << ',';
        if (node->time_window()) { nodes << node->time_window()->earliest; }
        nodes << ',';
        if (node->time_window()) { nodes << node->time_window()->latest; }
        nodes << '\n';
        std::vector<const Edge*> outgoing;
        for (const auto& edge : graph.outgoing_edges(id)) { outgoing.push_back(&edge); }
        std::sort(outgoing.begin(), outgoing.end(), [](const Edge* a, const Edge* b) { return a->to_id() < b->to_id(); });
        for (const auto* edge : outgoing) {
            edges << csv_string(edge->id()) << ',' << csv_string(edge->from_id()) << ',' << csv_string(edge->to_id())
                  << ',' << edge->distance() << ',' << edge->time() << ',' << edge->cost() << ',' << to_string(edge->traffic_status()) << '\n';
        }
    }
    auto route = csv_file(directory / "route.csv");
    route << "step,node_id,incoming_edge_id,arrival_minutes,cumulative_distance,cumulative_time,cumulative_cost\n";
    for (std::size_t index = 0; index < plan.node_sequence.size(); ++index) {
        route << index << ',' << csv_string(plan.node_sequence[index]) << ','
              << csv_string(index == 0 ? "" : plan.road_sequence[index - 1]) << ','
              << detail::checked_sum(plan.departure_time, totals[index].time) << ','
              << totals[index].distance << ',' << totals[index].time << ',' << totals[index].cost << '\n';
    }
    auto stops = csv_file(directory / "stops.csv");
    stops << "stop_index,role,node_id,route_step,arrival_minutes,delivery_minutes\n";
    for (std::size_t index = 0; index < plan.stops.size(); ++index) {
        const auto& stop = plan.stops[index];
        stops << index << ',' << role_name(stop.role) << ',' << csv_string(stop.node_id) << ',' << stop.route_node_index
              << ',' << stop.estimated_arrival << ',' << stop.estimated_delivery << '\n';
    }
    auto summary = csv_file(directory / "summary.csv");
    summary << "strategy,method,status,start,warehouse,departure_minutes,delivery_count,total_distance,total_time,total_cost\n"
            << (plan.strategy == RouteStrategy::ShortestDistance ? "distance" : "cost")
            << ",nearest_neighbor_dijkstra,feasible," << csv_string(plan.start_node_id) << ',' << csv_string(plan.warehouse_id)
            << ',' << plan.departure_time << ',' << plan.requested_delivery_node_ids.size()
            << ',' << plan.total_distance_km << ',' << plan.total_travel_minutes << ',' << plan.total_cost_yuan << '\n';
    nodes.flush();
    edges.flush();
    route.flush();
    stops.flush();
    summary.flush();
}

}  // namespace logistics
