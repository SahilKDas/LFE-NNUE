#include "simulation.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <unordered_set>

int main(int argc,char** argv){
  const int seconds=argc>1?std::max(1,std::atoi(argv[1])):60;
  life::NativeSimulation simulation(life::WorldWidth,life::WorldHeight,.2,500,524114809,334462);
  const int populated=simulation.seedBenchmarkMovers(10'000);
  if(populated!=10'000){std::cerr<<"Unable to place 10,000 benchmark movers.\n";return 2;}
  simulation.step(120);
  const int birthsBefore=simulation.birthCount(),deathsBefore=simulation.deadLineageRecordCount();
  const auto start=std::chrono::steady_clock::now(),deadline=start+std::chrono::seconds(seconds);
  int ticks=0;
  while(std::chrono::steady_clock::now()<deadline){simulation.step();++ticks;}
  const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  const auto metrics=simulation.metrics();
  const auto plantTiles=std::count(simulation.world.resources.begin(),simulation.world.resources.end(),uint8_t(life::ResourceType::Plant));
  std::unordered_set<const life::Nnue*> brainFamilies;for(const auto& organism:simulation.organisms())if(organism.living&&organism.brain)brainFamilies.insert(organism.brain.get());
  const int births=simulation.birthCount()-birthsBefore,deaths=simulation.deadLineageRecordCount()-deathsBefore;
  std::cout<<std::fixed<<std::setprecision(2)
    <<"organisms="<<metrics.organisms<<" elapsed_s="<<elapsed<<" ticks="<<ticks
    <<" measured_tps="<<(ticks/elapsed)<<" nnue_eval_s="<<(metrics.nnueEvaluations/elapsed)
    <<" brain_families="<<brainFamilies.size()<<" plant_tiles="<<plantTiles<<" oxygen="<<metrics.oxygen<<" co2="<<metrics.carbonDioxide<<" births="<<births<<" deaths="<<deaths<<'\n';
  return metrics.organisms>=8'500&&metrics.organisms<=10'000&&brainFamilies.size()>=128&&plantTiles>1'000&&births>0&&deaths>0&&ticks/elapsed>=60.0?0:1;
}
