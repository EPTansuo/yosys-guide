#include "kernel/log.h"
#include "kernel/register.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include <algorithm>
#include <fstream>
#include <string_view>
#include <sys/wait.h> 
#include <string>
#include <vector>

USING_YOSYS_NAMESPACE


PRIVATE_NAMESPACE_BEGIN

#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>

void extract_retime_module(RTLIL::Design *design, const std::string& outpath, 
                            const std::string& prefix, bool set_blackbox){
    
    if (outpath.empty()) {
        log_cmd_error("Output path is not specified.\n");
        return;
    }

    struct stat st;
    if((stat(outpath.c_str(), &st) == 0)){
        rmdir(outpath.c_str());
    }


    try {
        if (mkdir(outpath.c_str(), 0777) != 0 && errno != EEXIST) {
            log_error("Failed to create output directory: %s\n", strerror(errno));
        }
    } catch (...) {
        log_error("An unexpected error occurred while creating the output directory.\n");
    }


    for (auto module : design->modules()) {
        // Check if the module has been retimed
        std::string guide_attr = module->get_string_attribute(ID::guide);
        std::string mod_name = module->name.str();
        if(mod_name[0] == '$'){
            mod_name[0] = '_';
        } else if(mod_name[0] == '\\'){
            mod_name = mod_name.substr(1);
        }

        if (guide_attr.find("retimed") != std::string::npos) {
            run_pass("select " + module->name.str());
            if(module->get_bool_attribute(ID::cells_not_processed)){
                run_pass("proc");
            }
            run_pass("write_verilog -selected " + outpath + "/" + prefix + mod_name + ".v");
            run_pass("select -clear");
            log("Extracted retimed module: %s\n", module->name.str().c_str());
            if(set_blackbox){
                module->set_bool_attribute(ID::blackbox, true);
            }
        }

        
        

    }
}


struct ExtractRetimePass : public Pass {
	ExtractRetimePass() : Pass("extract_retime", "Extract the Module that been retimed") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    extract_retime [options]\n");
        log("\n");
        log("    -outpath <path>\n");
        log("        Specify the output path for the extracted module.\n");
        log("\n");
        log("    -prefix <prefix>\n");
        log("        Specify the prefix for the output filename name.\n");
        log("\n");
        log("    -set_blackbox\n");
        log("        Set the extracted module as blackbox.\n");
		log("\n");

	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override{

        std::string outpath = "";
        std::string prefix = "";
        bool set_blackbox = false;

        for(unsigned int idx=1; idx < args.size(); idx++){
            if(args[idx] == "-outpath" && idx+1 < args.size()){
                outpath = args[++idx];
            }
            else if(args[idx] == "-prefix" && idx+1 < args.size()){
                prefix = args[++idx];
            }
            else if(args[idx] == "-set_blackbox"){
                set_blackbox = true;
            }
            else{
                log_cmd_error("Invalid arguments.\n");
                return;
            }
        }

        log_header(design, "Executing EXTRACT_RETIME pass.\n");
        // run_pass("proc");
        extract_retime_module(design, outpath,prefix, set_blackbox);
    }

} ExtractRetimePass;


PRIVATE_NAMESPACE_END
