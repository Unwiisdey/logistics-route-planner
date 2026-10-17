#pragma once
#include "logistics/model/Node.h"
#include "logistics/model/Edge.h"
#include <cstddef>
#include <iosfwd>
#include <optional>
#include <unordered_map>
#include <vector>

namespace logistics {
struct AdjacencyMatrix {
    std::vector<NodeId> node_ids;  // Sorted row/column labels.
    std::vector<std::vector<std::optional<double>>> weights;
};

class Graph {
public:
    void add_node(Node node);
    bool remove_node(const NodeId& id);
    void add_edge(Edge edge);
    bool remove_edge(const NodeId& from_id, const NodeId& to_id);
    void update_edge_traffic(const NodeId& from_id, const NodeId& to_id,
                             Minutes time, TrafficStatus status);
    const Node* find_node(const NodeId& id) const noexcept;
    const Edge* find_edge(const NodeId& from_id, const NodeId& to_id) const noexcept;
    const Edge* find_edge_by_id(const RoadId& id) const noexcept;
    const std::vector<Edge>& outgoing_edges(const NodeId& id) const;
    const std::unordered_map<NodeId, Node>& nodes() const noexcept { return nodes_; }
    std::size_t node_count() const noexcept { return nodes_.size(); }
    std::size_t node_count(NodeType type) const noexcept;
    std::size_t edge_count() const noexcept;
    AdjacencyMatrix adjacency_matrix(WeightMetric metric = WeightMetric::Distance) const;
    void print_adjacency_list(std::ostream& output) const;
    void print_adjacency_matrix(std::ostream& output,
                                WeightMetric metric = WeightMetric::Distance) const;
private:
    std::vector<NodeId> sorted_node_ids() const;
    std::unordered_map<NodeId, Node> nodes_;
    std::unordered_map<NodeId, std::vector<Edge>> adjacency_;
};
}  // namespace logistics
