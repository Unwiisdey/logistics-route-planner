#include "test_support.hpp"
#include "logistics/config/ConfigLoader.h"
#include "logistics/config/OrderConfigLoader.h"
#include "logistics/simulation/FleetSimulation.h"
#include <filesystem>
#include <fstream>
#include <sstream>
using namespace logistics;
namespace {
Graph network() {
    Graph g;
    g.add_node(Node("W", NodeType::Warehouse, 0, 0));
    g.add_node(Node("T", NodeType::TransferStation, 1, 0));
    g.add_node(Node("A", NodeType::DeliveryPoint, 2, 0));
    g.add_node(Node("B", NodeType::DeliveryPoint, 0, 1));
    for (const auto& e : std::vector<Edge>{Edge("WT","W","T",1,5,1), Edge("TW","T","W",1,5,1),
        Edge("TA","T","A",1,3,1), Edge("AT","A","T",1,3,1), Edge("WB","W","B",1,2,1), Edge("BW","B","W",1,2,1),
        Edge("TB","T","B",2,4,2), Edge("BT","B","T",2,4,2), Edge("WA","W","A",3,9,3), Edge("AW","A","W",3,9,3)}) { g.add_edge(e); }
    return g;
}
SimulationOptions options() { SimulationOptions o; o.capacity_kg=10; o.unload_minutes=1; return o; }
Order hub(const std::string& id="hub", double quantity=3, TimeWindow window={480,600}) {
    return Order(id,"T","A",quantity,window,OrderPriority::Urgent,480);
}
template<class T> void finish(T& sim, double step=1) {
    int guard=0; while (!sim.finished()) { sim.advance(step); check(++guard<1000,"must terminate"); }
}
void schema() {
    std::istringstream in(R"({"schema_version":1,"initial_orders":[{"id":"old","warehouse_id":"W","delivery_node_id":"A","quantity":1,"time_window":null}],"dynamic_orders":[{"id":"hub","pickup_node_id":"T","delivery_node_id":"A","quantity":3,"time_window":{"earliest":480,"latest":600},"release_time":480}]})");
    const auto loaded=OrderConfigLoader::read(in);
    check(loaded.initial_orders[0].pickup_node_id()=="W", "legacy source compatible");
    check(loaded.dynamic_orders[0].pickup_node_id()=="T" && loaded.dynamic_orders[0].warehouse_id()=="T", "canonical pickup and legacy alias");
    for (const auto& fields : {std::string{},std::string{"\"pickup_node_id\":\"T\",\"warehouse_id\":\"W\","}}) {
        std::istringstream bad("{\"schema_version\":1,\"initial_orders\":[],\"dynamic_orders\":[{"+fields+"\"id\":\"bad\",\"delivery_node_id\":\"A\",\"quantity\":1,\"time_window\":null,\"release_time\":480}]}");
        expect_throw<ConfigError>([&] { OrderConfigLoader::read(bad); });
    }
}
void physical() {
    Simulation sim(network(),"W",{},480,options());
    check(sim.insert_order(hub()).accepted,"hub pickup accepted");
    check(sim.waiting_pickups("T")==std::vector<OrderId>{"hub"},"cargo ready, awaiting physical collection");
    check_near(sim.vehicle().current_load(),0);
    sim.advance(4); check(!sim.deliveries()[0].picked_up_at,"no remote loading");
    sim.advance(1); check_near(*sim.deliveries()[0].picked_up_at,485);
    check_near(sim.vehicle().current_load(),3); check(sim.waiting_pickups("T").empty(),"taken cargo leaves waiting list");
    finish(sim);
    check(sim.vehicle().position().node_id=="W","return home warehouse, not pickup station");
    check_near(*sim.deliveries()[0].service_start,488); check_near(sim.vehicle().current_load(),0);
    int pickups=0;
    for (const auto& e : sim.events()) { if(e.kind=="pickup") { ++pickups;check(e.state.node_id=="T","physical pickup location"); } }
    check(pickups==1 && !sim.insert_order(hub()).accepted,"no duplicate pickup, including completed order");
    sim.export_csv("test_artifacts/hub_pickup");
    std::ifstream file("test_artifacts/hub_pickup/deliveries.csv");std::stringstream text;text<<file.rdbuf();
    check(text.str().find("picked_up_at,pickup_node_id")!=std::string::npos,"export names pickup source explicitly");
}
void rejection() {
    Simulation sim(network(),"W",{Order("old","W","B",4,TimeWindow{480,600})},480,options());
    const auto revision=sim.route_revisions().size();const auto load=sim.vehicle().current_load();
    for (const auto& order : {hub("huge",11),hub("late",1,TimeWindow{480,481}),Order("bad","A","B",1,TimeWindow{480,600})}) {
        check(!sim.insert_order(order).accepted,"invalid or infeasible hub pickup rejected");
        check(sim.waiting_pickups("T").empty() && sim.route_revisions().size()==revision,"no ghost cargo or route mutation");
        check_near(sim.vehicle().current_load(),load);
    }
    expect_throw<std::invalid_argument>([&] { sim.waiting_pickups("A"); });
    expect_throw<std::invalid_argument>([] { Simulation invalid(network(),"W",{hub()},480,options()); });
    // A fast new pickup cannot sacrifice an existing tight deadline.
    Simulation tight(network(),"W",{Order("old","W","B",4,TimeWindow{480,482})},480,options());
    check(!tight.insert_order(hub("impossible",1,TimeWindow{480,487})).accepted,"protect original deadline");
}
void passing() {
    Simulation sim(network(),"W",{Order("old","W","A",1,TimeWindow{480,600})},480,options());
    sim.advance(5); // The preferred path goes through T, without a pickup action.
    check(sim.waiting_pickups("T").empty(),"station has no implicit stock tasks");
    check_near(sim.vehicle().current_load(),1);
    for(const auto& e:sim.events())check(e.kind!="pickup","passing a station cannot auto-load");
    finish(sim);check(sim.vehicle().position().node_id=="W","original home retained");
}
void fleet() {
    FleetSimulation fleet(network(),{{"V01","W",5},{"V02","W",5}},{},480,options());
    const auto a=fleet.insert_order(hub());check(a.accepted && a.vehicle_id=="V01","one selected vehicle");
    check(fleet.waiting_pickups("T")==std::vector<OrderId>{"hub"},"one pending cargo record across fleet");
    check(!fleet.insert_order(hub()).accepted && fleet.waiting_pickups("T").size()==1,"no second reservation");
    fleet.advance(2);fleet.apply_traffic({{"TA",6,TrafficStatus::Congested}});finish(fleet);
    check(fleet.waiting_pickups("T").empty() && fleet.ownership().at("hub")=="V01","ownership survives congestion and collection");
    int pickups=0;
    for(const auto& spec:fleet.vehicle_specs()) {
        const auto& sim=fleet.vehicle(spec.id);
        for(const auto& e:sim.events())if(e.kind=="pickup")++pickups;
        check(sim.vehicle().position().node_id=="W","each vehicle returns to own warehouse");
    }
    check(pickups==1 && fleet.vehicle("V02").deliveries().empty(),"losing vehicle never collects cargo");
}
void steps() {
    std::optional<Minutes> service;
    for (const auto step : {.25,1.,5.,100.}) {
        Simulation sim(network(),"W",{},480,options());sim.enable_traffic(TrafficOptions{1000,.1});
        check(sim.insert_order(hub()).accepted,"accept hub order");sim.advance(2);sim.apply_traffic({{"TA",6,TrafficStatus::Congested}});
        finish(sim,step);
        if(service)check_near(*service,*sim.deliveries()[0].service_start);else service=sim.deliveries()[0].service_start;
        check_near(*sim.deliveries()[0].picked_up_at,485);check(sim.waiting_pickups("T").empty(),"all steps clear waiting cargo");
    }
}
void demo_network() {
    const auto g=ConfigLoader::load_network("config/network.json",true);
    check(g.node_count()==30 && g.edge_count()==82,"approved road topology");
    check(g.find_edge("D12","D18") && g.find_edge("D18","D12"),"requested diagonal exists in both directions");
    check(g.node_count(NodeType::Warehouse)==2 && g.node_count(NodeType::TransferStation)==3,"preserve node roles");
    const auto scenario=OrderConfigLoader::load("config/hub-orders.json");
    check(scenario.dynamic_orders[0].pickup_node_id()=="T02","runnable station example");
}
}
int main(int argc,char* argv[]) {
    return run_tests(argc,argv,{{"schema",schema},{"physical",physical},{"rejection",rejection},{"passing",passing},
        {"fleet",fleet},{"steps",steps},{"network",demo_network}});
}
