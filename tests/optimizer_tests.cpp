#include "test_support.hpp"
#include "logistics/planning/DynamicOrder.h"
using namespace logistics;
void relocation() {
    Graph g;
    const std::vector<std::string> ids{"W", "A", "B", "C"};
    for (const auto& id : ids) { g.add_node(Node(id, id == "W" ? NodeType::Warehouse : NodeType::DeliveryPoint, 0, 0)); }
    for (std::size_t i = 0; i < ids.size(); ++i) {
        for (std::size_t j = 0; j < ids.size(); ++j) {
            if (i != j) {
                const double weight = j == (i + 1) % ids.size() ? 1 : 10;
                g.add_edge(Edge(ids[i] + ids[j], ids[i], ids[j], weight, weight, weight));
            }
        }
    }
    OrderPlanningContext ctx;
    ctx.start_node = ctx.warehouse = "W"; ctx.capacity = ctx.current_load = 3;
    for (const auto& id : {"A", "B", "C"}) {
        ctx.orders.emplace_back(id, "W", id, 1, TimeWindow{0, 100}); ctx.loaded.insert(id);
    }
    const std::vector<OrderAction> reverse{{"C", OrderActionKind::Delivery}, {"B", OrderActionKind::Delivery}, {"A", OrderActionKind::Delivery}};
    auto seed = DynamicOrder::evaluate(g, ctx, reverse);
    auto improved = DynamicOrder::improve(g, ctx, seed);
    check(improved.accepted && improved.route.distance < seed.route.distance, "relocation improves directed tour");
    check_near(improved.route.distance, 4);
    check(improved.route.stops.size() == 3, "no tasks lost");
    check_near(improved.route.stops.back().load_after, 0);
    ctx.orders[0] = Order("A", "W", "A", 1, TimeWindow{0, 1});
    const auto constrained = DynamicOrder::improve(g, ctx, DynamicOrder::evaluate(g, ctx,
        {{"A", OrderActionKind::Delivery}, {"C", OrderActionKind::Delivery}, {"B", OrderActionKind::Delivery}}));
    check(constrained.accepted && constrained.route.stops.front().action.order_id == "A", "deadline retained during moves");
    const auto urgent = DynamicOrder::improve(g, ctx, constrained, nullptr, "A");
    check_near(urgent.route.stops.front().service_start, 1);
    check(!DynamicOrder::improve(g, ctx, {}).accepted, "failed seed stays failed");
}
int main(int argc, char* argv[]) { return run_tests(argc, argv, {{"relocation", relocation}}); }
