#include "simulation.hpp"
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc,char** argv){
  const int seconds=argc>1?std::max(1,std::atoi(argv[1])):60;
  life::NativeSimulation simulation(life::WorldWidth,life::WorldHeight,.2,1'000'000'000,524114809,334462);
  const int populated=simulation.seedBenchmarkMovers(10'000);
  if(populated!=10'000){std::cerr<<"Unable to place 10,000 benchmark movers.\n";return 2;}
  simulation.step(60);
  const auto start=std::chrono::steady_clock::now(),deadline=start+std::chrono::seconds(seconds);
  int ticks=0;
  while(std::chrono::steady_clock::now()<deadline){simulation.step();++ticks;}
  const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  const auto metrics=simulation.metrics();
  std::cout<<std::fixed<<std::setprecision(2)
    <<"organisms="<<metrics.organisms<<" elapsed_s="<<elapsed<<" ticks="<<ticks
    <<" measured_tps="<<(ticks/elapsed)<<" nnue_eval_s="<<(metrics.nnueEvaluations/elapsed)<<'\n';
  return metrics.organisms==10'000&&ticks/elapsed>=60.0?0:1;
}
