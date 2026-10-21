#include "logistics/config/ConfigLoader.h"
#include "logistics/config/OrderConfigLoader.h"
#include "logistics/planning/Planner.h"
#include "logistics/visualization/Visualizer.h"
#include "logistics/simulation/Simulation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
double parse_number(const std::string& text, const std::string& option) {
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size()) { throw std::invalid_argument(option + " expects a number"); }
    return value;
}

std::vector<logistics::NodeId> parse_deliveries(const std::string& text) {
    std::vector<logistics::NodeId> ids;
    if (text.empty()) { return ids; }
    std::size_t start = 0;
    while (true) {
        const auto end = text.find(',', start);
        const auto id = text.substr(start, end == std::string::npos ? end : end - start);
        if (id.empty()) { throw std::invalid_argument("--deliveries contains an empty node ID"); }
        ids.push_back(id);
        if (end == std::string::npos) { break; }
        start = end + 1;
    }
    return ids;
}

void print_plan(const logistics::RoutePlan& plan) {
    std::cout << "Strategy: " << (plan.strategy == logistics::RouteStrategy::ShortestDistance ? "distance" : "cost")
              << " | Method: nearest-neighbor + Dijkstra\nDelivery order: ";
    for (std::size_t index = 0; index < plan.stops.size(); ++index) {
        if (index != 0) { std::cout << " -> "; }
        std::cout << plan.stops[index].node_id;
    }
    std::cout << "\nPath nodes: ";
    for (std::size_t index = 0; index < plan.node_sequence.size(); ++index) {
        if (index != 0) { std::cout << " -> "; }
        std::cout << plan.node_sequence[index];
    }
    std::cout << std::fixed << std::setprecision(2)
              << "\nTotal distance: " << plan.total_distance_km << " km\nTotal travel time: "
              << plan.total_travel_minutes << " min\nTotal cost: " << plan.total_cost_yuan << " CNY\n"
              << "Static route estimate excludes waiting/unloading; visit order is heuristic.\n";
}
}  // namespace

