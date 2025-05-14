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

static std::unordered_set<std::string> multi_modules;

std::string guide_multi_mod_name(int a_width, int b_width){
    return "guide_multigen_" + std::to_string(a_width) + "x" + std::to_string(b_width);
}

// ret: filename
std::string guide_genmulti(int a_width, int b_width, int y_width){

    if(a_width <= 0 || b_width <= 0 || y_width <= 0 || 
        y_width != a_width + b_width ) {
         log_error("Invalid width: a_width: %d, b_width: %d, y_width: %d\n", a_width, b_width, y_width);
    } 
    
    std::string filename = make_temp_file();
    std::ofstream file(filename);
    if(a_width != 1 && b_width != 1) {
        file << "module "<< guide_multi_mod_name(a_width, b_width) << " (input [" << a_width-1 << ":0] A, input [" << b_width-1 
                                                                        << ":0] B, output [" << a_width+b_width-1 << ":0] Y);\n";
    }
    else{
        file << "module "<< guide_multi_mod_name(a_width, b_width) << " (input A, input B, output Y);\n";
        
    }
    file << "    assign Y = A * B;\n";
    file << "endmodule\n";
    file.close();

    return filename;
}


struct GuideMultiPass : public Pass {
	GuideMultiPass() : Pass("guide_multi", "Convert the $mul to a instance of multiplier and preserve the infomation") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    guide_multi \n");
        log("Convert the $mul to a instance of multiplixer and preserve the infomation if no arguments are given.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override{
        (void)args;
        std::cout << "GuideMultiPass::execute" << std::endl;  
        // dict<RTLIL::Cell, RTLIL::Cell*> to_replace_cells;
        std::vector<RTLIL::Cell*>  to_replace_cells;
        for(auto module: design->selected_whole_modules_warn()){
            std::cout << "Module: " << module->name.str() << std::endl;
            to_replace_cells.clear();
            for(auto cell: module->selected_cells()){
                if(cell->type == ID($mul)){
                    std::cout << "Found $mul: " << cell->name.str() << std::endl;
                    auto a = cell->getPort(ID::A);
                    auto b = cell->getPort(ID::B);
                    auto y = cell->getPort(ID::Y);
                    auto a_width = a.size();
                    auto b_width = b.size();
                    auto y_width = y.size();
                    std::cout << "    A: "  << " width: " << a_width << std::endl;
                    std::cout << "    B: "  << " width: " << b_width << std::endl;
                    std::cout << "    Y: "  << " width: " << y_width << std::endl;


                    for(auto connections: cell->connections()){
                        auto port = connections.first;
                        auto sig = connections.second;
                        std::cout << "    " << port.str() << " : " << 
                        (cell->input(port) ? "input" : cell->output(port) ? "output" : "inout")
                             << "  ---->  " << log_signal(sig) << std::endl;
                    }
                    to_replace_cells.push_back(cell);
                }
            }
            int i = 0;
            
            std::cout <<"after read_verilog" << std::endl;
            for(auto to_replace_cell : to_replace_cells){

                auto a = to_replace_cell->getPort(ID::A);
                auto b = to_replace_cell->getPort(ID::B);
                auto y = to_replace_cell->getPort(ID::Y);

                // the width of the wire, not the real width of the multiplier
                auto a_width = a.size();
                auto b_width = b.size();
                auto y_width = y.size();

                int y_width_origin = y_width; 

                // get the real witdh of the multiplier
                for(auto conn: to_replace_cell->connections()){
                    if(conn.first == ID::A){
                        for(auto chunk: conn.second.chunks()){
                            if(chunk.is_wire()){
                                a_width = conn.second.extract(chunk).size();
                                break;
                            }
                        }
                    }else if(conn.first == ID::B){
                        for(auto chunk: conn.second.chunks()){
                            if(chunk.is_wire()){
                                b_width = conn.second.extract(chunk).size();
                                break;
                            }
                        }
                    }else if(conn.first == ID::Y){
                        for(auto chunk: conn.second.chunks()){
                            if(chunk.is_wire()){
                                y_width = conn.second.extract(chunk).size();
                                y_width_origin = y_width;
                                break;
                            }
                        }
                    } 
 
                }

                std::cout << "y_width: " << y_width << std::endl;
                std::cout << "a_width: " << a_width << std::endl;
                std::cout << "b_width: " << b_width << std::endl;
                if(y_width == a_width && y_width == b_width){
                    // TODO: to verify the multiplier in picorv32
                }
                else if(y_width != a_width + b_width){
                    log_warning("A width + B width != Y width\nSet Y width to A width + B width!\n");
                    y_width = a_width + b_width;
                }    


                // Not already generated the module
                if(multi_modules.count(guide_multi_mod_name(a_width, b_width)) == 0){
                    multi_modules.insert(guide_multi_mod_name(a_width, b_width));
                    auto file_name = guide_genmulti(a_width, b_width, y_width);
                    Yosys::run_pass("read_verilog " + file_name);
                    //remove(file_name.c_str());
                }
                
                std::cout << "before multigen_cell " << std::endl;
                std::string mod_name = "\\guide_multigen_" + std::to_string(a_width) + "x" + std::to_string(b_width);
                IdString name = mod_name + "_" + std::to_string(i) ; 
                IdString type = mod_name;


                for(auto conn: module->connections_){
                        std::cout << "  " << log_signal(conn.first) << " :------------: " << log_signal(conn.second) << std::endl;
                }

                for(auto wire : module->selected_wires()){
                    std::cout << "  " << wire->name.str() << " : " << 
                    (wire->port_input ? "input" : wire->port_output ? "output" : "inout")
                        << std::endl;

                    // wiure
                }

                
                std::cout<<"Add Cell" << std::endl;
                RTLIL::Cell *cell = module->addCell(name, type);
                if(y_width != y_width_origin){
                    for(auto conn: to_replace_cell->connections()){
                        if(conn.first == ID::Y){
                            
                            // std::cout << conn.first.str() << " :----------: " << log_signal(conn.second) << std::endl;
                            RTLIL::SigSpec y_conn = conn.second;

                            cell->connections_.insert(std::make_pair(conn.first, y_conn));

                            for(auto wire: module->selected_wires()){
                                if(wire->name == log_signal(conn.second)){
                                    // std::cout << "Find the wire: " << wire->name.str() << std::endl;

                                    RTLIL::SigSpec to_zero = RTLIL::SigSpec(wire, y_width, y_width_origin - y_width);
                                    module->connect(to_zero, RTLIL::SigSpec(RTLIL::State::S0, y_width_origin - y_width));
                                    break;
                                }
                            }
                            
                        }else{
                            cell->connections_.insert(conn);
                        }
                    }
                }else{
                    cell->connections_ = to_replace_cell->connections_;
                }
                // cell->parameters = to_replace_cell->parameters;
                cell->attributes = to_replace_cell->attributes;

                
                cell->set_string_attribute(ID::guide, "multiplier");
                module->remove(to_replace_cell);

                i++;
                // break;
            }
            
        }
    }

} GuideMultiPass;


PRIVATE_NAMESPACE_END
