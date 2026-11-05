#include "simulation.hpp"
#include <filesystem>
#include <stdexcept>
#include <iostream>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 life::Mulberry32 random(99);life::Nnue original(random);const std::vector<uint16_t> first{0,21,42,1601},second{2,23,44,1602};original.evaluate(first);auto frozen=original.cloneRuntime();require(original.genome()==frozen->genome(),"Freeze copied immutable weights");const auto expected=original.evaluate(second);require(expected==frozen->evaluate(second),"Runtime freeze lost inference cache");const auto source=original.genome();frozen->mutate(random);require(source==original.genome()&&source!=frozen->genome(),"Neural mutation changed a shared genome");
 life::NativeSimulation world(32,24,.2,500,53,17);world.seedBenchmarkMovers(25);world.step(40);auto path=std::filesystem::current_path()/"research-v3-test.lfesave";world.save(path);life::NativeSimulation restored(32,24,.2,500,9,9);restored.load(path);world.step(100);restored.step(100);require(world.cells==restored.cells&&world.owners==restored.owners,"Version 3 replay differs");for(size_t i=0;i<world.organisms().size();++i)require(world.organisms()[i].lastOutputs==restored.organisms()[i].lastOutputs,"Version 3 lost neural runtime state");std::filesystem::remove(path);std::cout<<"Version 3 and neural genome/runtime separation passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
