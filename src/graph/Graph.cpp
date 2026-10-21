#include "logistics/graph/Graph.h"
#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace logistics {
void Graph::add_node(Node node) {
    const NodeId id = node.id();
    if (nodes_.find(id) != nodes_.end()) {
        throw std::invalid_argument("duplicate node id: " + id);
    }
    revision_ = std::make_shared<GraphRevision>();
    nodes_.emplace(id, std::move(node));
    try {
        adjacency_.emplace(id, std::vector<Edge>{});
    } catch (...) {
        nodes_.erase(id);
        throw;
    }
}

bool Graph::remove_node(const NodeId& id) {
    // The argument can itself be a reference returned by find_node().
    const NodeId removed_id = id;
    if (!find_node(removed_id)) { return false; }
    revision_ = std::make_shared<GraphRevision>();
    nodes_.erase(removed_id);
    adjacency_.erase(removed_id);
    for (auto& entry : adjacency_) {
        auto& edges = entry.second;
        edges.erase(std::remove_if(edges.begin(), edges.end(), [&](const Edge& edge) {
            return edge.to_id() == removed_id;
        }), edges.end());
    }
    return true;
}

void Graph::add_edge(Edge edge) {
    if (!find_node(edge.from_id()) || !find_node(edge.to_id())) {
        throw std::invalid_argument("edge references a missing endpoint: " + edge.id());
    }
    if (find_edge_by_id(edge.id())) {
        throw std::invalid_argument("duplicate edge id: " + edge.id());
    }
    if (find_edge(edge.from_id(), edge.to_id())) {
        throw std::invalid_argument("duplicate directed edge: " + edge.from_id() + " -> " + edge.to_id());
    }
    const NodeId from_id = edge.from_id();
    revision_ = std::make_shared<GraphRevision>();
    adjacency_.at(from_id).push_back(std::move(edge));
}

bool Graph::remove_edge(const NodeId& from_id, const NodeId& to_id) {
    const auto entry = adjacency_.find(from_id);
    if (entry == adjacency_.end()) { return false; }
    auto& edges = entry->second;
    const auto edge = std::find_if(edges.begin(), edges.end(), [&](const Edge& candidate) {
        return candidate.to_id() == to_id;
    });
    if (edge == edges.end()) { return false; }
    revision_ = std::make_shared<GraphRevision>();
    edges.erase(edge);
    return true;
}

void Graph::update_edge_traffic(const NodeId& from_id, const NodeId& to_id,
                                Minutes time, TrafficStatus status) {
    const auto entry = adjacency_.find(from_id);
    if (entry != adjacency_.end()) {
        for (auto& edge : entry->second) {
            if (edge.to_id() == to_id) {
                auto updated = edge.with_traffic(time, status);
                if (edge.time() == time && edge.traffic_status() == status) { return; }
                revision_ = std::make_shared<GraphRevision>();
                edge = std::move(updated);
                return;
            }
        }
    }
    throw std::out_of_range("cannot update a missing edge");
}

const Node* Graph::find_node(const NodeId& id) const noexcept {
    const auto node = nodes_.find(id);
    return node == nodes_.end() ? nullptr : &node->second;
}

const Edge* Graph::find_edge(const NodeId& from_id, const NodeId& to_id) const noexcept {
    const auto entry = adjacency_.find(from_id);
    if (entry != adjacency_.end()) {
        for (const auto& edge : entry->second) {
            if (edge.to_id() == to_id) { return &edge; }
        }
    }
    return nullptr;
}

const Edge* Graph::find_edge_by_id(const RoadId& id) const noexcept {
    for (const auto& entry : adjacency_) {
        for (const auto& edge : entry.second) {
            if (edge.id() == id) { return &edge; }
        }
    }
    return nullptr;
}

const std::vector<Edge>& Graph::outgoing_edges(const NodeId& id) const {
    return adjacency_.at(id);
}

std::size_t Graph::node_count(NodeType type) const noexcept {
    return static_cast<std::size_t>(std::count_if(nodes_.begin(), nodes_.end(),
        [type](const auto& entry) { return entry.second.type() == type; }));
}

std::size_t Graph::edge_count() const noexcept {
    std::size_t count = 0;
    for (const auto& entry : adjacency_) { count += entry.second.size(); }
    return count;
}

std::vector<NodeId> Graph::sorted_node_ids() const {
    std::vector<NodeId> ids;
    ids.reserve(nodes_.size());
    for (const auto& entry : nodes_) { ids.push_back(entry.first); }
    std::sort(ids.begin(), ids.end());
    return ids;
}

AdjacencyMatrix Graph::adjacency_matrix(WeightMetric metric) const {
    if (metric != WeightMetric::Distance && metric != WeightMetric::Time && metric != WeightMetric::Cost) {
        throw std::invalid_argument("invalid weight metric");
    }
    AdjacencyMatrix matrix;
    matrix.node_ids = sorted_node_ids();
    const std::size_t count = matrix.node_ids.size();
    matrix.weights.assign(count, std::vector<std::optional<double>>(count));
    std::unordered_map<NodeId, std::size_t> indices;
    for (std::size_t i = 0; i < count; ++i) {
        indices.emplace(matrix.node_ids[i], i);
        matrix.weights[i][i] = 0.0;
    }
    for (std::size_t i = 0; i < count; ++i) {
        for (const auto& edge : outgoing_edges(matrix.node_ids[i])) {
            matrix.weights[i][indices.at(edge.to_id())] = edge.weight(metric);
        }
    }
    return matrix;
}

void Graph::print_adjacency_list(std::ostream& output) const {
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << std::fixed << std::setprecision(2);
    for (const auto& id : sorted_node_ids()) {
        text << id << " (" << to_string(nodes_.at(id).type()) << "):";
        std::vector<const Edge*> edges;
        for (const auto& edge : outgoing_edges(id)) { edges.push_back(&edge); }
        std::sort(edges.begin(), edges.end(), [](const Edge* left, const Edge* right) {
            return left->to_id() < right->to_id();
        });
        if (edges.empty()) { text << " (none)"; }
        for (const auto* edge : edges) {
            text << ' ' << edge->to_id() << " [id=" << edge->id()
                 << ", distance=" << edge->distance() << ", time=" << edge->time()
                 << ", cost=" << edge->cost() << ", traffic=" << to_string(edge->traffic_status()) << ']';
        }
        text << '\n';
    }
    output << text.str();
}

void Graph::print_adjacency_matrix(std::ostream& output, WeightMetric metric) const {
    const auto matrix = adjacency_matrix(metric);
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << std::fixed << std::setprecision(2) << "id";
    for (const auto& id : matrix.node_ids) { text << '\t' << id; }
    text << '\n';
    for (std::size_t row = 0; row < matrix.node_ids.size(); ++row) {
        text << matrix.node_ids[row];
        for (const auto& weight : matrix.weights[row]) {
            text << '\t';
            if (weight) { text << *weight; } else { text << "INF"; }
        }
        text << '\n';
    }
    output << text.str();
}
}  // namespace logistics