int main(int argc, char* argv[]) {
    try {
        std::filesystem::path path = "config/network.json";
        bool explicit_path = false;
        bool check_size = false;
        bool summary_only = false;
        std::optional<logistics::WeightMetric> matrix;
        bool plan_enabled = false;
        bool planning_options = false;
        bool simulate = false;
        bool simulation_options = false;
        bool explicit_departure = false;
        bool hard_windows = false;
        bool traffic_enabled = false;
        logistics::TrafficOptions traffic;
        std::optional<std::filesystem::path> scenario_path;
        logistics::SimulationOptions simulation;
        logistics::PlanningRequest request;
        std::optional<std::filesystem::path> export_directory;
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--help") {
                std::cout << "Usage: logistics_app [--config FILE] [--check-size]\n"
                             "  [--summary | --matrix distance|time|cost | --plan distance|cost]\n"
                             "Planning: [--warehouse ID] [--deliveries ID,ID,...] [--departure MINUTES]\n"
                             "          [--capacity KG] [--export DIRECTORY]\n"
                             "Simulation: --simulate [--step MINUTES] [--unload-minutes MINUTES]\n"
                             "            [--late-penalty POINTS] [--vehicle-id ID]\n"
                             "            [--cache-capacity 0..65536] (default 256; 0 disables cache)\n"
                             "            [--hard-windows | --scenario ORDER_JSON]\n"
                             "Traffic: --traffic [--traffic-interval SECONDS] [--traffic-fraction 0..1]\n"
                             "         [--traffic-seed UINT32] (defaults: 30 seconds, 0.1, 42)\n"
                             "Simulation defaults: start 480 (08:00), step 5, unload 2, capacity 1000,\n"
                             "fixed penalty 10 points per late order. Use --departure to change start.\n"
                             "Default: print adjacency list. Planning defaults to the first warehouse\n"
                             "by ID and all delivery points. Capacity is checked only when provided.\n";
                return 0;
            } else if (argument == "--config" && index + 1 < argc) {
                path = std::filesystem::path(argv[++index]);
                explicit_path = true;
            } else if (argument == "--matrix" && index + 1 < argc) {
                const std::string value = argv[++index];
                if (value == "distance") { matrix = logistics::WeightMetric::Distance; }
                else if (value == "time") { matrix = logistics::WeightMetric::Time; }
                else if (value == "cost") { matrix = logistics::WeightMetric::Cost; }
                else { throw std::invalid_argument("--matrix expects distance, time or cost"); }
            } else if (argument == "--summary") {
                summary_only = true;
            } else if (argument == "--check-size") {
                check_size = true;
            } else if (argument == "--plan" && index + 1 < argc) {
                const std::string value = argv[++index];
                if (value == "distance") { request.strategy = logistics::RouteStrategy::ShortestDistance; }
                else if (value == "cost") { request.strategy = logistics::RouteStrategy::LowestCost; }
                else { throw std::invalid_argument("--plan expects distance or cost"); }
                plan_enabled = true;
            } else if (argument == "--warehouse" && index + 1 < argc) {
                request.warehouse_id = argv[++index];
                planning_options = true;
            } else if (argument == "--deliveries" && index + 1 < argc) {
                request.delivery_node_ids = parse_deliveries(argv[++index]);
                planning_options = true;
            } else if (argument == "--departure" && index + 1 < argc) {
                request.departure_time = parse_number(argv[++index], argument);
                explicit_departure = true;
                planning_options = true;
            } else if (argument == "--simulate") {
                simulate = true;
                planning_options = true;
            } else if (argument == "--traffic") {
                traffic_enabled = true; simulation_options = true;
            } else if ((argument == "--traffic-interval" || argument == "--traffic-fraction" || argument == "--traffic-seed") && index + 1 < argc) {
                const double value = parse_number(argv[++index], argument);
                if (argument == "--traffic-interval") { traffic.interval_minutes = value / 60.0; }
                else if (argument == "--traffic-fraction") { traffic.fraction = value; }
                else {
                    if (!std::isfinite(value) || value < 0 || value > std::numeric_limits<std::uint32_t>::max() || std::floor(value) != value) {
                        throw std::invalid_argument("traffic seed must be a uint32 integer");
                    }
                    traffic.seed = static_cast<std::uint32_t>(value);
                }
                traffic_enabled = true; simulation_options = true;
            } else if (argument == "--hard-windows") {
                hard_windows = true;
                simulation_options = true;
            } else if (argument == "--scenario" && index + 1 < argc) {
                scenario_path = std::filesystem::path(argv[++index]);
                hard_windows = true;
                simulation_options = true;
            } else if (argument == "--cache-capacity" && index + 1 < argc) {
                const auto value = parse_number(argv[++index], argument);
                if (!std::isfinite(value) || value < 0 || value > 65536 || std::floor(value) != value) {
                    throw std::invalid_argument("cache capacity must be an integer in 0..65536");
                }
                simulation.path_cache_capacity = static_cast<std::size_t>(value);
                simulation_options = true;
            } else if (argument == "--step" && index + 1 < argc) {
                simulation.step_minutes = parse_number(argv[++index], argument);
                simulation_options = true;
            } else if (argument == "--unload-minutes" && index + 1 < argc) {
                simulation.unload_minutes = parse_number(argv[++index], argument);
                simulation_options = true;
            } else if (argument == "--late-penalty" && index + 1 < argc) {
                simulation.late_penalty_points = parse_number(argv[++index], argument);
                simulation_options = true;
            } else if (argument == "--vehicle-id" && index + 1 < argc) {
                simulation.vehicle_id = argv[++index];
                simulation_options = true;
            } else if (argument == "--capacity" && index + 1 < argc) {
                request.capacity_kg = parse_number(argv[++index], argument);
                planning_options = true;
            } else if (argument == "--export" && index + 1 < argc) {
                export_directory = std::filesystem::path(argv[++index]);
                planning_options = true;
            } else {
                throw std::invalid_argument("unknown or incomplete option: " + argument);
            }
        }
        if (summary_only && matrix) { throw std::invalid_argument("--summary and --matrix cannot be combined"); }
        if (plan_enabled && (summary_only || matrix)) { throw std::invalid_argument("--plan cannot be combined with --summary or --matrix"); }
        if (!plan_enabled && planning_options) { throw std::invalid_argument("planning options require --plan"); }
        if (!simulate && simulation_options) { throw std::invalid_argument("simulation options require --simulate"); }
        if (simulate) {
            if (!explicit_departure) { request.departure_time = 480; }
            simulation.capacity_kg = request.capacity_kg.value_or(simulation.capacity_kg);
            request.capacity_kg = simulation.capacity_kg;
        }
        if (!explicit_path && !std::filesystem::exists(path)) {
            path = std::filesystem::path(argv[0]).parent_path() / "config" / "network.json";
        }
        const auto graph = logistics::ConfigLoader::load_network(path, check_size);
        std::cout << "Logistics Route Planner v" << LOGISTICS_VERSION << '\n'
                  << "Nodes: " << graph.node_count() << " | Edges: " << graph.edge_count() << '\n'
                  << "Warehouses: " << graph.node_count(logistics::NodeType::Warehouse)
                  << " | Delivery points: " << graph.node_count(logistics::NodeType::DeliveryPoint)
                  << " | Transfer stations: " << graph.node_count(logistics::NodeType::TransferStation) << '\n';
        if (check_size) { std::cout << "Course size requirements: PASS\n"; }
        if (plan_enabled) {
            if (request.warehouse_id.empty()) {
                std::vector<logistics::NodeId> warehouses;
                for (const auto& entry : graph.nodes()) {
                    if (entry.second.type() == logistics::NodeType::Warehouse) { warehouses.push_back(entry.first); }
                }
                if (warehouses.empty()) { throw std::invalid_argument("network has no warehouse"); }
                request.warehouse_id = *std::min_element(warehouses.begin(), warehouses.end());
            }
            if (hard_windows) {
                logistics::OrderScenario scenario;
                if (scenario_path) {
                    if (request.delivery_node_ids) { throw std::invalid_argument("--scenario cannot be combined with --deliveries"); }
                    scenario = logistics::OrderConfigLoader::load(*scenario_path);
                } else {
                    const auto basic = logistics::Planner::plan(graph, request);
                    if (basic.status != logistics::PlanStatus::Feasible) { throw std::invalid_argument(basic.diagnostic); }
                    for (const auto& id : basic.requested_delivery_node_ids) {
                        const auto* node = graph.find_node(id);
                        scenario.initial_orders.emplace_back("order:" + id, request.warehouse_id, id, node->demand(), node->time_window());
                    }
                }
                for (const auto& order : scenario.dynamic_orders) {
                    if (order.release_time() < request.departure_time) { throw std::invalid_argument("dynamic order release precedes simulation start"); }
                }
                logistics::Simulation run(graph, request.warehouse_id, scenario.initial_orders, request.departure_time, simulation, request.strategy);
                if (traffic_enabled) { run.enable_traffic(traffic); }
                std::size_t next_order = 0;
                while (!run.finished() || next_order < scenario.dynamic_orders.size()) {
                    if (next_order < scenario.dynamic_orders.size() && scenario.dynamic_orders[next_order].release_time() <= run.clock().now()) {
                        run.insert_order(scenario.dynamic_orders[next_order++]);
                        continue;
                    }
                    if (run.finished()) { run.wait_until(scenario.dynamic_orders[next_order].release_time()); }
                    else {
                        double delta = simulation.step_minutes;
                        if (next_order < scenario.dynamic_orders.size()) {
                            delta = std::min(delta, scenario.dynamic_orders[next_order].release_time() - run.clock().now());
                        }
                        run.advance(delta);
                    }
                }
                run.write_log(std::cout);
                if (export_directory) {
                    run.export_csv(*export_directory);
                    std::cout << "Order simulation CSV exported to: " << export_directory->u8string() << '\n';
                }
                return 0;
            }
            const auto plan = logistics::Planner::plan(graph, request);
            if (plan.status != logistics::PlanStatus::Feasible) {
                std::cerr << "Planning failed: " << plan.diagnostic << '\n';
                return 2;
            }
            print_plan(plan);
            if (simulate) {
                logistics::Simulation run(graph, plan, simulation);
                if (traffic_enabled) { run.enable_traffic(traffic); }
                std::cout << "Simulation: fixed penalty=" << simulation.late_penalty_points
                          << " points/late order; window checked at unloading start.\n";
                while (!run.finished()) { run.advance(); }
                run.write_log(std::cout);
                if (export_directory) {
                    run.export_csv(*export_directory);
                    std::cout << "Simulation CSV exported to: " << export_directory->u8string() << '\n';
                }
            } else if (export_directory) {
                logistics::Visualizer::export_csv(graph, plan, *export_directory);
                std::cout << "CSV exported to: " << export_directory->u8string() << '\n';
            }
        } else if (matrix) { graph.print_adjacency_matrix(std::cout, *matrix); }
        else if (!summary_only) { graph.print_adjacency_list(std::cout); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
