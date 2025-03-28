#include "kernel/log.h"
#include "kernel/register.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include <algorithm>
#include <fstream>
#include <sys/wait.h> 
#include <string>
#include <vector>

USING_YOSYS_NAMESPACE


PRIVATE_NAMESPACE_BEGIN



std::vector<RTLIL::Cell *> get_multi_cells(RTLIL::Module* module){
    std::vector<RTLIL::Cell *> multi_cells;
    for(auto cell: module->selected_cells()){
        if(cell->get_string_attribute(ID::guide) == "multiplier"){
            multi_cells.push_back(cell);
        }
    }
    return multi_cells;
}

// generate wrapper for multipliers with different input width
bool gen_wrapper_multi(std::string filename, std::string module_name, std::string submodule_name,
    const Wire* A, const Wire* B, const Wire* Y)
{
    std::ofstream file;
    file.open(filename,std::ios::app);
    if(!file.is_open()){
        log_error("Can't open file %s\n", filename.c_str());
        return false;
    }
    
    auto width = A->width > B->width ? A->width : B->width;
    auto A_name = A->name.str()[0] == '\\' ? A->name.str().substr(1) : A->name.str();
    auto B_name = B->name.str()[0] == '\\' ? B->name.str().substr(1) : B->name.str();
    auto Y_name = Y->name.str()[0] == '\\' ? Y->name.str().substr(1) : Y->name.str();
    file << "module " << module_name << " (input [" << width - 1 << ":0] A, input [" << width - 1 << ":0] B, output [" << 2 * width - 1 << ":0] Y);\n";
    if(A->width > B->width){
        auto width_diff = A->width - B->width;
        file << "    wire [" << Y->width - 1 << ":0] sub_mod_Y;\n";
        file << "    " << submodule_name << " " << submodule_name + "_inst" << " (";
        
        file << " ." << A_name << "(A), ." << B_name << "(B["<<  B->width-1 << ":0]), ." << Y_name << "(sub_mod_Y));\n";
        // file << "    wire [" << width_diff - 1 << ":0] B_app;\n";
        file << "    wire [" << width*2 - 1 << ":0] Y_app;\n";
        file << "    assign Y_app = A * B["<<  width-1 << ":" << width-width_diff << "];\n";
        file << "    assign Y = (Y_app << " << width_diff << ") + sub_mod_Y ;\n";

    }
    else{
        auto width_diff = B->width - A->width;
        file << "    wire [" << Y->width - 1 << ":0] sub_mod_Y;\n";
        file << "    " << submodule_name << " " << submodule_name + "_inst" << " (";
        file << " ." << A_name << "(A[" << A->width-1 << ":0], " << B_name << "(B), ." << Y_name << "(sub_mod_Y));\n";
        // file << "    wire [" << width_diff - 1 << ":0] A_app;\n";
        file << "    wire [" << width*2 - 1 << ":0] Y_app;\n";
        file << "    assign Y_app = B * A[ "<<  width-1 << ":" << width-width_diff << "];\n";
        file << "    assign Y = (Y_app << " << width_diff << ") + sub_mod_Y ;\n";
    }
    // file << "    " << submodule_name << " " << submodule_name + "_inst" << " (";
    file << "endmodule" << std::endl;
    file.close();
    return true;
}


void extract_multi_delete_cells(RTLIL::Design* design){
    
    for(auto module: design->selected_whole_modules_warn()){
        auto multi_cells = get_multi_cells(module);    
        for(auto cell: multi_cells){
            auto a = cell->getPort(ID::A);
            auto b = cell->getPort(ID::B);
            auto y = cell->getPort(ID::Y);  

            if(a.size() + b.size() > y.size()){
                RTLIL::SigSpec a_in;
                RTLIL::SigSpec b_in;
                std::cout << "Finding Connections" << std::endl;
                for(auto conn: cell->connections()){
                    if(conn.first == ID::A){
                        for(auto chunk: conn.second.chunks()){
                            std::cout << "chunk: " << log_signal(chunk) << std::endl;
                            if(chunk.is_wire()){
                                a_in = conn.second.extract(chunk);
                                std::cout << "find a ----> " << log_signal(a_in) << std::endl;
                            }
                        }
                        
                    }
                    else if(conn.first == ID::B){
                        for(auto chunk: conn.second.chunks()){
                            std::cout << "chunk: " << log_signal(chunk) << std::endl;
                            if(chunk.is_wire()){
                                b_in = conn.second.extract(chunk);
                                std::cout << "find b -----> " << log_signal(b_in) << std::endl;
                            }
                        }
                    }
                }
                std::cout << "a_in.size(): " << a_in.size() << std::endl;
                std::cout << "b_in.size(): " << b_in.size() << std::endl;
                // for(auto conn: module->connections()){
                //     if()
                // }
                
                auto a_conn = y.extract(0, a_in.size());
                auto b_conn = y.extract(a_in.size(), b_in.size());
                module->connect(a_in, a_conn);
                module->connect(b_in, b_conn);
            }else{
                auto a_conn = y.extract(0, a.size());
                auto b_conn = y.extract(a.size(), b.size());
                module->connect(a_conn, a);
                module->connect(b_conn, b);
            }
            std::cout << "Delete Cell: " << cell->name.str() << std::endl;
            module->remove(cell);
        }
    }
}


