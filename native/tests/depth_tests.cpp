#include "simulation.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
void require(bool valid,const char* message){if(!valid)throw std::runtime_error(message);}
int main(){try{
  const auto directory=std::filesystem::current_path()/"test-archives";std::filesystem::create_directories(directory);const auto path=directory/"depth-test.lfesave";
  life::NativeSimulation original(96,60,.2,500,524114809,334462);original.seedBenchmarkMovers(100);original.step(360);original.select(1);original.savedView.zoom=2.5f;original.savedView.cameraX=-137;original.savedView.weather=false;original.savedView.fps=120;original.savedView.tool=9;original.save(path);original.save(path);
  life::NativeSimulation restored(96,60,.2,500,4,9);restored.load(path);
  require(original.cells==restored.cells&&original.owners==restored.owners,"Roundtrip world differs");
  require(restored.savedView.zoom==2.5f&&restored.savedView.cameraX==-137&&!restored.savedView.weather&&restored.savedView.fps==120&&restored.savedView.tool==9,"View and controls did not restore");
  require(original.atlasSnapshot().timeline.size()==restored.atlasSnapshot().timeline.size(),"History missing");
  const auto history=restored.atlasSnapshot();require(history.timeline.back().tick==360&&history.timeline.back().rainfall==0&&history.timeline.back().averageMoisture>=0,"Habitat/weather history missing");
  for(size_t i=0;i<original.organisms().size();++i)for(size_t j=i+1;j<original.organisms().size();++j)require((original.organisms()[i].brain==original.organisms()[j].brain)==(restored.organisms()[i].brain==restored.organisms()[j].brain),"Shared brain identity changed");
  original.step(360);restored.step(360);
  require(original.cells==restored.cells&&original.owners==restored.owners&&original.world.nutrients==restored.world.nutrients,"Loaded replay diverges");
  require(original.organisms().size()==restored.organisms().size(),"Loaded lineage diverges");
  const auto expectedAtlas=original.atlasSnapshot(),loadedAtlas=restored.atlasSnapshot();require(expectedAtlas.timeline.size()==loadedAtlas.timeline.size()&&expectedAtlas.species.size()==loadedAtlas.species.size()&&expectedAtlas.events.size()==loadedAtlas.events.size(),"Loaded Atlas diverges");for(size_t i=0;i<expectedAtlas.timeline.size();++i){const auto& a=expectedAtlas.timeline[i];const auto& b=loadedAtlas.timeline[i];require(a.tick==b.tick&&a.population==b.population&&a.births==b.births&&a.deaths==b.deaths&&a.cavePopulation==b.cavePopulation&&a.bridgePopulation==b.bridgePopulation&&a.rainfall==b.rainfall&&a.averageMoisture==b.averageMoisture,"Loaded habitat history diverges");}
  for(size_t i=0;i<original.organisms().size();++i){const auto& a=original.organisms()[i];const auto& b=restored.organisms()[i];require(a.id==b.id&&a.energy==b.energy&&a.damage==b.damage&&a.lastOutputs==b.lastOutputs&&a.speciesId==b.speciesId&&a.lineage->mutations==b.lineage->mutations,"Loaded organism or neural state differs");}
  const auto before=restored.cells;std::filesystem::resize_file(path,32);bool rejected=false;try{restored.load(path);}catch(...){rejected=true;}require(rejected&&restored.cells==before,"Failed load changed current world");
  auto recovery=path;recovery+=".bak";restored.load(recovery);require(restored.metrics().ticks==360,"Previous save backup did not recover");original.save(path);{std::fstream damaged(path,std::ios::binary|std::ios::in|std::ios::out);damaged.seekg(64);char byte{};damaged.read(&byte,1);byte^=1;damaged.seekp(64);damaged.write(&byte,1);}rejected=false;const int priorTick=restored.metrics().ticks;try{restored.load(path);}catch(...){rejected=true;}require(rejected&&restored.metrics().ticks==priorTick,"Corrupt save was accepted or replaced the world");
  for(int version:{1,2}){life::NativeSimulation legacy(16,12,.2,500,1,1);legacy.load(std::filesystem::path(LEGACY_FIXTURE_DIR)/("legacy-depth-v"+std::to_string(version)+".lfesave"));require(life::passable(legacy.world,4*16+3)&&life::passable(legacy.world,4*16+4),"Legacy cave/bridge did not flatten to land");require(legacy.savedView.overlay==0&&legacy.savedView.tool==0&&!legacy.savedView.weather,"Legacy view did not migrate to 2D");for(const auto& sample:legacy.atlasSnapshot().timeline)require(sample.cavePopulation==0&&sample.bridgePopulation==0&&sample.rainfall==0,"Legacy depth history remained active");legacy.step(60);}
  auto flat=life::generateWorld(96,60,53);for(size_t i=0;i<flat.terrain.size();++i)require(life::passable(flat,int(i))==(flat.terrain[i]<uint8_t(life::TerrainType::Water)),"2D terrain passability differs");
  std::filesystem::remove(path);auto backup=path;backup+=".bak";std::filesystem::remove(backup);
  std::cout<<"Flat worlds, legacy archives, archive recovery, and neural replay passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
