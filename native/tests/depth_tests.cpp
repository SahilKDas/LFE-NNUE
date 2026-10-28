#include "simulation.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
namespace life {struct NativeSimulationTestAccess {static void drainage(NativeSimulation& s){s.updateDrainage();}static void tick(NativeSimulation& s,int value){s.ticks_=value;}};}
void require(bool valid,const char* message){if(!valid)throw std::runtime_error(message);}
int main(){try{
  const auto directory=std::filesystem::current_path()/"test-archives";std::filesystem::create_directories(directory);const auto path=directory/"depth-test.lfesave";
  life::NativeSimulation original(96,60,.2,500,524114809,334462);original.seedBenchmarkMovers(100);original.step(360);original.select(1);original.savedView.zoom=2.5f;original.savedView.cameraX=-137;original.savedView.weather=false;original.savedView.fps=120;original.savedView.tool=11;original.save(path);original.save(path);
  life::NativeSimulation restored(96,60,.2,500,4,9);restored.load(path);
  require(original.cells==restored.cells&&original.owners==restored.owners&&original.moisture==restored.moisture,"Roundtrip world differs");
  require(restored.savedView.zoom==2.5f&&restored.savedView.cameraX==-137&&!restored.savedView.weather&&restored.savedView.fps==120&&restored.savedView.tool==11,"View and controls did not restore");
  require(original.atlasSnapshot().timeline.size()==restored.atlasSnapshot().timeline.size(),"History missing");
  const auto history=restored.atlasSnapshot();require(history.timeline.back().tick==360&&history.timeline.back().rainfall==original.rainfall()&&history.timeline.back().averageMoisture>=0,"Habitat/weather history missing");
  for(size_t i=0;i<original.organisms().size();++i)for(size_t j=i+1;j<original.organisms().size();++j)require((original.organisms()[i].brain==original.organisms()[j].brain)==(restored.organisms()[i].brain==restored.organisms()[j].brain),"Shared brain identity changed");
  original.step(360);restored.step(360);
  require(original.cells==restored.cells&&original.owners==restored.owners&&original.world.nutrients==restored.world.nutrients&&original.moisture==restored.moisture,"Loaded replay diverges");
  require(original.organisms().size()==restored.organisms().size(),"Loaded lineage diverges");
  const auto expectedAtlas=original.atlasSnapshot(),loadedAtlas=restored.atlasSnapshot();require(expectedAtlas.timeline.size()==loadedAtlas.timeline.size()&&expectedAtlas.species.size()==loadedAtlas.species.size()&&expectedAtlas.events.size()==loadedAtlas.events.size(),"Loaded Atlas diverges");for(size_t i=0;i<expectedAtlas.timeline.size();++i){const auto& a=expectedAtlas.timeline[i];const auto& b=loadedAtlas.timeline[i];require(a.tick==b.tick&&a.population==b.population&&a.births==b.births&&a.deaths==b.deaths&&a.cavePopulation==b.cavePopulation&&a.bridgePopulation==b.bridgePopulation&&a.rainfall==b.rainfall&&a.averageMoisture==b.averageMoisture,"Loaded habitat history diverges");}
  for(size_t i=0;i<original.organisms().size();++i){const auto& a=original.organisms()[i];const auto& b=restored.organisms()[i];require(a.id==b.id&&a.energy==b.energy&&a.damage==b.damage&&a.lastOutputs==b.lastOutputs&&a.speciesId==b.speciesId&&a.lineage->mutations==b.lineage->mutations,"Loaded organism or neural state differs");}
  const auto before=restored.cells;std::filesystem::resize_file(path,32);bool rejected=false;try{restored.load(path);}catch(...){rejected=true;}require(rejected&&restored.cells==before,"Failed load changed current world");
  auto recovery=path;recovery+=".bak";restored.load(recovery);require(restored.metrics().ticks==360,"Previous save backup did not recover");original.save(path);{std::fstream damaged(path,std::ios::binary|std::ios::in|std::ios::out);damaged.seekg(64);char byte{};damaged.read(&byte,1);byte^=1;damaged.seekp(64);damaged.write(&byte,1);}rejected=false;const int priorTick=restored.metrics().ticks;try{restored.load(path);}catch(...){rejected=true;}require(rejected&&restored.metrics().ticks==priorTick,"Corrupt save was accepted or replaced the world");
  life::NativeSimulation runoff(8,8,.2,500,1,1);runoff.world.terrain.assign(64,uint8_t(life::TerrainType::Plains));runoff.world.habitat.assign(64,uint8_t(life::HabitatType::Surface));runoff.world.elevation.assign(64,200);runoff.world.elevation[28]=100;runoff.world.nutrients.assign(64,0);runoff.world.nutrients[27]=1000;runoff.moisture.assign(64,0);runoff.moisture[27]=900;life::NativeSimulationTestAccess::drainage(runoff);require(runoff.world.nutrients[27]<1000&&runoff.world.nutrients[28]>0,"Nutrients did not drain downhill");require(runoff.moisture[28]>0,"Water did not drain downhill");
  runoff.world.terrain[28]=uint8_t(life::TerrainType::Water);life::NativeSimulationTestAccess::drainage(runoff);require(runoff.world.nutrients[28]==0,"Water retained usable nutrients");
  const int startRain=runoff.rainfall();life::NativeSimulationTestAccess::tick(runoff,24000);require(runoff.rainfall()==startRain,"Weather cycle failed to wrap");life::NativeSimulationTestAccess::tick(runoff,6000);require(runoff.rainfall()==52,"Seasonal rainfall peak differs");life::NativeSimulationTestAccess::tick(runoff,18000);require(runoff.rainfall()==12,"Seasonal rainfall minimum differs");
  std::filesystem::remove(path);auto backup=path;backup+=".bak";std::filesystem::remove(backup);
  std::cout<<"Depth, drainage, archive recovery, and neural replay passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