void extract_multi(RTLIL::Design* design, bool delete_flag){
    std::set<RTLIL::IdString> multi_modules_name;
    std::vector<RTLIL::Module *> multi_modules;
    std::vector<std::string> aig_files;

    auto v_tmp_file = make_temp_file();
    Pass::call(design, "write_verilog "  + v_tmp_file);

    for(auto module: design->selected_whole_modules_warn()){
        if(module->name.str().find("\\guide_multigen_") != std::string::npos){
            continue;
        }
        
        auto multi_cells = get_multi_cells(module);
        for(auto cell: multi_cells){
            auto a = cell->getPort(ID::A);
            auto b = cell->getPort(ID::B);
            auto y = cell->getPort(ID::Y);
            auto type = cell->type;
            auto a_width = a.size();
            auto b_width = b.size();
            auto y_width = y.size();
            std::cout << "Extracting Multiplier: " << cell->name.str() << std::endl;
            std::cout << " type: "  << type.str() << std::endl;
            std::cout << "    a: "  << " width: " << a_width << std::endl;
            std::cout << "    b: "  << " width: " << b_width << std::endl;
            std::cout << "    y: "  << " width: " << y_width << std::endl;
            multi_modules_name.insert(type);


            for(auto conn: cell->connections()){
                if(conn.first == ID::A){
                    std::cout << "    A: " << log_signal(conn.second) << std::endl;
                }else if(conn.first == ID::B){
                    std::cout << "    B: " << log_signal(conn.second) << std::endl;
                }else if(conn.first == ID::Y){
                    std::cout << "    Y: " << log_signal(conn.second) << std::endl;
                }
            }
        }
    }



    for(auto module: design->selected_whole_modules_warn()){
        if(std::find(multi_modules_name.begin(), multi_modules_name.end(), module->name) 
            != multi_modules_name.end()){
            multi_modules.push_back(module);
            std::cout << module->name.str() <<  " is a multiplier" <<std::endl;
            std::string module_name = module->name.str();

            auto aig_tmp_file = make_temp_file();
            aig_files.push_back(aig_tmp_file);


            RTLIL::Wire* A = nullptr, *B = nullptr;
            for(auto port: module->ports){
                std::cout << "Port: " << port.str() << std::endl;
                auto w = module->wire(port);
                if(w->port_input && A == nullptr){
                    A = w;
                    continue;
                }
                if(w->port_input && B == nullptr){
                    B = w;
                    break;
                }
            }
            if(A == nullptr || B == nullptr){
                log_error("Invalid module ports.\n");
                return;
            }

            // We need a wrapper module to make the input width match
            if( A->width != B->width){
                gen_wrapper_multi(v_tmp_file, module_name + "wrap", module_name, A, B, module->wire("\\Y"));
                module_name = module_name + "wrap";
            }


            std::string yosys_cmd = "read_verilog " + v_tmp_file + "; ";
            yosys_cmd += "hierarchy -top " + module_name + "; ";
            yosys_cmd += "synth -flatten; ";
            yosys_cmd += "techmap; ";
//           yosys_cmd += "opt_clean; ";
            yosys_cmd += "aigmap; ";
            yosys_cmd += "write_aiger " + aig_tmp_file;

            std::cout << "Run yosys with \"" << yosys_cmd << "\"" <<std::endl;
            
            std::cout << "Writing to aig file: " << aig_tmp_file << std::endl;
            system(("yosys -p \"" + yosys_cmd  + "\"").c_str());            
        }
    }

    for(auto aig_file: aig_files){
        auto miter_tmp_file = make_temp_file();
        auto rewritten_tmp_file = make_temp_file();
        auto amulet_sub_cmd = "amulet -substitute " + aig_file + " " + miter_tmp_file  + " " + rewritten_tmp_file;
        std::cout << "Running amulet: " << amulet_sub_cmd << std::endl;
        auto ret = system(amulet_sub_cmd.c_str());
        // if(WEXITSTATUS(ret) != 1){
        //     log_error("Amulet failed.\n");
        //     return;
        // }
        // TODO: add kissat to verify the `miter` cnf file
        auto amulet_veri_cmd = "amulet -verify " + rewritten_tmp_file;
        ret = system(amulet_veri_cmd.c_str());
        if(WEXITSTATUS(ret) != 1){
            log_error("Amulet Verify failed.\n");
            return;
        }
        remove(rewritten_tmp_file.c_str());
        remove(miter_tmp_file.c_str());
    }

    for(auto aig_file: aig_files){
        remove(aig_file.c_str());
    }

    remove(v_tmp_file.c_str());

    if(delete_flag){
        extract_multi_delete_cells(design);
        for(auto module: multi_modules){
            std::cout << "Remove Module: " << module->name.str() << std::endl;
            design->remove(module);
        }
        Pass::call(design, "hierarchy -top " + design->top_module()->name.str());
    }
}


struct ExtractMultiPass : public Pass {
	ExtractMultiPass() : Pass("extract_multi", "Extract the Multiplier") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    extract -d\n");
		log("\n");
		log("    -d\n");
		log("        Delete the Multiplier\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override{
        bool delete_flag = false;
        if(args.size() > 2){
            log_cmd_error("Invalid number of arguments.\n");
            return;
        }
        if(args.size() == 2){
            if(args[1] == "-d"){
                delete_flag = true;
            }else{
                log_cmd_error("Invalid arguments.\n");
                return;
            }
        }
        
        if(design->top_module() == nullptr){
            log_cmd_error("No top module found.\n");
            return;
        }
        extract_multi(design, delete_flag);

    }

} ExtractMultiPass;


PRIVATE_NAMESPACE_END
