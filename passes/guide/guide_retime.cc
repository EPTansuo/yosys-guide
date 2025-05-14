#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include <cctype>
#include <fstream>
#include <ostream>
#include <string>
#include <unordered_set>
// #include <tclDecls.h>

USING_YOSYS_NAMESPACE


PRIVATE_NAMESPACE_BEGIN

struct GuideRetimePass : public Pass {
	GuideRetimePass() : Pass("guide_retime", "Add guidence for retiming") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    guide_retime <module_name> \n");
        log("Indicates the module has been retimed.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override{
        if (args.size() != 2) {
            log_cmd_error("Usage: guide_retime <module_name>\n");
            return;
        }
        
        std::string module_name = args[1];
        log_header(design, "Executing GUIDE_RETIME pass.\n");
        // std::cout << "GuideRetimePass::execute" << std::endl;  
        if(std::isalpha(module_name[0])){
            module_name = '\\' + module_name;
        }
        for(auto module: design->selected_whole_modules_warn()){
            std::cout << "Module Name: " << module->name.str() << std::endl;
            
            if(module->name == module_name){
                std::cout << "Set the guide attribute to retimed: " << module->name.str() << std::endl;
                auto attr_origin = module->get_string_attribute(ID::guide);

                module->set_string_attribute(ID::guide, 
                       attr_origin.empty() ?  "retimed": attr_origin + "; retimed");
            }
        }
    }

} GuideRetimePass;


PRIVATE_NAMESPACE_END
