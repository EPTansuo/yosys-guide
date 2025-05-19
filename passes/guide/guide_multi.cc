#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include <fstream>
#include <ostream>
#include <string>
#include <unordered_set>
// #include <tclDecls.h>
#include <cassert>
#include <chrono>   

USING_YOSYS_NAMESPACE


PRIVATE_NAMESPACE_BEGIN

static std::unordered_set<std::string> multi_modules;

std::string guide_multi_mod_name(int a_width, int b_width, bool sign = false) {
    return "guide_multigen_" + std::to_string(a_width) + "x" + std::to_string(b_width) + 
           (sign ? "_signed" : "");
}

// ret: filename
std::string guide_genmulti(int a_width, int b_width, int y_width, bool sign = false){

    if(a_width <= 0 || b_width <= 0 || y_width <= 0 || 
        y_width != a_width + b_width ) {
         log_error("Invalid width: a_width: %d, b_width: %d, y_width: %d\n", a_width, b_width, y_width);
    } 
    
    std::string filename = make_temp_file();
    std::ofstream file(filename);
    file << "(* guide = \"multiplier\" *)\n";
    if(a_width != 1 && b_width != 1) {
        file << "module "<< guide_multi_mod_name(a_width, b_width, sign) << " (input [" << a_width-1 << ":0] A, input [" << b_width-1 
                                                                        << ":0] B, output [" << a_width+b_width-1 << ":0] Y);\n";
    }
    else{
        file << "module "<< guide_multi_mod_name(a_width, b_width, sign) << " (input A, input B, output Y);\n";
        
    }
    if(!sign){
        file << "    assign Y = A * B;\n";
    }else{
        file << "    assign Y = $signed(A) * $signed(B);\n";
    }
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
        using clock = std::chrono::steady_clock;
        auto t0 = clock::now();



        std::cout << "GuideMultiPass::execute" << std::endl;  
        // dict<RTLIL::Cell, RTLIL::Cell*> to_replace_cells;
        std::vector<RTLIL::Cell*>  to_replace_cells;
        std::vector<bool>  to_replace_cells_signed;
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
                    
                    auto a_signed = cell->getParam(ID::A_SIGNED).as_bool();
                    auto b_signed = cell->getParam(ID::B_SIGNED).as_bool();

                    if(a_signed ^ b_signed) {
                        log_error("A_SIGNED and B_SIGNED are not the same!\n");
                    }

                    std::cout << "    A: "  << " width: " << a_width << ", A_SIGNED: " << (a_signed ? "true" : "false" ) << std::endl;
                    std::cout << "    B: "  << " width: " << b_width << ", B_SIGNED: " << (b_signed ? "true" : "false" ) << std::endl;
                    std::cout << "    Y: "  << " width: " << y_width << std::endl;

                    

                    // for(auto connections: cell->connections()){
                    //     auto port = connections.first;
                    //     auto sig = connections.second;
                    //     std::cout << "    " << port.str() << " : " << 
                    //     (cell->input(port) ? "input" : cell->output(port) ? "output" : "inout")
                    //          << "  ---->  " << log_signal(sig) << std::endl;
                    // }
                    to_replace_cells.push_back(cell);
                    to_replace_cells_signed.push_back(a_signed & b_signed);
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

                if(y_width != a_width + b_width){
                    log_warning("A width + B width != Y width\nSet Y width to A width + B width!\n");
                    y_width = a_width + b_width;
                }    


                auto sign = to_replace_cells_signed[i];
                // Not already generated the module
                if(multi_modules.count(guide_multi_mod_name(a_width, b_width)) == 0){
                    multi_modules.insert(guide_multi_mod_name(a_width, b_width));
                    auto file_name = guide_genmulti(a_width, b_width, y_width, sign);
                    Yosys::run_pass("read_verilog " + file_name);
                    //remove(file_name.c_str());
                }
                
                std::cout << "before multigen_cell " << std::endl;
                // std::string mod_name = "\\guide_multigen_" + std::to_string(a_width) + "x" + std::to_string(b_width);
                std::string mod_name = "\\" + guide_multi_mod_name(a_width, b_width, sign);
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

                                    
                                    if(y_width_origin > y_width){
                                        RTLIL::SigSpec to_zero = RTLIL::SigSpec(wire, y_width, y_width_origin - y_width);
                                        module->connect(to_zero, RTLIL::SigSpec(RTLIL::State::S0, y_width_origin - y_width));
                                    }else{

                                    }
                                    
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

                auto origin_attr = cell->get_string_attribute(ID::guide);

                cell->set_string_attribute(ID::guide, (origin_attr.empty() ? "" : origin_attr + "; ")  + "multiplier");
                module->remove(to_replace_cell);

                i++;
                // break;
            }
            
        }

        auto t1  = clock::now();
        auto ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        double ms = ns / 1'000'000.0;

        std::cout << "Duration of guide_multi: " << ms << " ms (" << ns << " ns)\n";
    }

} GuideMultiPass;


PRIVATE_NAMESPACE_END
