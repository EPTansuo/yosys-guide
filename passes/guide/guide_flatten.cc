#include "kernel/yosys.h"
#include <fstream>

USING_YOSYS_NAMESPACE


PRIVATE_NAMESPACE_BEGIN


void print_all_scopeinfo(RTLIL::Module *module){
    for(auto cell: module->selected_cells()){
         if(cell->type == ID($scopeinfo)){
            std::cout << "ScopeInfo: " << cell->name.str() << std::endl;
            for(auto attr: cell->attributes){
                std::cout << "    " << attr.first.str() << " : " << attr.second.decode_string() << std::endl;
            }
         }
    }
}


struct GuideFlattenPass : public Pass {
	GuideFlattenPass() : Pass("guide_flatten", "write the formal verification guide") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    guide_flatten \n");
        log("Collect information after flatten");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override{
        (void)args;
        std::cout << "GuideFlattenPass::execute" << std::endl;  

        for(auto module: design->selected_whole_modules_warn()){
            print_all_scopeinfo(module);
        }
    }

} GuideFlattenPass;


PRIVATE_NAMESPACE_END
