#include "kernel/yosys.h"
#include <fstream>

USING_YOSYS_NAMESPACE


PRIVATE_NAMESPACE_BEGIN

using Sig = std::string;
using Sigs = std::set<Sig>;
using Connects = std::map<std::string, Sigs>;




void get_node_all_connections_dfs(const Connects& graph, const Sig& node, Sigs& visited) {
    visited.insert(node);

    auto it = graph.find(node);
    if (it != graph.end()) {
        for (const auto& neighbor : it->second) {
            if (visited.find(neighbor) == visited.end()) {
                get_node_all_connections_dfs(graph, neighbor, visited);
            }
        }
    }
}


Sigs get_node_all_connections(const Connects& graph, const std::string& node){
    Connects graph_;

    // Convert the directed graph to an undirected graph
    for(const auto &it: graph){
        for(const auto &it2: it.second){
            graph_[it.first].insert(it2);
            graph_[it2].insert(it.first);
        }
    }
    Sigs visited;
    get_node_all_connections_dfs(graph_, node, visited);
    return visited;
}

void print_all_connects(const Connects& conn){
    for(auto &assign_node: conn){
		std::cout << assign_node.first << " === {";
		int first = 1;
		for(auto &t: assign_node.second){
			if(first) first = 0;
			else std::cout << ", ";
			std::cout << (t);
		}	
		std::cout << "}\n";
	}
    std::cout << "Finish" << std::endl;
}

Connects get_module_wire_connects(RTLIL::Module *module){
    Connects connect_graph = {};
    Connects ret = {};
    Sigs tmp;


    // get Directed Acyclic Graph of connections
    for(auto wire: module->wires_){
        for(auto connection: module->connections_){
            if(connection.second == SigSpec(wire.second)){
                if(connect_graph.count(wire.first.str())){
                    tmp = connect_graph[wire.first.str()];
                }else{
                    tmp = {};
                }
                if(tmp.count(log_signal(connection.first))){
                    continue;
                }
                tmp.insert(log_signal(connection.first));
                connect_graph[wire.first.str()] = tmp;
            }
        }
    }

    // get all nodes of the graph
    Sigs nodes; 
    for(auto &assign_node: connect_graph){
        nodes.insert(assign_node.first);
		for(auto &t: assign_node.second){
            nodes.insert(t);
		}	
	}


    // get all connections of each node
    for(const auto &it: nodes){
        ret[it] = get_node_all_connections(connect_graph, it);
    }

    
    print_all_connects(ret);

    return ret;
}


void write_guide_to_attr(RTLIL::Module *module, const Connects& conn){
    for(auto& wire: module->wires_){
        if(conn.count(wire.first.str())){
            std::string guide = "connect: {";
            int first = 1;
            for(auto& t: conn.at(wire.first.str())){
                if(first) first = 0;
                else guide += ",";
                guide += t;
            }
            guide += "};";
            //wire.second->attributes[ID::guide] = guide;
            wire.second->set_string_attribute(ID::guide, guide);
        }
    }
    
}

   



struct GuideConnectPass : public Pass {
	GuideConnectPass() : Pass("guide_connect", "write the formal verification guide") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    guide_connect \n");
        log("Add connection information to the wires' attributes.");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override{
        (void)args;
        std::cout << "GuideConnectPass::execute" << std::endl;  
        Connects conn;
        std::cout << "Module Num: " << std::to_string(design->modules_.size()) << std::endl;
        for(auto module: design->selected_whole_modules_warn()){
            auto connects = get_module_wire_connects(module);
            write_guide_to_attr(module, connects);
        }
    }

} GuideConnectPass;


PRIVATE_NAMESPACE_END
