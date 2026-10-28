#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace life {
enum class EvolutionEventType : uint8_t { Season, PopulationPeak, Crash, Recovery, ResourceShortage, AtmosphericStress, Speciation, Extinction, Regeneration, CarrionAccumulation, NutrientCollapse, NutrientRecovery, DecomposerEmergence, DecomposerExtinction, HabitatShift };

struct GenomeDescriptor {
  uint64_t bodyHash{},neuralSignature{},brainGenomeId{};
  std::array<uint8_t,13> cellCounts{};
  uint8_t dietMask{},perceptionRadius{};
  uint16_t senseChannels{};
  uint16_t moveRange{},birthDistance{},oxygenTolerance{},stressRecovery{},frailty{},mutability{},neuralMutability{};
  bool operator==(const GenomeDescriptor&) const=default;
};

// Reusable open-addressed query cache. Representatives are immutable during a
// scan; newly created species extend the search instead of invalidating results.
class SpeciesDistanceCache {
public:
  struct Entry {GenomeDescriptor key;uint64_t hash{};uint32_t epoch{};size_t checked{},best{SIZE_MAX};double distance{2};uint32_t jobEpoch{};};
private:
  std::vector<Entry> entries_;uint32_t epoch_{};size_t used_{};
public:
  void invalidate(){used_=0;if(++epoch_==0){for(auto& entry:entries_)entry.epoch=0;epoch_=1;}}
  void begin(size_t organisms,bool reuse=false){size_t count=16;while(count<organisms*4)count*=2;if(entries_.size()<count){entries_.assign(count,{});epoch_=1;used_=0;}else if(!reuse||used_+organisms>=entries_.size()*3/4)invalidate();}
  Entry& query(GenomeDescriptor key){
    key.brainGenomeId=key.mutability=key.neuralMutability=0;uint64_t hash=key.bodyHash^key.neuralSignature;
    const auto mix=[&](uint64_t value){hash^=value+0x9e3779b97f4a7c15ULL+(hash<<6)+(hash>>2);};
    for(auto count:key.cellCounts)mix(count);mix(key.dietMask);mix(key.perceptionRadius);mix(key.senseChannels);mix(key.moveRange);mix(key.birthDistance);mix(key.oxygenTolerance);mix(key.stressRecovery);mix(key.frailty);
    const size_t start=size_t(hash)&(entries_.size()-1);for(size_t index=start;;index=(index+1)&(entries_.size()-1)){auto& entry=entries_[index];if(entry.epoch!=epoch_){++used_;entry={key,hash,epoch_,0,SIZE_MAX,2};return entry;}if(entry.hash==hash&&entry.key==key)return entry;}
  }
};

struct SpeciesSummary {
  int id{},parentId{-1},representativeId{-1},population{},peakPopulation{},firstTick{},lastTick{},missingScans{},descendantSpecies{};
  uint8_t dominantTerrain{},dietMask{};
  bool extinct{};
  std::string extinctionCause;
  std::array<uint8_t,25> recentDeathCauses{};
  uint8_t recentDeathCount{},recentDeathCursor{};
  GenomeDescriptor representative;
  std::array<int,3> habitatPopulation{};
  float averageElevation{};
  uint8_t dominantHabitat{};
};

struct EcologySample {
  int cavePopulation{},bridgePopulation{},rainfall{};
  float averageElevation{},averageMoisture{};
  int tick{},population{},births{},plantEaters{},scavengers{},mineralEaters{},decomposers{},predators{},species{},criticalRespiratory{};
  std::array<int,7> deaths{};
  uint32_t plant{},carrion{},mineral{},soilNutrients{},nutrientPoorTiles{},carrionProcessed{},plantProduced{};
  float averageEnergy{},averageBodySize{},averagePerceptionCost{},averageRespiratoryStress{},averageOxygenTolerance{},averageStressRecovery{},averageFrailty{},oxygen{},carbonDioxide{},temperature{},fertility{};
  uint64_t nnueEvaluations{},memoryEstimate{};
};

struct EvolutionEvent {
  EvolutionEventType type{};
  int tick{},speciesId{-1};
  float value{};
  std::string label;
};

struct AtlasSnapshot {
  uint64_t revision{};
  int selectedSpecies{-1};
  std::vector<EcologySample> timeline;
  std::vector<SpeciesSummary> species;
  std::vector<EvolutionEvent> events;
};

double genomeDistance(const GenomeDescriptor& left,const GenomeDescriptor& right);
const char* evolutionEventName(EvolutionEventType type);
}
