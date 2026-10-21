#pragma once
#include "logistics/planning/Dijkstra.h"
#include <cstdint>
#include <map>
#include <tuple>

namespace logistics {
struct PathCacheStats {
    std::size_t hits{0};
    std::size_t misses{0};  // Actual Dijkstra requests, including unreachable results.
    std::size_t invalidations{0};
    std::size_t evictions{0};
};
class ShortestPathCache {
public:
    // Zero disables caching, useful for correctness/performance comparisons.
    explicit ShortestPathCache(std::size_t capacity = 256) : capacity_(capacity) {}
    PathResult query(const Graph& graph, const NodeId& source, const NodeId& target,
                     WeightMetric metric = WeightMetric::Distance);
    const PathCacheStats& stats() const noexcept { return stats_; }
    std::size_t size() const noexcept { return entries_.size(); }
    std::size_t capacity() const noexcept { return capacity_; }
    void clear();  // Discard entries/version and reset statistics.
private:
    using Key = std::tuple<NodeId, NodeId, WeightMetric>;
    struct Entry { PathResult path; std::uint64_t used; };
    std::size_t capacity_;
    std::shared_ptr<const GraphRevision> revision_;
    std::map<Key, Entry> entries_;
    std::uint64_t sequence_{0};
    PathCacheStats stats_;
};
}  // namespace logistics
