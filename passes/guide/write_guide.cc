#include "kernel/yosys.h"
#include <fstream>

USING_YOSYS_NAMESPACE


PRIVATE_NAMESPACE_BEGIN


void write_guide(std::ostream& f, RTLIL::Design *design){
    // f << "guide" << std::endl;
    f << "Have't Finished Yet!!!!!!\n";
    f << "{\n";
    // f << "  " << design->top_module()->name.str() << " : {\n";
    f << "  \"modules\": {\n";
    for(auto module: design->modules_){
        f << "    \"" << module.first.str() <<  "\": {" <<std::endl;
        
        for(auto wire: module.second->wires_){
            if(wire.second->attributes.count(ID::guide)){
                f << "      " << wire.first.str() << ": " << wire.second->attributes.at(ID::guide).decode_string() << std::endl;
            }
        }

        for(auto cell: module.second->cells_){
            if(cell.second->attributes.count(ID::guide)){
                f << "      " << cell.first.str() << ": " << cell.second->attributes.at(ID::guide).decode_string() << ";" <<std::endl;
            }
        }
        f << "    }\n";
    }
    f << "  }\n";
    f << "}" << std::endl;
}


struct WriteGuidePass : public Pass {
	WriteGuidePass() : Pass("write_guide", "write the formal verification guide") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    write_guide [filename]\n");
		log("\n");
		log("If no file name is given, it will output to the stdout.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override{
        if(args.size() > 2){
            log_cmd_error("Invalid number of arguments.\n");
            return;
        }
        if(args.size() == 2){
            auto file = std::ofstream(args[1]);
            write_guide(file, design);
            file.close();
        }else{
            write_guide(std::cout, design);
        }
    }

} WriteGuidePass;


PRIVATE_NAMESPACE_END
