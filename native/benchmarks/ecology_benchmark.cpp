#include "simulation.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>

namespace {
uint64_t digest(const life::NativeSimulation& simulation){
  uint64_t value=1469598103934665603ULL;
  const auto mix=[&](uint64_t item){value^=item;value*=1099511628211ULL;};
  for(const auto cell:simulation.cells)mix(cell);
  for(const auto owner:simulation.owners)mix(uint32_t(owner));
  for(const auto& sample:simulation.atlasSnapshot().timeline){mix(sample.tick);mix(sample.population);mix(sample.births);for(const int deaths:sample.deaths)mix(deaths);}
  for(const auto& species:simulation.atlasSnapshot().species){mix(species.id);mix(species.parentId);mix(species.population);mix(species.extinct);}
  return value;
}
}

int main(int argc,char** argv){
  const int ticks=argc>1?std::max(300,std::atoi(argv[1])):12'000;
  life::NativeSimulation first(life::WorldWidth,life::WorldHeight,.2,500,524114809,334462);
  life::NativeSimulation second(life::WorldWidth,life::WorldHeight,.2,500,524114809,334462);
  if(first.seedBenchmarkMovers(10'000)!=10'000||second.seedBenchmarkMovers(10'000)!=10'000)return 2;
  first.step(ticks);second.step(ticks);
  const auto metrics=first.metrics();const auto atlas=first.atlasSnapshot();
  const int livingSpecies=int(std::count_if(atlas.species.begin(),atlas.species.end(),[](const life::SpeciesSummary& species){return !species.extinct&&species.population>0;}));
  const int extinctions=int(std::count_if(atlas.events.begin(),atlas.events.end(),[](const life::EvolutionEvent& event){return event.type==life::EvolutionEventType::Extinction;}));
  const bool deterministic=digest(first)==digest(second);
  std::cout<<std::fixed<<std::setprecision(3)<<"ticks="<<ticks<<" organisms="<<metrics.organisms<<" births="<<first.birthCount()<<" deaths="<<first.deadLineageRecordCount()<<" plant="<<atlas.timeline.back().plant<<" carrion="<<atlas.timeline.back().carrion<<" mineral="<<atlas.timeline.back().mineral<<" living_species="<<livingSpecies<<" retained_species="<<atlas.species.size()<<" extinctions="<<extinctions<<" samples="<<atlas.timeline.size()<<" deterministic="<<(deterministic?"yes":"no")<<" digest="<<std::hex<<digest(first)<<'\n';
  return deterministic&&!atlas.timeline.empty()&&atlas.timeline.size()<=24'000&&atlas.species.size()<=2'048&&metrics.organisms>0?0:1;
}
