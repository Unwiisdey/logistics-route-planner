#pragma once
#include "logistics/planning/Planner.h"
#include "logistics/planning/ShortestPathCache.h"
#include <set>

namespace logistics {
enum class OrderActionKind { Pickup, Delivery };
struct OrderAction {
    OrderId order_id;
    OrderActionKind kind{OrderActionKind::Delivery};
};
struct ScheduledOrderAction {
    OrderAction action;
    std::size_t node_index{0};
    Minutes arrival{0};
    Minutes service_start{0};
    Minutes departure{0};
    double load_after{0};
};
struct OrderRoute {
    std::vector<NodeId> nodes;
    std::vector<RoadId> roads;
    std::vector<ScheduledOrderAction> stops;
    Minutes start_time{0};
    Minutes finish_time{0};
    double distance{0};
    double cost{0};
    Minutes travel{0};
};
struct OrderPlanningContext {
    NodeId start_node;
    NodeId warehouse;
    Minutes start_time{0};
    double capacity{0};
    double current_load{0};
    Minutes unload_minutes{0};
    RouteStrategy strategy{RouteStrategy::ShortestDistance};
    std::vector<Order> orders;                 // Every remaining accepted order.
    std::set<OrderId> loaded;                  // Cargo physically on vehicle at the anchor.
    std::vector<OrderAction> incumbent;        // Relative order of existing actions is retained.
};
struct InsertionResult {
    bool accepted{false};
    std::string reason;
    OrderRoute route;
};
class DynamicOrder {
public:
    static Order urgent(OrderId id, NodeId warehouse, NodeId destination, double quantity,
                        Minutes received_at, Minutes deadline_after = 60);
    static InsertionResult evaluate(const Graph& graph, const OrderPlanningContext& context,
                                    const std::vector<OrderAction>& sequence, ShortestPathCache* cache = nullptr);
    static InsertionResult insert(const Graph& graph, const OrderPlanningContext& context, const Order& order,
                                  ShortestPathCache* cache = nullptr);
    static InsertionResult initial(const Graph& graph, OrderPlanningContext context, ShortestPathCache* cache = nullptr);
};
}  // namespace logistics
