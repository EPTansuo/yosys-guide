#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include <fstream>
#include <ostream>
#include <string>
// #include <tclDecls.h>

USING_YOSYS_NAMESPACE


PRIVATE_NAMESPACE_BEGIN



// ret: filename
std::string guide_genmulti(int a_width, int b_width, int y_width){
    std::string filename = make_temp_file();
    std::ofstream file(filename);
    file << "module guide_multigen_" << a_width << "x" << b_width << " (input [" << a_width-1 << ":0] A, input [" << b_width-1 
                                                                    << ":0] B, output [" << y_width-1 << ":0] Y);\n";
    file << "    assign Y = A * B;\n";
    file << "endmodule\n";
    file.close();
    return filename;
}


struct GuideMultiPass : public Pass {
	GuideMultiPass() : Pass("guide_multi", "Convert the $mul to a instance of multiplixer and preserve the infomation") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    guide_multi \n");
        log("Convert the $mul to a instance of multiplixer and preserve the infomation");
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
                    to_replace_cells.push_back(cell);
                }
            }
            int i = 0;
            
            std::cout <<"after read_verilog" << std::endl;
            for(auto to_replace_cell : to_replace_cells){

                auto a = to_replace_cell->getPort(ID::A);
                auto b = to_replace_cell->getPort(ID::B);
                auto y = to_replace_cell->getPort(ID::Y);

                auto a_width = a.size();
                auto b_width = b.size();
                auto y_width = y.size();


                auto file_name = guide_genmulti(a_width, b_width, y_width);
                Yosys::run_pass("read_verilog " + file_name);
                
                remove(file_name.c_str());
                
                std::cout << "before multigen_cell " << std::endl;
                std::string mod_name = "\\guide_multigen_" + std::to_string(a_width) + "x" + std::to_string(b_width);
                IdString name = mod_name + "_" + std::to_string(i) ; 
                IdString type = mod_name;


                RTLIL::Cell *cell = module->addCell(name, type);
                cell->connections_ = to_replace_cell->connections_;
                // cell->parameters = to_replace_cell->parameters;
                cell->attributes = to_replace_cell->attributes;

                
                cell->set_string_attribute(ID::guide, "multiplier");
                module->remove(to_replace_cell);

                i++;
                break;
            }
            
        }
    }

} GuideMultiPass;


PRIVATE_NAMESPACE_END
