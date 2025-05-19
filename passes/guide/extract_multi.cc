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

#include <iostream>
#include <cstdio>
#include <string>
#include <vector>

int exectue_and_check(const std::string & cmd, bool & correct, 
                      const std::string & target_output) {
    correct = false;
    char buffer[1024];
    std::string output;

    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        log_error("Error executing command: ");
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
        std::cout<< buffer;
        if (output.find(target_output) != std::string::npos) {
            correct = true;
        }
    }

    
    int status = pclose(pipe);
    if (WIFEXITED(status)) {
        status = WEXITSTATUS(status);
    } else {
        status = -1; 
    }

    return status;
}


int execute_dynphaseorderopt_and_check(const std::string& command, 
                     bool& correct) {
    correct = false;
    const std::string target_line = "CIRCUIT IS CORRECT.";
    return exectue_and_check(command, correct, target_line);
}

int execute_revsca_and_check(const std::string& command, 
                     bool& correct) {
    correct = false;
    const std::string target_line = "The multiplier is correct!";
    return exectue_and_check(command, correct, target_line);
}


std::vector<RTLIL::Cell *> get_multi_cells(RTLIL::Module* module){
    std::vector<RTLIL::Cell *> multi_cells;
    for(auto cell: module->selected_cells()){
        if(cell->get_string_attribute(ID::guide).find("multiplier")!= std::string::npos){
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
            std::cout << "a.size()" << a.size() << std::endl;
            std::cout << "b.size()" << b.size() << std::endl;
            std::cout << "y.size()" << y.size() << std::endl;   

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
                std::cout << "y_out.size(): " << y.size() << std::endl;
                // for(auto conn: module->connections()){
                //     if()
                // }
                
                auto a_conn = y.extract(0, a_in.size());
                auto b_conn = y.extract(a_in.size(), y.size()-a_in.size());
                module->connect(a_in, a_conn);
                module->connect(b_in.extract(0, y.size()-a_in.size()), b_conn);
            }else if(a.size() + b.size() < y.size()) { 
                log_warning("Y width is smaller than A width + B width\n");
                auto a_conn = y.extract(0, a.size());
                auto b_conn = y.extract(a.size(), y.size()-a.size());
                module->connect(a_conn, a);
                module->connect(b_conn, b);
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


void extract_multi(RTLIL::Design* design, bool delete_flag, int mode){
    std::set<RTLIL::IdString> multi_modules_name;
    // std::map<RTLIL::IdString, bool> multi_sign_map;
    std::vector<RTLIL::Module *> multi_modules;
    std::vector<std::string> aig_files;
    std::vector<bool> sign;

    auto v_tmp_file = make_temp_file();
    Pass::call(design, "write_verilog "  + v_tmp_file);

    for(auto module: design->selected_whole_modules_warn()){
        // if(module->name.str().find("\\guide_multigen_") != std::string::npos){
        //     continue;
        // }
        
        if(module->get_string_attribute(ID::guide).find("multiplier") != std::string::npos){
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

            // if(cell->getParam(ID::A_SIGNED).as_bool() != cell->getParam(ID::B_SIGNED).as_bool()){
            //     log_error("A_SIGNED and B_SIGNED are not the same!\n");
            //     return;
            // }
            // multi_sign_map.insert(std::make_pair(type, cell->getParam(ID::A_SIGNED).as_bool()));
            // multi_sign_map.insert(std::make_pair(type, cell->ty));

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


    std::map<std::string, bool> aig_sign_map;
    for(auto module: design->selected_whole_modules_warn()){
        if(std::find(multi_modules_name.begin(), multi_modules_name.end(), module->name) 
            != multi_modules_name.end()){
            multi_modules.push_back(module);
            std::cout << module->name.str() <<  " is a multiplier" <<std::endl;
            std::string module_name = module->name.str();
            
            auto aig_tmp_file = make_temp_file();
            aig_files.push_back(aig_tmp_file);
            // aig_modname_map.insert(std::make_pair(module_name, module->name));
            aig_sign_map.insert(std::make_pair(aig_tmp_file, module->name.str().find("_signed") != std::string::npos));

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
        
        auto sign = aig_sign_map[aig_file];
        if(mode == 0){
            log("Using dynphaseorderopt to verify the multiplier.\n");
            auto dynphaseorderopt_cmd = "dynphaseorderopt " + aig_file;
            auto correct = false;
            execute_dynphaseorderopt_and_check(dynphaseorderopt_cmd, correct);
            if(!correct){
                log_error("Dynphaseorderopt Verify failed.\n");
                // return;
            }
        }
        else if(mode == 1){
            log("Using amulet to verify the multiplier.\n");
            auto miter_tmp_file = make_temp_file();
            auto rewritten_tmp_file = make_temp_file();
            auto amulet_sub_cmd = "amulet -substitute " + aig_file + " " + miter_tmp_file  + " " + rewritten_tmp_file + (sign? " -signed" : "");
            std::cout << "Running amulet: " << amulet_sub_cmd << std::endl;
            auto ret = system(amulet_sub_cmd.c_str());
            // if(WEXITSTATUS(ret) != 1){
            //     log_error("Amulet failed.\n");
            //     return;
            // }
            // TODO: add kissat to verify the `miter` cnf file
            auto amulet_veri_cmd = "amulet -verify " + rewritten_tmp_file + (sign ? " -signed" : "");

            std::cout << "Running amulet: " << amulet_veri_cmd << std::endl;

            ret = system(amulet_veri_cmd.c_str());
            if(WEXITSTATUS(ret) != 1){
                log_error("Amulet Verify failed.\n");
                // return;
            }
            remove(rewritten_tmp_file.c_str());
            remove(miter_tmp_file.c_str());
        }else if(mode == 2){
            log("Using RevSCA to verify the multiplier.\n");

            auto out_tmp_file = make_temp_file();
            auto revsca_cmd = "revsca " + aig_file + " " + out_tmp_file + (sign ? " -s": " -u");
            auto correct = false;
            execute_revsca_and_check(revsca_cmd, correct);
            if(!correct){
                log_error("RevSCA Verify failed.\n");
                // return;
            }
            remove(out_tmp_file.c_str());
        }

    }

    for(auto aig_file: aig_files){
        // TODO: Remove aig files
        // remove(aig_file.c_str());
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
		log("    extract [options]\n");
		log("\n");
		log("    -d\n");
		log("        Delete the Multiplier (default: do not delete). \n");
        log("\n");
        log("    -amulet\n");
        log("        Use amulet to verify the multiplier. Equivanence to \"-mode 1\"\n");
        log("    -mode <num>\n");
        log("        select tool to verify the multiplier (default: dynphaseorderopt). \n");
        log("             0: dynphaseorderopt\n");
        log("             1: amulet\n");
        log("             2: revsca");
        log("        Only amulet and revsca can verify the multiplier with signed input.\n");
        log("        dynphaseorderopt: Konrad, Alexander, and Christoph Scholl. Symbolic Computer Algebra for \n");
        log("        Multipliers Revisited-It's All About Orders and Phases.\n");
        log("        Amulet: Kaufmann, Daniela, and Armin Biere. AMulet 2.0 for verifying multiplier circuits.\n");
        log("        RevSCA: Mahzoon, Alireza, Daniel Große, and Rolf Drechsler. RevSCA-2.0: SCA-based formal \n");
        log("        verification of nontrivial multipliers using reverse engineering and local vanishing removal.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override{
        bool delete_flag = false;
        int mode = 0;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++)
		{
            if (args[argidx] == "-d") {
                delete_flag = true;
                continue;
            }
            else if (args[argidx] == "-amulet"){
                mode = 1;
                continue;
            }
            else if (args[argidx] == "-mode" && argidx+1 < args.size()) {
                mode = std::stoi(args[++argidx]);
                continue;
            }
            else {
                log_cmd_error("Invalid arguments.\n");
                return;
            }
		}

        
        if(design->top_module() == nullptr){
            log_cmd_error("No top module found.\n");
            return;
        }
        extract_multi(design, delete_flag, mode);

    }

} ExtractMultiPass;


PRIVATE_NAMESPACE_END
