#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include <fstream>
#include <ostream>
#include <string>
#include <unordered_set>
// #include <tclDecls.h>

USING_YOSYS_NAMESPACE


PRIVATE_NAMESPACE_BEGIN

void test_remove_cell(RTLIL::Design* design){
    for(auto module: design->selected_whole_modules_warn()){
        for(auto cell: module->selected_cells()){
            if(cell->type == ID($mul)){
                std::cout << "Found $mul: " << cell->name.str() << std::endl;
                auto a = cell->getPort(ID::A);
                auto b = cell->getPort(ID::B);
                auto y = cell->getPort(ID::Y);
                auto a_width = a.size();
                auto b_width = b.size();
                auto y_width = y.size();
                std::cout << "    a: "  << " width: " << a_width << std::endl;
                std::cout << "    b: "  << " width: " << b_width << std::endl;
                std::cout << "    y: "  << " width: " << y_width << std::endl;
        
                for(auto connections: cell->connections()){
                    auto port = connections.first;
                    auto sig = connections.second;
                    std::cout << "    " << port.str() << " : " << 
                    (cell->input(port) ? "input" : cell->output(port) ? "output" : "inout")
                        << std::endl;
                }
                // to_replace_cells.push_back(cell);

                RTLIL::SigSpec a_conn = y.extract(0, a_width);
                RTLIL::SigSpec b_conn = y.extract(a_width, b_width);
                module->connect(a, a_conn);
                module->connect(b, b_conn);
                module->remove(cell);

            }
        }
    }
}

void test_sigspec_connect(RTLIL::Design *design){
    for(auto module: design->selected_whole_modules_warn()){
        if(module->name == "\\WT_USP_RP_10x10_noX_spec"){
            std::cout << "Found module: " << module->name.str() << std::endl;
        }else{
            continue;
        }
        std::cout<< "Num of connections: " << module->connections().size() << std::endl;
        for(auto conn: module->connections()){
            auto sig = conn.first;
            auto port = conn.second;
            std::cout << "  " << log_signal(sig) << " : " << log_signal(port) << std::endl;
        }

        std::cout << "Num of wires: " << module->selected_wires().size() << std::endl;
        for(auto wire: module->selected_wires()){
            std::cout << "  " << wire->name.str() << " : " << 
            (wire->port_input ? "input" : wire->port_output ? "output" : "inout")
                << std::endl;
        }

        for(auto cell: module->selected_cells()){
            std::cout << "Connection of cell: " << cell->name.str() << std::endl;
            for(auto conn: cell->connections()){
                auto port = conn.first;
                auto sig = conn.second;
                std::cout << "  " << port.str() << " : " << log_signal(sig) << std::endl;
            }
        }
    }
}

void test_connect_zero(RTLIL::Design* design){
    auto module = design->top_module();
    for(auto wire: module->selected_wires()){
        
        if(wire->port_output || wire->port_input){
            continue;
        }

        module->connect(wire, RTLIL::SigSpec(State::S0, wire->width));
        
        break;
    }
    
}


void test_wires(RTLIL::Design* design){
    auto module = design->top_module();
    for(auto wire: module->selected_wires()){
        std::cout << "Wire: " << wire->name.str();
        // std::cout << " " << wire->driverPort().str();
        // if(wire->driverCell() != nullptr){
        //     std::cout << " " << wire->driverCell()->name.str(); 
        // }
        std::cout << std::endl;

    }
    for(auto conn: module->connections()){
        std::cout << "Mod  Connection: " << log_signal(conn.first) << " -> " << log_signal(conn.second) << std::endl;
    }

    for(auto cell: module->selected_cells()){
        for(auto conn: cell->connections()){
            std::cout << "Cell Connection: " << cell->name.str() + "." + (conn.first.str()) << " -> " << log_signal(conn.second) << std::endl;
        }
    }
}

struct GuideTestPass : public Pass {
	GuideTestPass() : Pass("guide_test", "Just For test") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    guide_test \n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override{
        (void)args;
        //test_remove_cell(design);
        //test_sigspec_connect(design);
         test_connect_zero(design);
        //test_wires(design);
    }

} GuideTestPass;


PRIVATE_NAMESPACE_END
