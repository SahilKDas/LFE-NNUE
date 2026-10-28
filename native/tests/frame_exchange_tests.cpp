#include "frame_exchange.hpp"
#include <iostream>
#include <thread>
#include <mutex>
#include <stdexcept>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
  life::NativeSimulation simulation(96,60,.2,500,1,1);life::FrameExchange exchange(96,60);
  std::array<std::vector<uint32_t>,4> known;std::array<std::vector<uint16_t>,4> changes;for(size_t layer=0;layer<4;++layer)simulation.changedLayerTilesInto(layer,known[layer],changes[layer]);simulation.paintResource(0,0,life::ResourceType::Plant,100);for(size_t layer=0;layer<4;++layer)simulation.changedLayerTilesInto(layer,known[layer],changes[layer]);require(changes[0].empty()&&changes[2].empty()&&changes[3].empty()&&changes[1].size()==1,"Resource write invalidated unrelated layers");
  simulation.paintTerrain(0,0,life::TerrainType::Plains);for(size_t layer=0;layer<4;++layer)simulation.changedLayerTilesInto(layer,known[layer],changes[layer]);require(changes[3].size()==1,"Terrain write did not invalidate static layer");
  require(exchange.publish(simulation,{},true,42),"Initial publication failed");auto first=exchange.acquire();require(bool(first),"No initial frame");require(first.frame().commandRevision==42,"Published command acknowledgment was lost");const auto held=first.frame().cells;
  for(int i=0;i<12;++i){simulation.step();require(exchange.publish(simulation,{}),"Producer blocked behind held frame");}
  require(first.frame().cells==held,"Producer changed a leased frame");auto latest=exchange.acquire();require(bool(latest)&&latest.frame().tick==12&&latest.frame().cells==simulation.cells&&latest.frame().owners==simulation.owners,"Latest completed state was not coherent");
  exchange.invalidate();require(!exchange.acquire(),"Old-world frame survived invalidation");
  life::NativeSimulation replacement(96,60,.2,500,9,7);require(exchange.publish(replacement,{}),"New-world publication failed");auto replaced=exchange.acquire();require(bool(replaced)&&replaced.frame().world.terrain==replacement.world.terrain&&replaced.frame().world.resources==replacement.world.resources,"Epoch reset reused old-world layers");
  {life::FrameExchange ordered(96,60);for(int i=0;i<3;++i){simulation.step();ordered.publish(simulation,{});}const int end=simulation.tickCount();for(int i=0;i<3;++i){auto frame=ordered.acquireBuffered();require(bool(frame)&&frame.frame().tick==end-2+i,"Buffered presentation skipped or reordered completed ticks");}require(!ordered.acquireBuffered(),"Buffered presentation repeated a consumed tick");for(int i=0;i<3;++i){simulation.step();ordered.publish(simulation,{});} {auto frame=ordered.acquire();require(bool(frame)&&frame.frame().tick==simulation.tickCount(),"Latest-frame mode failed");}require(!ordered.acquireBuffered(),"Switching presentation mode replayed stale ticks");ordered.invalidate();simulation.step();ordered.publish(simulation,{});auto reset=ordered.acquireBuffered();require(bool(reset)&&reset.frame().tick==simulation.tickCount(),"Buffered reset retained stale frames");}
  {life::FrameExchange selective(96,60);for(int i=0;i<4;++i){simulation.step();selective.publish(simulation,{},false);auto frame=selective.acquire();require(bool(frame)&&!frame.frame().ownersCurrent&&frame.frame().cells==simulation.cells,"Ownerless frame lost body data");}selective.publish(simulation,{},true);auto frame=selective.acquire();require(bool(frame)&&frame.frame().ownersCurrent&&frame.frame().owners==simulation.owners,"Ownership request did not hydrate the complete owner layer");}
  life::FrameExchange threaded(96,60);std::atomic_bool finished=false;std::atomic_int failures=0;int consumed=0,lastTick=-1;life::CompletedFrame rendered;
  std::jthread producer([&]{for(int i=0;i<1000;++i){simulation.step();threaded.publish(simulation,{});}finished=true;});
  do{auto frame=threaded.acquire();if(!frame){std::this_thread::yield();continue;}auto& value=frame.frame();if(value.tick<lastTick)++failures;lastTick=value.tick;for(size_t index=0;index<value.cells.size();++index)if((value.cells[index]==0)!=(value.owners[index]<0))++failures;
    std::swap(rendered,value);++consumed; // Recycle all layers, their versions and their epoch together.
  }while(!finished.load());producer.join();require(consumed>0&&failures==0,"Concurrent frame exchange corrupted data");
  std::cout<<"Three-buffer publication, held-frame protection, epoch reset and concurrent coherence passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
