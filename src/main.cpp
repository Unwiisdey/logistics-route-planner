#include "logistics/config/ConfigLoader.h"
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) {
    try {
        std::filesystem::path path = "config/network.json";
        bool explicit_path = false;
        bool check_size = false;
        bool summary_only = false;
        std::optional<logistics::WeightMetric> matrix;
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--help") {
                std::cout << "Usage: logistics_app [--config FILE] [--matrix distance|time|cost] "
                             "[--summary] [--check-size]\n"
                             "Default: load config/network.json and print its adjacency list.\n";
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
            } else {
                throw std::invalid_argument("unknown or incomplete option: " + argument);
            }
        }
        if (summary_only && matrix) { throw std::invalid_argument("--summary and --matrix cannot be combined"); }
        if (!explicit_path && !std::filesystem::exists(path)) {
            path = std::filesystem::path(argv[0]).parent_path() / "config" / "network.json";
        }
        const auto graph = logistics::ConfigLoader::load_network(path, check_size);
        std::cout << "Logistics Route Planner v0.2.0\n"
                  << "Nodes: " << graph.node_count() << " | Edges: " << graph.edge_count() << '\n'
                  << "Warehouses: " << graph.node_count(logistics::NodeType::Warehouse)
                  << " | Delivery points: " << graph.node_count(logistics::NodeType::DeliveryPoint)
                  << " | Transfer stations: " << graph.node_count(logistics::NodeType::TransferStation) << '\n';
        if (check_size) { std::cout << "Course size requirements: PASS\n"; }
        if (matrix) { graph.print_adjacency_matrix(std::cout, *matrix); }
        else if (!summary_only) { graph.print_adjacency_list(std::cout); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
