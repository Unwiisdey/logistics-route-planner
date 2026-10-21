#include "logistics/planning/ShortestPathCache.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace logistics {
PathResult ShortestPathCache::query(const Graph& graph, const NodeId& source, const NodeId& target, WeightMetric metric) {
    if (!graph.find_node(source) || !graph.find_node(target) ||
        (metric != WeightMetric::Distance && metric != WeightMetric::Time && metric != WeightMetric::Cost)) {
        throw std::invalid_argument("invalid shortest-path cache query");
    }
    if (revision_ != graph.revision()) {
        if (revision_) { ++stats_.invalidations; }
        entries_.clear(); revision_ = graph.revision(); sequence_ = 0;
    }
    if (sequence_ == std::numeric_limits<std::uint64_t>::max()) { entries_.clear(); sequence_ = 0; }
    const Key key{source, target, metric};
    const auto found = entries_.find(key);
    if (found != entries_.end()) {
        ++stats_.hits; found->second.used = ++sequence_;
        return found->second.path;  // Value result; callers cannot mutate the cached snapshot.
    }
    ++stats_.misses;
    auto result = Dijkstra::shortest_path(graph, source, target, metric);
    if (capacity_ != 0) {
        if (entries_.size() == capacity_) {
            const auto oldest = std::min_element(entries_.begin(), entries_.end(), [](const auto& a, const auto& b) {
                return a.second.used < b.second.used;
            });
            entries_.erase(oldest); ++stats_.evictions;
        }
        entries_.emplace(key, Entry{result, ++sequence_});
    }
    return result;
}
void ShortestPathCache::clear() {
    entries_.clear(); revision_.reset(); sequence_ = 0; stats_ = {};
}
}  // namespace logistics
