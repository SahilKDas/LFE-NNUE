#pragma once
#include "simulation.hpp"
#include <atomic>
#include <utility>

namespace life {
struct FrameDiagnostics {
  NativeMetrics metrics;std::optional<OrganismInspection> inspection;WatchedSnapshot watched;
  std::unordered_map<int,FamilyRole> family;
  std::vector<std::pair<int,uint16_t>> familyTiles;
  int highlightRoot{-1};size_t history{};
  uint64_t context{};
};
struct CompletedFrame {
  WorldLayers world;std::vector<uint8_t> cells;std::vector<int32_t> owners;std::vector<uint32_t> versions;
  std::array<std::vector<uint32_t>,4> layerVersions;
  WatchedPose pose;int tick{};uint64_t epoch{};bool ownersCurrent{};std::shared_ptr<const FrameDiagnostics> diagnostics;
};

// One producer under the simulation write lock, one renderer consumer. Capacity is fixed.
class FrameExchange {
  enum : int { Free,Writing,Ready,Reading };
  struct Slot {std::atomic_int state{Free};std::atomic_uint64_t sequence{};CompletedFrame frame;std::vector<uint16_t> dirty;std::array<std::vector<uint16_t>,4> layerDirty;};
  std::array<Slot,3> slots_;std::atomic_int latest_{-1};std::atomic_uint64_t epoch_{1};
  int width_,height_,cursor_{};uint64_t publishedSequence_{},consumedSequence_{};
public:
  class Lease {
    Slot* slot_{};
  public:
    explicit Lease(Slot* slot=nullptr):slot_(slot){}
    Lease(const Lease&)=delete;Lease& operator=(const Lease&)=delete;
    Lease(Lease&& other) noexcept:slot_(std::exchange(other.slot_,nullptr)){}
    ~Lease(){if(slot_)slot_->state.store(Free,std::memory_order_release);}
    explicit operator bool()const{return slot_!=nullptr;}
    CompletedFrame& frame(){return slot_->frame;}
  };
  FrameExchange(int width,int height):width_(width),height_(height){}
  void invalidate(){epoch_.fetch_add(1,std::memory_order_acq_rel);latest_.store(-1,std::memory_order_release);}
  uint64_t epoch() const{return epoch_.load(std::memory_order_acquire);}
  Lease acquire(){const int index=latest_.load(std::memory_order_acquire);if(index<0)return Lease{};auto& slot=slots_[index];int expected=Ready;if(!slot.state.compare_exchange_strong(expected,Reading,std::memory_order_acq_rel))return Lease{};if(slot.frame.epoch!=epoch()){slot.state.store(Free,std::memory_order_release);return Lease{};}consumedSequence_=slot.sequence.load(std::memory_order_acquire);return Lease(&slot);}
  // At equal simulation/render rates, retain a short presentation cushion.
  // Consume completed ticks in publication order instead of skipping to newest.
  // Capacity remains three; slower render rates still use acquire() above.
  Lease acquireBuffered(){
    for(int attempt=0;attempt<3;++attempt){int chosen=-1;uint64_t oldest=UINT64_MAX;
      for(int index=0;index<3;++index){auto& slot=slots_[index];if(slot.state.load(std::memory_order_acquire)!=Ready)continue;const auto sequence=slot.sequence.load(std::memory_order_acquire);if(sequence>consumedSequence_&&sequence<oldest){oldest=sequence;chosen=index;}}
      if(chosen<0)return Lease{};auto& slot=slots_[chosen];int expected=Ready;if(!slot.state.compare_exchange_strong(expected,Reading,std::memory_order_acq_rel))continue;
      const auto sequence=slot.sequence.load(std::memory_order_acquire);if(sequence!=oldest){slot.state.store(Ready,std::memory_order_release);continue;}if(slot.frame.epoch!=epoch()||sequence<=consumedSequence_){slot.state.store(Free,std::memory_order_release);continue;}consumedSequence_=sequence;return Lease(&slot);
    }return Lease{};
  }
  bool publish(const NativeSimulation& simulation,std::shared_ptr<const FrameDiagnostics> diagnostics,bool includeOwners=true){
    const int latest=latest_.load(std::memory_order_acquire);int chosen=-1;
    for(int attempt=0;attempt<3;++attempt){const int index=(cursor_+attempt)%3;int expected=Free;if(slots_[index].state.compare_exchange_strong(expected,Writing,std::memory_order_acq_rel)){chosen=index;break;}}
    if(chosen<0)for(int attempt=0;attempt<3;++attempt){const int index=(cursor_+attempt)%3;if(index==latest)continue;int expected=Ready;if(slots_[index].state.compare_exchange_strong(expected,Writing,std::memory_order_acq_rel)){chosen=index;break;}}
    if(chosen<0)return false;auto& slot=slots_[chosen];auto& frame=slot.frame;
    try{
      const uint64_t generation=epoch();const bool initial=frame.cells.size()!=simulation.cells.size()||frame.epoch!=generation;if(initial){frame.versions.clear();for(auto& known:frame.layerVersions)known.clear();}
      const bool hydrateOwners=includeOwners&&(initial||!frame.ownersCurrent||frame.owners.size()!=simulation.owners.size());
      if(hydrateOwners)frame.owners=simulation.owners;
      else if(frame.owners.size()!=simulation.owners.size())frame.owners.resize(simulation.owners.size(),-1);
      frame.ownersCurrent=includeOwners;
      simulation.changedTilesInto(frame.versions,slot.dirty);constexpr int tileSize=NativeSimulation::dirtyTileSize();const int columns=(width_+tileSize-1)/tileSize,total=columns*((height_+tileSize-1)/tileSize);
      // Resource amounts are diagnostic state, not raster input; omit their full-world copy.
      for(size_t layer=0;layer<4;++layer){
        simulation.changedLayerTilesInto(layer,frame.layerVersions[layer],slot.layerDirty[layer]);const auto& dirty=slot.layerDirty[layer];if(dirty.empty())continue;
        const bool all=initial||int(dirty.size())>total*3/4;
        if(all){if(layer==0){frame.cells=simulation.cells;if(includeOwners&&!hydrateOwners)frame.owners=simulation.owners;}else if(layer==1)frame.world.resources=simulation.world.resources;else if(layer==2)frame.world.nutrients=simulation.world.nutrients;else{frame.world.terrain=simulation.world.terrain;frame.world.elevation=simulation.world.elevation;frame.world.habitat=simulation.world.habitat;}continue;}
        // Merge horizontal tile runs: a dirty band becomes a few long copies, not thousands of tiny copies.
        for(size_t offset=0;offset<dirty.size();){const int first=dirty[offset],row=first/columns;int last=first;++offset;while(offset<dirty.size()&&dirty[offset]==last+1&&dirty[offset]/columns==row)last=dirty[offset++];
          const int x=(first%columns)*tileSize,right=std::min(width_,((last%columns)+1)*tileSize),top=row*tileSize;
          for(int y=top;y<std::min(top+tileSize,height_);++y){const int begin=y*width_+x,count=right-x;const auto copy=[&](const auto& source,auto& destination){std::copy_n(source.begin()+begin,count,destination.begin()+begin);};if(layer==0){copy(simulation.cells,frame.cells);if(includeOwners&&!hydrateOwners)copy(simulation.owners,frame.owners);}else if(layer==1)copy(simulation.world.resources,frame.world.resources);else if(layer==2)copy(simulation.world.nutrients,frame.world.nutrients);else{copy(simulation.world.terrain,frame.world.terrain);copy(simulation.world.elevation,frame.world.elevation);copy(simulation.world.habitat,frame.world.habitat);}}
        }
      }
      frame.tick=simulation.tickCount();frame.pose=simulation.watchedPose();frame.epoch=generation;frame.diagnostics=std::move(diagnostics);slot.sequence.store(++publishedSequence_,std::memory_order_release);slot.state.store(Ready,std::memory_order_release);latest_.store(chosen,std::memory_order_release);cursor_=(chosen+1)%3;return true;
    }catch(...){slot.state.store(Free,std::memory_order_release);throw;}
  }
};
}
