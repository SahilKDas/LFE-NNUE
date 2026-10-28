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
  std::vector<uint8_t> legacyElevation,legacyHabitat;std::vector<uint16_t> legacyMoisture;
  if(!a.reading){legacyElevation.resize(cells.size());legacyHabitat.resize(cells.size());legacyMoisture.resize(cells.size());}
  a.fields(world.terrain,world.resources,world.resourceAmount,world.nutrients,legacyElevation,legacyHabitat,legacyMoisture,cells,owners,activeSlots_,regionAwake_,regionNutrientTotals_,climateTemperature_,climateFertility_,climateGradient_,climateBand_,timeline_);
  uint64_t count=species_.size();a(count);if(count>100000)throw std::runtime_error("Invalid species count");if(a.reading)species_.resize(count);
  for(auto& s:species_)a.fields(s.id,s.parentId,s.representativeId,s.population,s.peakPopulation,s.firstTick,s.lastTick,s.missingScans,s.descendantSpecies,s.dominantTerrain,s.dietMask,s.extinct,s.extinctionCause,s.recentDeathCauses,s.recentDeathCount,s.recentDeathCursor,s.representative,s.habitatPopulation,s.averageElevation,s.dominantHabitat);
  count=evolutionEvents_.size();a(count);if(count>100000)throw std::runtime_error("Invalid event count");if(a.reading)evolutionEvents_.resize(count);for(auto& e:evolutionEvents_)a.fields(e.type,e.tick,e.speciesId,e.value,e.label);
  std::vector<std::shared_ptr<Nnue>> brains;std::unordered_map<const Nnue*,uint64_t> brainIndices;
  if(!a.reading)for(auto& o:organisms_)if(o.brain&&!brainIndices.contains(o.brain.get())){brainIndices[o.brain.get()]=brains.size();brains.push_back(o.brain);}
  count=brains.size();a(count);if(count>100000)throw std::runtime_error("Invalid brain count");if(a.reading)brains.resize(count);
  Mulberry32 temporary(0);for(auto& brain:brains){if(a.reading)brain=std::make_shared<Nnue>(temporary);brain->materialize();a.fields(brain->inputWeights,brain->hiddenBias,brain->accumulator,brain->outputWeights,brain->outputBias,brain->output,brain->active,brain->signatureCache_);if(brain->inputWeights.size()!=InputSize)throw std::runtime_error("Incompatible neural topology");}
  count=organisms_.size();a(count);if(count>500000)throw std::runtime_error("Invalid organism count");if(a.reading)organisms_.resize(count);
  for(auto& o:organisms_){a.fields(o.id,o.parentId,o.lineage->generation,o.lineage->birthTick,o.lineage->deathTick,o.x,o.y,o.cells,o.energy,o.minerals,o.lifetime,o.damage,o.mutability,o.neuralMutability,o.birthDistance,o.moveRange,o.moveCount,o.direction,o.rotation,o.living,o.isProducer,o.isMover,o.isConsumer,o.isAttacker,o.thermalStress,o.hypoxiaStress,o.oxygenTolerance,o.stressRecovery,o.frailty,o.respiratoryRisk,o.deathCauseCode,o.lineage->deathCause,o.perceptionRadius,o.senseChannels,o.neuralCost,o.carrionConsumed,o.nutrientsReleased,o.decompositionEnergy,o.lineage->totalDescendants,o.lineage->mutations,o.lastFeatures,o.lastOutputs,o.lastAction,o.activeIndex,o.brainGenomeId,o.speciesId);uint64_t index=o.brain?brainIndices[o.brain.get()]:UINT64_MAX;a(index);if(a.reading){if(index!=UINT64_MAX&&index>=brains.size())throw std::runtime_error("Invalid neural reference");o.brain=index==UINT64_MAX?nullptr:brains[index];}}
  if(a.reading){const size_t area=size_t(width_)*height_;if(world.terrain.size()!=area||world.resources.size()!=area||world.resourceAmount.size()!=area||world.nutrients.size()!=area||legacyElevation.size()!=area||legacyHabitat.size()!=area||legacyMoisture.size()!=area||cells.size()!=area||owners.size()!=area)throw std::runtime_error("Invalid world layers");
    // Caves and bridges from older archives become ordinary traversable land.
    for(size_t i=0;i<area;++i){if(world.terrain[i]>uint8_t(TerrainType::Mountain)||legacyHabitat[i]>2)throw std::runtime_error("Invalid legacy terrain");if(legacyHabitat[i]&& !passable(world,int(i)))world.terrain[i]=uint8_t(TerrainType::Plains);}
    savedView.weather=false;savedView.habitatHistory=false;if(savedView.overlay>6)savedView.overlay=0;if(savedView.tool>9)savedView.tool=0;
    for(auto& sample:timeline_){sample.cavePopulation=sample.bridgePopulation=sample.rainfall=0;sample.averageElevation=sample.averageMoisture=0;}
    for(auto& species:species_){species.habitatPopulation.fill(0);species.averageElevation=0;species.dominantHabitat=0;}
    std::erase_if(evolutionEvents_,[](const auto& event){return event.type==EvolutionEventType::HabitatShift;});
slotById_.clear();for(size_t i=0;i<organisms_.size();++i)slotById_[organisms_[i].id]=i;for(auto slot:activeSlots_)if(slot!=SIZE_MAX&&slot>=organisms_.size())throw std::runtime_error("Invalid active slot");classificationBuffer_.clear();speciesSlotById_.clear();for(size_t index=0;index<species_.size();++index)speciesSlotById_[species_[index].id]=index;classificationCache_.invalidate();respiratoryCandidates_.clear();rebuildRegionIndex();rebuildOwnerSlots();rebuildAtmosphereCounts();markAllDirty();}
}
void NativeSimulation::save(const std::filesystem::path& path,uint32_t archiveVersion){
  if(archiveVersion!=1&&archiveVersion!=2)throw std::runtime_error("Unsupported save version");
  static_assert(sizeof(SavedView)==52,"Version 1 camera layout must remain stable");
  auto temporary=path;temporary+=".tmp";try{std::fstream file(temporary,std::ios::binary|std::ios::out|std::ios::trunc);StateArchive a{file,false};uint64_t magic=0x3145564153464cULL;uint32_t version=archiveVersion,inputs=InputSize;a.fields(magic,version,inputs);archiveState(a);if(version==2){a.fields(followPreferences.target,followPreferences.enabled,followPreferences.family,followPreferences.compact,followPreferences.wrapped);uint64_t count=organisms_.size();a(count);for(auto& organism:organisms_)a.fields(organism.id,organism.lineage->directOffspring,organism.lineage->offspringCountComplete);}const uint64_t checksum=a.checksum;file.write(reinterpret_cast<const char*>(&checksum),sizeof(checksum));file.flush();if(!file)throw std::runtime_error("Unable to save simulation");file.close();auto backup=path;backup+=".bak";if(std::filesystem::exists(path))backupSaveFile(path,backup);replaceSaveFile(temporary,path);}catch(...){std::error_code error;std::filesystem::remove(temporary,error);throw;}
}
void NativeSimulation::load(const std::filesystem::path& path){
  verifySaveChecksum(path);std::fstream file(path,std::ios::binary|std::ios::in);StateArchive a{file,true};uint64_t magic{};uint32_t version{},inputs{};a.fields(magic,version,inputs);if(magic!=0x3145564153464cULL||(version!=1&&version!=2)||inputs!=InputSize)throw std::runtime_error("Unsupported simulation save");
  NativeSimulation restored(width_,height_,foodChance_,lifespan_,worldSeed_,0,lineageLimit_);restored.archiveState(a);if(version==2){auto& preferences=restored.followPreferences;a.fields(preferences.target,preferences.enabled,preferences.family,preferences.compact,preferences.wrapped);uint64_t count{};a(count);if(count!=restored.organisms_.size())throw std::runtime_error("Invalid offspring counters");for(auto& organism:restored.organisms_){int id{};a.fields(id,organism.lineage->directOffspring,organism.lineage->offspringCountComplete);if(id!=organism.id||organism.lineage->directOffspring<0)throw std::runtime_error("Invalid offspring record");}}else{for(const auto& organism:restored.organisms_)if(auto* parent=restored.organismById(organism.parentId))++parent->lineage->directOffspring;for(auto& organism:restored.organisms_)organism.lineage->offspringCountComplete=false;}uint64_t checksum{};file.read(reinterpret_cast<char*>(&checksum),sizeof(checksum));if(!file||checksum!=a.checksum||file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Corrupt simulation save");if(restored.width_!=width_||restored.height_!=height_)throw std::runtime_error("Save world dimensions differ");auto& preferences=restored.followPreferences;const auto* target=restored.organismById(preferences.target);if(!target)preferences={};else if(!target->living)preferences.enabled=false;restored.watch(target?target->id:restored.savedView.selectedOrganism);*this=std::move(restored);
}
}
