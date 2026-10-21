#pragma once
#include "logistics/planning/DynamicOrder.h"

namespace logistics {
struct ReplanResult {
    OrderRoute route;
    std::size_t searched_legs{0};
    std::vector<OrderId> predicted_late;
};
class Replanner {
public:
    // Keep action order. Search by travel time only between stops whose old path
    // contains a trigger road; retain all other roads verbatim, then refresh ETAs.
    static ReplanResult repair(const Graph& graph, const OrderPlanningContext& context,
                              const OrderRoute& remaining, const std::set<RoadId>& triggers);
};
}  // namespace logistics
