#include "archive.hpp"
#include <unordered_map>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
namespace life {
void verifySaveChecksum(const std::filesystem::path& path){
  std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Unable to open saved world");const auto size=std::filesystem::file_size(path);if(size<24)throw std::runtime_error("Incomplete simulation save");uint64_t hash=1469598103934665603ULL,left=size-sizeof(uint64_t);std::array<char,16384> block{};while(left){const size_t count=size_t(std::min<uint64_t>(left,block.size()));input.read(block.data(),count);if(!input)throw std::runtime_error("Incomplete simulation save");for(size_t i=0;i<count;++i){hash^=uint8_t(block[i]);hash*=1099511628211ULL;}left-=count;}uint64_t expected{};input.read(reinterpret_cast<char*>(&expected),sizeof(expected));if(!input||expected!=hash)throw std::runtime_error("Saved world checksum mismatch");
}
void replaceSaveFile(const std::filesystem::path& source,const std::filesystem::path& target){
#ifdef _WIN32
  if(!MoveFileExW(source.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Unable to replace saved world");
#else
  std::filesystem::rename(source,target);
#endif
}
void backupSaveFile(const std::filesystem::path& source,const std::filesystem::path& target){
#ifdef _WIN32
  if(!CopyFileW(source.c_str(),target.c_str(),FALSE))throw std::runtime_error("Unable to back up saved world: "+std::to_string(GetLastError()));
#else
  std::filesystem::copy_file(source,target,std::filesystem::copy_options::overwrite_existing);
#endif
}
void NativeSimulation::archiveState(StateArchive& a){
  const int expectedWidth=width_,expectedHeight=height_;
  a(savedView);
  a.fields(width_,height_,worldSeed_,nextId_,ticks_,resets_,record_,largest_,selectedId_,deadCount_,nextBrainGenomeId_,foodChance_,lifespan_,lineageLimit_,nnueEvaluations_,atmosphericOxygen_,carbonDioxide_,reproductionEnabled_,mortalityEnabled_,populationTarget_,births_,deathsByCause_,sampledDeaths_,sampledBirths_,nextSpeciesId_,selectedSpeciesId_,lastSeason_,lastPopulationPeak_,crashBaseline_,crashMinimum_,crashActive_,atmosphereEventActive_,carrionEventActive_,nutrientCollapseActive_,lastDecomposerPopulation_,resourceShortageRuns_,resourceShortageActive_,atlasRevision_,respiratoryDeaths_,lastRespiratoryDeathTick_,carrionProcessed_,sampledCarrionProcessed_,plantProduced_,sampledPlantProduced_);
  if(width_!=expectedWidth||height_!=expectedHeight||width_<1||height_<1||size_t(width_)*height_>8'000'000||ticks_<0)throw std::runtime_error("Invalid simulation dimensions or clock");
  uint32_t rng=random_.checkpoint();a(rng);if(a.reading)random_.restore(rng);
  a.fields(world.terrain,world.resources,world.resourceAmount,world.nutrients,world.elevation,world.habitat,moisture,cells,owners,activeSlots_,regionAwake_,regionNutrientTotals_,climateTemperature_,climateFertility_,climateGradient_,climateBand_,timeline_);
  uint64_t count=species_.size();a(count);if(count>100000)throw std::runtime_error("Invalid species count");if(a.reading)species_.resize(count);
  for(auto& s:species_)a.fields(s.id,s.parentId,s.representativeId,s.population,s.peakPopulation,s.firstTick,s.lastTick,s.missingScans,s.descendantSpecies,s.dominantTerrain,s.dietMask,s.extinct,s.extinctionCause,s.recentDeathCauses,s.recentDeathCount,s.recentDeathCursor,s.representative,s.habitatPopulation,s.averageElevation,s.dominantHabitat);
  count=evolutionEvents_.size();a(count);if(count>100000)throw std::runtime_error("Invalid event count");if(a.reading)evolutionEvents_.resize(count);for(auto& e:evolutionEvents_)a.fields(e.type,e.tick,e.speciesId,e.value,e.label);
  std::vector<std::shared_ptr<Nnue>> brains;std::unordered_map<const Nnue*,uint64_t> brainIndices;
  if(!a.reading)for(auto& o:organisms_)if(o.brain&&!brainIndices.contains(o.brain.get())){brainIndices[o.brain.get()]=brains.size();brains.push_back(o.brain);}
  count=brains.size();a(count);if(count>100000)throw std::runtime_error("Invalid brain count");if(a.reading)brains.resize(count);
  Mulberry32 temporary(0);for(auto& brain:brains){if(a.reading)brain=std::make_shared<Nnue>(temporary);a.fields(brain->inputWeights,brain->hiddenBias,brain->accumulator,brain->outputWeights,brain->outputBias,brain->output,brain->active,brain->signatureCache_);if(brain->inputWeights.size()!=InputSize)throw std::runtime_error("Incompatible neural topology");}
  count=organisms_.size();a(count);if(count>500000)throw std::runtime_error("Invalid organism count");if(a.reading)organisms_.resize(count);
  for(auto& o:organisms_){a.fields(o.id,o.parentId,o.generation,o.birthTick,o.deathTick,o.x,o.y,o.cells,o.energy,o.minerals,o.lifetime,o.damage,o.mutability,o.neuralMutability,o.birthDistance,o.moveRange,o.moveCount,o.direction,o.rotation,o.living,o.isProducer,o.isMover,o.isConsumer,o.isAttacker,o.thermalStress,o.hypoxiaStress,o.oxygenTolerance,o.stressRecovery,o.frailty,o.respiratoryRisk,o.deathCauseCode,o.deathCause,o.perceptionRadius,o.senseChannels,o.neuralCost,o.carrionConsumed,o.nutrientsReleased,o.decompositionEnergy,o.totalDescendants,o.mutations,o.lastFeatures,o.lastOutputs,o.lastAction,o.activeIndex,o.brainGenomeId,o.speciesId);uint64_t index=o.brain?brainIndices[o.brain.get()]:UINT64_MAX;a(index);if(a.reading){if(index!=UINT64_MAX&&index>=brains.size())throw std::runtime_error("Invalid neural reference");o.brain=index==UINT64_MAX?nullptr:brains[index];}}
  if(a.reading){const size_t area=size_t(width_)*height_;if(world.terrain.size()!=area||world.resources.size()!=area||world.resourceAmount.size()!=area||world.nutrients.size()!=area||world.elevation.size()!=area||world.habitat.size()!=area||moisture.size()!=area||cells.size()!=area||owners.size()!=area)throw std::runtime_error("Invalid world layers");slotById_.clear();for(size_t i=0;i<organisms_.size();++i)slotById_[organisms_[i].id]=i;for(auto slot:activeSlots_)if(slot!=SIZE_MAX&&slot>=organisms_.size())throw std::runtime_error("Invalid active slot");classificationBuffer_.clear();respiratoryCandidates_.clear();rebuildRegionIndex();markAllDirty();}
}
void NativeSimulation::save(const std::filesystem::path& path,uint32_t archiveVersion){
  if(archiveVersion!=1&&archiveVersion!=2)throw std::runtime_error("Unsupported save version");
  static_assert(sizeof(SavedView)==52,"Version 1 camera layout must remain stable");
  auto temporary=path;temporary+=".tmp";try{std::fstream file(temporary,std::ios::binary|std::ios::out|std::ios::trunc);StateArchive a{file,false};uint64_t magic=0x3145564153464cULL;uint32_t version=archiveVersion,inputs=InputSize;a.fields(magic,version,inputs);archiveState(a);if(version==2){a.fields(followPreferences.target,followPreferences.enabled,followPreferences.family,followPreferences.compact,followPreferences.wrapped);uint64_t count=organisms_.size();a(count);for(auto& organism:organisms_)a.fields(organism.id,organism.directOffspring,organism.offspringCountComplete);}const uint64_t checksum=a.checksum;file.write(reinterpret_cast<const char*>(&checksum),sizeof(checksum));file.flush();if(!file)throw std::runtime_error("Unable to save simulation");file.close();auto backup=path;backup+=".bak";if(std::filesystem::exists(path))backupSaveFile(path,backup);replaceSaveFile(temporary,path);}catch(...){std::error_code error;std::filesystem::remove(temporary,error);throw;}
}
void NativeSimulation::load(const std::filesystem::path& path){
  verifySaveChecksum(path);std::fstream file(path,std::ios::binary|std::ios::in);StateArchive a{file,true};uint64_t magic{};uint32_t version{},inputs{};a.fields(magic,version,inputs);if(magic!=0x3145564153464cULL||(version!=1&&version!=2)||inputs!=InputSize)throw std::runtime_error("Unsupported simulation save");
  NativeSimulation restored(width_,height_,foodChance_,lifespan_,worldSeed_,0,lineageLimit_);restored.archiveState(a);if(version==2){auto& preferences=restored.followPreferences;a.fields(preferences.target,preferences.enabled,preferences.family,preferences.compact,preferences.wrapped);uint64_t count{};a(count);if(count!=restored.organisms_.size())throw std::runtime_error("Invalid offspring counters");for(auto& organism:restored.organisms_){int id{};a.fields(id,organism.directOffspring,organism.offspringCountComplete);if(id!=organism.id||organism.directOffspring<0)throw std::runtime_error("Invalid offspring record");}}else{for(const auto& organism:restored.organisms_)if(auto* parent=restored.organismById(organism.parentId))++parent->directOffspring;for(auto& organism:restored.organisms_)organism.offspringCountComplete=false;}uint64_t checksum{};file.read(reinterpret_cast<char*>(&checksum),sizeof(checksum));if(!file||checksum!=a.checksum||file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Corrupt simulation save");if(restored.width_!=width_||restored.height_!=height_)throw std::runtime_error("Save world dimensions differ");auto& preferences=restored.followPreferences;const auto* target=restored.organismById(preferences.target);if(!target)preferences={};else if(!target->living)preferences.enabled=false;restored.watch(target?target->id:restored.savedView.selectedOrganism);*this=std::move(restored);
}
int NativeSimulation::rainfall() const {return 12+int((.5+.5*std::sin(cyclePhase(ticks_)*std::numbers::pi*2))*40);}
void NativeSimulation::updateDrainage(){
  const int rain=rainfall();
  if(moisture.size()!=world.nutrients.size())moisture.assign(world.nutrients.size(),300);
  drainageNutrientScratch_.assign(world.nutrients.size(),0);drainageWaterScratch_.assign(moisture.size(),0);auto& nutrientDelta=drainageNutrientScratch_;auto& waterDelta=drainageWaterScratch_;
  for(int y=0;y<height_;++y)for(int x=0;x<width_;++x){const int i=y*width_+x;const auto terrain=TerrainType(world.terrain[i]);int wet=std::clamp(int(moisture[i])+rain-(terrain==TerrainType::Desert?35:18),0,1000);if(terrain==TerrainType::Water)wet=1000;moisture[i]=uint16_t(wet);if(HabitatType(world.habitat[i])==HabitatType::Cave)continue;int target=i;for(int j:{wrappedIndex(x-1,y),wrappedIndex(x+1,y),wrappedIndex(x,y-1),wrappedIndex(x,y+1)})if(world.elevation[j]<world.elevation[target])target=j;if(target!=i&&wet>350){const int flow=std::min(32,(wet-350)/8);waterDelta[i]-=flow;waterDelta[target]+=flow;const int nutrient=std::min(int(world.nutrients[i]),flow/4);nutrientDelta[i]-=nutrient;if(TerrainType(world.terrain[target])!=TerrainType::Water&&TerrainType(world.terrain[target])!=TerrainType::Mountain)nutrientDelta[target]+=nutrient;}}
  for(size_t i=0;i<moisture.size();++i){moisture[i]=uint16_t(std::clamp(int(moisture[i])+waterDelta[i],0,1000));const int capacity=TerrainType(world.terrain[i])==TerrainType::Fertile?2400:TerrainType(world.terrain[i])==TerrainType::Plains?1600:TerrainType(world.terrain[i])==TerrainType::Desert?500:0;world.nutrients[i]=uint16_t(std::clamp(int(world.nutrients[i])+nutrientDelta[i],0,capacity));if(nutrientDelta[i])markDirtyIndex(int(i));}
  std::fill(regionNutrientTotals_.begin(),regionNutrientTotals_.end(),0);for(int y=0;y<height_;++y)for(int x=0;x<width_;++x)regionNutrientTotals_[(y/RegionSize)*regionColumns_+x/RegionSize]+=world.nutrients[y*width_+x];
}
}
