#include "simulation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>
#include <chrono>
#include "persistence.inc"
#include "watch.inc"

namespace life {
double genomeDistanceBounded(const GenomeDescriptor&,const GenomeDescriptor&,double);
namespace {
constexpr std::array<std::pair<int,int>,4> directions{{{0,-1},{0,1},{-1,0},{1,0}}};
const char* cellName(CellType type){constexpr const char* names[]={"empty","food","wall","mouth","producer","mover","killer","armor","plant mouth","scavenger mouth","mineral mouth","inert tissue","decomposer mouth"};const int index=int(type);return index>=0&&index<int(std::size(names))?names[index]:"unknown";}
}
const char* deathCauseName(DeathCause cause){constexpr const char* names[]={"","Starvation","Combat","Old age","Thermal stress","Terrain","Respiratory failure"};const auto index=size_t(cause);return index<std::size(names)?names[index]:"Unknown";}

NativeSimulation::NativeSimulation(int width, int height, double foodChance, int lifespan, uint32_t worldSeed, uint32_t randomSeed, int lineageLimit)
  : width_(width), height_(height), worldSeed_(worldSeed), random_(randomSeed), foodChance_(foodChance), lifespan_(lifespan),lineageLimit_(std::max(0,lineageLimit)),
    climateTemperature_(height),climateFertility_(height),climateGradient_(height),climateBand_(height),world(generateWorld(width, height, worldSeed)), cells(width * height), owners(width * height, -1) {regionColumns_=(width_+RegionSize-1)/RegionSize;regionRows_=(height_+RegionSize-1)/RegionSize;dirtyColumns_=(width_+DirtyTileSize-1)/DirtyTileSize;dirtyRows_=(height_+DirtyTileSize-1)/DirtyTileSize;regionBatches_.resize(regionColumns_*regionRows_);regionAwake_.resize(regionBatches_.size());regionNutrientTotals_.resize(regionBatches_.size());nutrientScratch_.resize(width_*height_);dirtyTiles_.resize(dirtyColumns_*dirtyRows_,1);dirtyVersions_.resize(dirtyTiles_.size(),1);for(auto& layer:dirtyLayerVersions_)layer.resize(dirtyTiles_.size(),1);organisms_.reserve(100'000);
  const size_t area=owners.size();nutrientResourceDirty_.resize(area);nutrientDecayRows_.resize(height_);soilResourceTileChanges_.resize(dirtyTiles_.size());soilNutrientTileChanges_.resize(dirtyTiles_.size());drainageNutrientScratch_.resize(area);drainageWaterScratch_.resize(area);drainageFlowTargets_.resize(area);drainageFlows_.resize(area);drainageNutrientFlows_.resize(area);
  if(area>=262144){activeSlots_.reserve(100'000);slotById_.reserve(100'000);preparedPhysiology_.reserve(100'000);classificationBuffer_.reserve(100'000);classificationDescriptors_.reserve(100'000);classificationJobs_.reserve(100'000);classificationMatches_.reserve(100'000);classificationCache_.begin(32768);pruneChildren_.reserve(100'000);pruneParents_.reserve(100'000);pruneSlotRemap_.reserve(100'000);pruneProtected_.reserve(100'000);pruneRemoved_.reserve(100'000);neuralMutationScratch_.reserve(InputSize*HiddenSize+HiddenSize+OutputSize*HiddenSize+OutputSize);}
  reset(); }

void NativeSimulation::updateClimateCache(){const double wave=std::sin(cyclePhase(ticks_)*std::numbers::pi*2.0),step=2.0/std::max(1,height_-1);const int direction=std::abs(wave*step)<.002?0:wave>0?-1:1;for(int y=0;y<height_;++y){const double latitude=1.0-2.0*y/std::max(1,height_-1),value=std::clamp(.5+.5*latitude*wave,0.0,1.0);climateTemperature_[y]=float(value);climateFertility_[y]=float(.35+value);climateGradient_[y]=int8_t(direction);climateBand_[y]=uint8_t(std::min(4,int(std::floor(value*5.0))));}}

void NativeSimulation::registerAtmosphere(const Organism& organism,int sign){atmosphereCells_+=int64_t(organism.cells.size())*sign;atmosphereMovers_+=int(organism.isMover)*sign;for(const auto& cell:organism.cells)atmosphereProducers_+=int(cell.type==CellType::Producer)*sign;}
void NativeSimulation::rebuildAtmosphereCounts(){atmosphereCells_=atmosphereMovers_=atmosphereProducers_=0;for(const auto slot:activeSlots_)if(slot!=SIZE_MAX&&slot<organisms_.size()&&organisms_[slot].living)registerAtmosphere(organisms_[slot],1);}
void NativeSimulation::updateAtmosphere(){const double respiration=(double(atmosphereCells_)+double(atmosphereMovers_)*2.0)*3e-9,photosynthesis=double(atmosphereProducers_)*7e-9,oxygenExchange=(.21-atmosphericOxygen_)*.0005,carbonExchange=(.0004-carbonDioxide_)*.0005;atmosphericOxygen_=std::clamp(atmosphericOxygen_+photosynthesis-respiration+oxygenExchange,.06,.25);carbonDioxide_=std::clamp(carbonDioxide_+respiration*.65-photosynthesis*.55+carbonExchange,.0004,.20);}

void NativeSimulation::updateNutrients(){
  std::fill(regionNutrientTotals_.begin(),regionNutrientTotals_.end(),0);nutrientResourceDirty_.assign(world.nutrients.size(),0);nutrientDecayRows_.assign(height_,0);
  const int tick=ticks_,phase=tick/30;const bool recover=tick%600==0;
  const auto prepare=[&](size_t begin,size_t end){for(int y=int(begin);y<int(end);++y){uint32_t decayed=0;const int row=y*width_,north=(y?y-1:height_-1)*width_,south=(y+1<height_?y+1:0)*width_;for(int x=0;x<width_;++x){const int index=row+x;const auto terrain=TerrainType(world.terrain[index]);if(terrain==TerrainType::Water||terrain==TerrainType::Mountain){nutrientScratch_[index]=0;continue;}const int capacity=terrain==TerrainType::Fertile?2400:terrain==TerrainType::Plains?1600:500;int value=world.nutrients[index];const int neighbors=world.nutrients[row+(x?x-1:width_-1)]+world.nutrients[row+(x+1<width_?x+1:0)]+world.nutrients[north+x]+world.nutrients[south+x];value+=(neighbors/4-value)/32;if(terrain==TerrainType::Fertile)value-=value/1200;else value-=value/2400;if(recover)value+=terrain==TerrainType::Desert?1:3;if(ResourceType(world.resources[index])==ResourceType::Carrion&&(index+phase)%16==0&&world.resourceAmount[index]){const int decay=std::min(10,int(world.resourceAmount[index]));world.resourceAmount[index]-=uint16_t(decay);value+=decay;if(!world.resourceAmount[index]){world.resources[index]=uint8_t(ResourceType::None);nutrientResourceDirty_[index]=1;}++decayed;}nutrientScratch_[index]=uint16_t(std::clamp(value,0,capacity));}nutrientDecayRows_[y]=decayed;}};
  if(parallelAnalysis_&&analysisPool_)analysisPool_->run(height_,prepare);else prepare(0,height_);
  for(const auto count:nutrientDecayRows_)carrionProcessed_+=count;
  soilResourceTileChanges_.assign(dirtyTiles_.size(),0);soilNutrientTileChanges_.assign(dirtyTiles_.size(),0);
  const auto apply=[&](size_t begin,size_t end){for(size_t region=begin;region<end;++region){const int tx=int(region%regionColumns_)*RegionSize,ty=int(region/regionColumns_)*RegionSize;uint32_t total=0;for(int y=ty;y<std::min(ty+RegionSize,height_);++y){const int row=y*width_,tileRow=(y/DirtyTileSize)*dirtyColumns_;for(int x=tx;x<std::min(tx+RegionSize,width_);++x){const int index=row+x,tile=tileRow+x/DirtyTileSize;if(nutrientResourceDirty_[index])++soilResourceTileChanges_[tile];if(world.nutrients[index]!=nutrientScratch_[index])++soilNutrientTileChanges_[tile];world.nutrients[index]=nutrientScratch_[index];total+=world.nutrients[index];}}regionNutrientTotals_[region]=total;}};
  if(parallelAnalysis_&&analysisPool_)analysisPool_->run(regionNutrientTotals_.size(),apply);else apply(0,regionNutrientTotals_.size());
  for(size_t tile=0;tile<dirtyTiles_.size();++tile){const auto resources=soilResourceTileChanges_[tile],nutrients=soilNutrientTileChanges_[tile];if(resources||nutrients){dirtyTiles_[tile]=1;dirtyVersions_[tile]+=resources+nutrients;dirtyLayerVersions_[1][tile]+=resources;dirtyLayerVersions_[2][tile]+=nutrients;}}
}

int NativeSimulation::safeIndex(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_ ? y * width_ + x : -1; }
int NativeSimulation::wrapX(int x) const{if(unsigned(x)<unsigned(width_))return x;if(x<0&&x>=-width_)return x+width_;if(x>=width_&&x<width_*2)return x-width_;return (x%width_+width_)%width_;}
int NativeSimulation::wrapY(int y) const{if(unsigned(y)<unsigned(height_))return y;if(y<0&&y>=-height_)return y+height_;if(y>=height_&&y<height_*2)return y-height_;return (y%height_+height_)%height_;}
int NativeSimulation::wrappedIndex(int x,int y) const{return wrapY(y)*width_+wrapX(x);}
void NativeSimulation::markDirtyIndex(int index,uint8_t layers){if(index<0)return;const int tile=width_==WorldWidth?(index/(WorldWidth*DirtyTileSize))*(WorldWidth/DirtyTileSize)+(index/DirtyTileSize)%(WorldWidth/DirtyTileSize):(index/width_/DirtyTileSize)*dirtyColumns_+(index%width_)/DirtyTileSize;dirtyTiles_[tile]=1;++dirtyVersions_[tile];for(size_t layer=0;layer<4;++layer)if(layers&(1u<<layer))++dirtyLayerVersions_[layer][tile];}
void NativeSimulation::markAllDirty(){std::fill(dirtyTiles_.begin(),dirtyTiles_.end(),1);for(auto& version:dirtyVersions_)++version;for(auto& layer:dirtyLayerVersions_)for(auto& version:layer)++version;}
std::vector<uint16_t> NativeSimulation::takeDirtyTiles(){std::vector<uint16_t> result;result.reserve(dirtyTiles_.size()/8);for(size_t index=0;index<dirtyTiles_.size();++index)if(dirtyTiles_[index]){result.push_back(uint16_t(index));dirtyTiles_[index]=0;}return result;}
void NativeSimulation::changedTilesInto(std::vector<uint32_t>& knownVersions,std::vector<uint16_t>& result) const{if(knownVersions.size()!=dirtyVersions_.size())knownVersions.assign(dirtyVersions_.size(),0);result.clear();if(result.capacity()<dirtyVersions_.size())result.reserve(dirtyVersions_.size());for(size_t index=0;index<dirtyVersions_.size();++index)if(knownVersions[index]!=dirtyVersions_[index]){knownVersions[index]=dirtyVersions_[index];result.push_back(uint16_t(index));}}
void NativeSimulation::changedLayerTilesInto(size_t layer,std::vector<uint32_t>& knownVersions,std::vector<uint16_t>& result) const{const auto& versions=dirtyLayerVersions_.at(layer);if(knownVersions.size()!=versions.size())knownVersions.assign(versions.size(),0);result.clear();if(result.capacity()<versions.size())result.reserve(versions.size());for(size_t index=0;index<versions.size();++index)if(knownVersions[index]!=versions[index]){knownVersions[index]=versions[index];result.push_back(uint16_t(index));}}
std::vector<uint16_t> NativeSimulation::changedTiles(std::vector<uint32_t>& knownVersions) const{std::vector<uint16_t> result;changedTilesInto(knownVersions,result);return result;}
void NativeSimulation::rebuildRegionIndex(){for(auto& batch:regionBatches_)batch.clear();std::fill(regionAwake_.begin(),regionAwake_.end(),0);for(const size_t slot:activeSlots_){if(slot==SIZE_MAX||slot>=organisms_.size()||!organisms_[slot].living)continue;const auto& organism=organisms_[slot];const int region=(wrapY(organism.y)/RegionSize)*regionColumns_+wrapX(organism.x)/RegionSize;regionBatches_[region].push_back(slot);regionAwake_[region]=1;}}
std::pair<int,int> NativeSimulation::rotated(int x, int y, int direction) const {
  if (direction == 1) return {-x,-y}; if (direction == 2) return {y,-x}; if (direction == 3) return {-y,x}; return {x,y};
}
void NativeSimulation::placeBody(const Organism& organism) {
  if(ownerSlots_.size()!=owners.size())ownerSlots_.resize(owners.size(),UINT32_MAX);const auto slot=slotById_.at(organism.id);
  for (const auto& cell : organism.cells) { const auto [rx,ry]=rotated(cell.x,cell.y,organism.rotation);const int index=wrappedIndex(organism.x+rx,organism.y+ry);cells[index]=uint8_t(cell.type);owners[index]=organism.id;ownerSlots_[index]=uint32_t(slot);markDirtyIndex(index,1); }
}
void NativeSimulation::rebuildOwnerSlots(){ownerSlots_.assign(owners.size(),UINT32_MAX);for(const auto slot:activeSlots_){if(slot==SIZE_MAX||slot>=organisms_.size())continue;const auto& organism=organisms_[slot];if(!organism.living)continue;for(const auto& cell:organism.cells){const auto [x,y]=rotated(cell.x,cell.y,organism.rotation);ownerSlots_[wrappedIndex(organism.x+x,organism.y+y)]=uint32_t(slot);}}}
void NativeSimulation::clearBody(const Organism& organism){for(const auto& cell:organism.cells){const auto [rx,ry]=rotated(cell.x,cell.y,organism.rotation);const int index=wrappedIndex(organism.x+rx,organism.y+ry);if(owners[index]==organism.id){cells[index]=uint8_t(CellType::Empty);owners[index]=-1;markDirtyIndex(index,1);}}}
void NativeSimulation::refreshCapabilities(Organism& organism) {
  organism.isProducer=organism.isMover=organism.isConsumer=organism.isAttacker=false;
  for(const auto& cell:organism.cells){organism.isProducer|=cell.type==CellType::Producer;organism.isMover|=cell.type==CellType::Mover;organism.isConsumer|=cell.type==CellType::Mouth||cell.type==CellType::PlantMouth||cell.type==CellType::ScavengerMouth||cell.type==CellType::MineralMouth||cell.type==CellType::DecomposerMouth;organism.isAttacker|=cell.type==CellType::Killer&&cell.durability>0;}
  if(organism.isMover&&!organism.brain){organism.brain=std::make_shared<Nnue>(random_,Nnue::DeferredInit{});organism.brainGenomeId=nextBrainGenomeId_++;}else if(!organism.isMover){organism.brain.reset();organism.brainGenomeId=0;}
}
void NativeSimulation::reset() {
  watchedId_=-1;decisionCursor_=decisionCount_=0;familyRoot_=-2;familyRecords_=SIZE_MAX;familyRoles_.clear();followPreferences={};
  selectedSpeciesId_=-1;deathsByCause_.fill(0);sampledDeaths_.fill(0);sampledBirths_=0;nextSpeciesId_=1;lastSeason_=-1;lastPopulationPeak_=crashBaseline_=crashMinimum_=0;crashActive_=atmosphereEventActive_=false;resourceShortageRuns_.fill(0);resourceShortageActive_.fill(false);atlasRevision_=0;species_.clear();timeline_.clear();evolutionEvents_.clear();classificationBuffer_.clear();
  atmosphereCells_=atmosphereMovers_=atmosphereProducers_=0;reproductionEnabled_=mortalityEnabled_=true;populationTarget_=0;births_=respiratoryDeaths_=0;carrionProcessed_=sampledCarrionProcessed_=plantProduced_=sampledPlantProduced_=0;lastRespiratoryDeathTick_=-10;atmosphericOxygen_=.21;carbonDioxide_=.0004;selectedId_=-1;deadCount_=0;std::fill(cells.begin(),cells.end(),uint8_t(CellType::Empty));std::fill(owners.begin(),owners.end(),-1);organisms_.clear();slotById_.clear();activeSlots_.clear();respiratoryCandidates_.clear();ticks_=0;++resets_;markAllDirty();
  moisture.assign(size_t(width_)*height_,300);
  Organism organism;organism.id=nextId_++;organism.x=width_/2;organism.y=height_/2;
  organism.cells={{CellType::Mouth,0,0,0},{CellType::Producer,-1,-1,0},{CellType::Producer,1,1,0}};
  refreshCapabilities(organism);largest_=std::max(largest_,int(organism.cells.size()));organism.activeIndex=activeSlots_.size();activeSlots_.push_back(organisms_.size());organisms_.push_back(std::move(organism));slotById_[organisms_.back().id]=organisms_.size()-1;placeBody(organisms_.back());registerAtmosphere(organisms_.back(),1);rebuildRegionIndex();
}

void NativeSimulation::regenerate(uint32_t seed) {
  auto preservedTimeline=std::move(timeline_);auto preservedSpecies=std::move(species_);auto preservedEvents=std::move(evolutionEvents_);const int preservedNextSpecies=nextSpeciesId_;const uint64_t preservedRevision=atlasRevision_;
  worldSeed_ = seed;
  world = generateWorld(width_, height_, worldSeed_);
  reset();
  timeline_=std::move(preservedTimeline);species_=std::move(preservedSpecies);evolutionEvents_=std::move(preservedEvents);nextSpeciesId_=preservedNextSpecies;atlasRevision_=preservedRevision;addEvolutionEvent(EvolutionEventType::Regeneration,-1,float(seed),"World regenerated from seed "+std::to_string(seed));
}

int NativeSimulation::seedBenchmarkMovers(int count) {
  watchedId_=-1;decisionCursor_=decisionCount_=0;familyRoot_=-2;familyRecords_=SIZE_MAX;familyRoles_.clear();followPreferences={};
  selectedSpeciesId_=-1;deathsByCause_.fill(0);sampledDeaths_.fill(0);sampledBirths_=0;nextSpeciesId_=1;lastSeason_=-1;lastPopulationPeak_=crashBaseline_=crashMinimum_=0;crashActive_=atmosphereEventActive_=false;atlasRevision_=0;species_.clear();timeline_.clear();evolutionEvents_.clear();classificationBuffer_.clear();
  atmosphereCells_=atmosphereMovers_=atmosphereProducers_=0;reproductionEnabled_=mortalityEnabled_=true;populationTarget_=count;births_=respiratoryDeaths_=0;lastRespiratoryDeathTick_=-10;
  std::fill(cells.begin(),cells.end(),uint8_t(CellType::Empty));
  std::fill(owners.begin(),owners.end(),-1);
  organisms_.clear();slotById_.clear();activeSlots_.clear();respiratoryCandidates_.clear();ticks_=0;record_=largest_=nnueEvaluations_=deadCount_=0;selectedId_=-1;
  constexpr int BrainFamilyCount=256;std::vector<std::shared_ptr<Nnue>> brainFamilies;brainFamilies.reserve(BrainFamilyCount);for(int family=0;family<BrainFamilyCount;++family){auto brain=std::make_shared<Nnue>(random_);if(family)brain->mutate(random_,.01,.08);brainFamilies.push_back(std::move(brain));}nextBrainGenomeId_=BrainFamilyCount+1;
  organisms_.reserve(std::max<size_t>(organisms_.capacity(),size_t(count)));
  constexpr std::array<uint8_t,4> channelPatterns{DefaultSenseChannels,255,uint8_t(Occupancy|Danger|Resources),uint8_t(Heading|Temperature|Fertility|Internal)};
  int candidate=0;
  for(int y=2;y<height_-2&&int(organisms_.size())<count;y+=3)for(int x=2;x<width_-2&&int(organisms_.size())<count;x+=3,++candidate){
    const bool stationaryProducer=candidate%5==0;Organism organism;organism.id=nextId_++;organism.x=x;organism.y=y;organism.energy=18'000+(candidate%9)*2'000;organism.minerals=400+(candidate%7)*150;organism.brain=stationaryProducer?nullptr:brainFamilies[candidate%BrainFamilyCount];organism.brainGenomeId=stationaryProducer?0:uint64_t(candidate%BrainFamilyCount+1);organism.mutability=3+candidate%8;organism.neuralMutability=2.0+(candidate%11);organism.oxygenTolerance=.105+(candidate%7)*.005;organism.stressRecovery=.006+(candidate%6)*.002;organism.frailty=.75+(candidate%9)*.0625;
    organism.cells.push_back({stationaryProducer?CellType::Producer:CellType::Mover,0,0,0});
    const CellType diet=candidate%4==0?CellType::PlantMouth:candidate%4==1?CellType::ScavengerMouth:candidate%4==2?CellType::MineralMouth:CellType::DecomposerMouth;organism.cells.push_back({diet,1,0,0});
    if(candidate%4==0)organism.cells.push_back({CellType::Killer,-1,0,KillerDurability});
    if(candidate%5==0)organism.cells.push_back({CellType::Armor,0,1,ArmorDurability});
    if(candidate%7==0)organism.cells.push_back({CellType::Producer,0,-1,0});
    const auto perception=clampPerception(1+candidate%4,channelPatterns[candidate%channelPatterns.size()]);organism.perceptionRadius=perception.radius;organism.senseChannels=perception.channels;organism.neuralCost=perception.cost;refreshCapabilities(organism);
    if(!isClear(organism,x,y,0))continue;
    largest_=std::max(largest_,int(organism.cells.size()));organism.activeIndex=activeSlots_.size();activeSlots_.push_back(organisms_.size());organisms_.push_back(std::move(organism));slotById_[organisms_.back().id]=organisms_.size()-1;placeBody(organisms_.back());registerAtmosphere(organisms_.back(),1);
  }
  record_=int(organisms_.size());updateClimateCache();markAllDirty();rebuildRegionIndex();return int(organisms_.size());
}

bool NativeSimulation::paintTerrain(int x, int y, TerrainType terrain) {
  const int index = safeIndex(x, y);
  if (index < 0) return false;
  if(owners[index]>=0&&(terrain==TerrainType::Water||terrain==TerrainType::Mountain))if(auto* organism=organismById(owners[index]))die(*organism,DeathCause::Terrain);
  world.terrain[index] = uint8_t(terrain);
  world.habitat[index] = uint8_t(HabitatType::Surface);
  if (terrain == TerrainType::Water || terrain == TerrainType::Mountain) {
    world.resources[index] = uint8_t(ResourceType::None);if(terrain==TerrainType::Water||terrain==TerrainType::Mountain)world.nutrients[index]=0;else world.nutrients[index]=uint16_t(std::min<int>(world.nutrients[index],terrain==TerrainType::Fertile?2400:terrain==TerrainType::Plains?1600:500));
    world.resourceAmount[index] = 0;
  }
  markDirtyIndex(index);
  return true;
}

bool NativeSimulation::paintResource(int x, int y, ResourceType resource, uint16_t amount) {
  const int index = safeIndex(x, y);
  if (index < 0) return false;
  world.resources[index] = uint8_t(resource);
  world.resourceAmount[index] = resource == ResourceType::None ? 0 : amount;
  markDirtyIndex(index,2);
  return true;
}
bool NativeSimulation::paintNutrients(int x,int y,uint16_t amount){const int index=safeIndex(x,y);if(index<0)return false;const auto terrain=TerrainType(world.terrain[index]);if(terrain==TerrainType::Water||terrain==TerrainType::Mountain)return false;const int capacity=terrain==TerrainType::Fertile?2400:terrain==TerrainType::Plains?1600:500;world.nutrients[index]=uint16_t(std::min<int>(amount,capacity));markDirtyIndex(index,4);return true;}
bool NativeSimulation::paintHabitat(int x,int y,HabitatType habitat){const int index=safeIndex(x,y);if(index<0)return false;if(habitat==HabitatType::Cave)world.terrain[index]=uint8_t(TerrainType::Mountain);else if(habitat==HabitatType::Bridge)world.terrain[index]=uint8_t(TerrainType::Water);world.habitat[index]=uint8_t(habitat);if(habitat!=HabitatType::Surface){world.resources[index]=uint8_t(ResourceType::None);world.resourceAmount[index]=0;}world.nutrients[index]=0;markDirtyIndex(index);return true;}

const Organism* NativeSimulation::organismAt(int x, int y) const {
  const int index = safeIndex(x, y);
  return index < 0 ? nullptr : organismById(owners[index]);
}

std::optional<OrganismInspection> NativeSimulation::inspect(int id) const {
  const auto findAny=[&](int sought)->const Organism*{return organismById(sought);};
  const Organism* organism=findAny(id);if(!organism)return std::nullopt;OrganismInspection result;
  result.id=organism->id;result.parentId=organism->parentId;result.generation=organism->generation;result.birthTick=organism->birthTick;result.deathTick=organism->deathTick;result.age=organism->lifetime;result.cellCount=int(organism->cells.size());result.energy=organism->energy;result.minerals=organism->minerals;result.damage=organism->damage;result.mutability=organism->mutability;result.neuralMutability=organism->neuralMutability;result.alive=organism->living;result.isMover=organism->isMover;result.thermalStress=organism->thermalStress;result.respiratoryStress=organism->hypoxiaStress;result.oxygenTolerance=organism->oxygenTolerance;result.stressRecovery=organism->stressRecovery;result.frailty=organism->frailty;result.respiratoryRisk=organism->respiratoryRisk;result.deathCause=organism->deathCause;result.perceptionRadius=organism->perceptionRadius;result.senseChannels=organism->senseChannels;result.neuralCost=organism->neuralCost;result.carrionConsumed=organism->carrionConsumed;result.nutrientsReleased=organism->nutrientsReleased;result.decompositionEnergy=organism->decompositionEnergy;result.localNutrients=world.nutrients[wrappedIndex(organism->x,organism->y)];result.mutations=organism->mutations;result.senses=organism->lastFeatures;result.outputs=organism->lastOutputs;result.hasOutputs=organism->lastAction>=0;constexpr const char* actions[]={"Up","Down","Left","Right","Wait"};result.action=organism->lastAction>=0&&organism->lastAction<5?actions[organism->lastAction]:"Static";
  for(int parent=organism->parentId;parent>=0;){const auto* ancestor=findAny(parent);if(!ancestor)break;result.ancestors.insert(result.ancestors.begin(),{ancestor->id,ancestor->generation,ancestor->living});parent=ancestor->parentId;}
  refreshFamily(id);result.descendants=familyDescendants_;
  result.totalDescendants=organism->totalDescendants;
  const int row=std::clamp(organism->y,0,height_-1),index=safeIndex(organism->x,organism->y);result.temperature=climateTemperature_[row];result.fertility=climateFertility_[row];constexpr const char* terrainNames[]={"Plains","Fertile","Desert","Water","Mountain"};constexpr const char* resourceNames[]={"None","Plant","Carrion","Mineral"};constexpr const char* habitatNames[]={"Surface","Cave","Bridge"};result.terrain=index>=0?terrainNames[world.terrain[index]]:"Unknown";result.resource=index>=0?resourceNames[world.resources[index]]:"None";result.habitat=index>=0?habitatNames[world.habitat[index]]:"Unknown";result.elevation=index>=0?world.elevation[index]:0;
  bool plant=false,carrion=false,mineral=false,decomposer=false;for(const auto& cell:organism->cells){plant|=cell.type==CellType::Mouth||cell.type==CellType::PlantMouth;carrion|=cell.type==CellType::ScavengerMouth;mineral|=cell.type==CellType::MineralMouth;decomposer|=cell.type==CellType::DecomposerMouth;if(cell.type==CellType::Killer)result.attackDurability+=cell.durability;if(cell.type==CellType::Armor)result.armorDurability+=cell.durability;}if(plant)result.diet="Plant";if(carrion)result.diet+=(result.diet.empty()?"":" + ")+std::string("Carrion");if(mineral)result.diet+=(result.diet.empty()?"":" + ")+std::string("Mineral");if(decomposer)result.diet+=(result.diet.empty()?"":" + ")+std::string("Decomposer");if(result.diet.empty())result.diet="None";result.metabolicCost=result.cellCount*CellBaseCost+(organism->isMover?MoveCost:0)+organism->neuralCost/20.0+std::count_if(organism->cells.begin(),organism->cells.end(),[](const BodyCell& cell){return cell.type==CellType::DecomposerMouth;})*3;return result;
}

void NativeSimulation::pruneLineage() {
  if(deadCount_<=lineageLimit_)return;
  const size_t count=organisms_.size();pruneChildren_.assign(count,0);pruneProtected_.assign(count,0);pruneRemoved_.assign(count,0);pruneParents_.assign(count,SIZE_MAX);
  for(size_t index=0;index<count;++index)if(const auto parent=slotById_.find(organisms_[index].parentId);parent!=slotById_.end()){pruneParents_[index]=parent->second;++pruneChildren_[parent->second];}
  const auto protect=[&](size_t slot){while(slot<count&&!pruneProtected_[slot]){pruneProtected_[slot]=1;slot=pruneParents_[slot];}};
  for(size_t index=0;index<count;++index)if(organisms_[index].living)protect(index);for(const int id:{selectedId_,watchedId_})if(const auto selected=slotById_.find(id);selected!=slotById_.end())protect(selected->second);
  bool progress=true,removed=false;
  while(deadCount_>lineageLimit_&&progress){progress=false;for(size_t index=0;index<count&&deadCount_>lineageLimit_;++index){const auto& organism=organisms_[index];if(organism.living||pruneProtected_[index]||pruneRemoved_[index]||pruneChildren_[index]>0)continue;pruneRemoved_[index]=1;--deadCount_;if(pruneParents_[index]!=SIZE_MAX)--pruneChildren_[pruneParents_[index]];progress=removed=true;}}
  if(!removed)return;
  familyRoot_=-2;familyRecords_=SIZE_MAX;pruneSlotRemap_.assign(count,SIZE_MAX);
  size_t write=0;for(size_t read=0;read<count;++read){if(pruneRemoved_[read]){slotById_.erase(organisms_[read].id);continue;}const bool living=organisms_[read].living;if(living)pruneSlotRemap_[read]=write;if(write!=read){organisms_[write]=std::move(organisms_[read]);slotById_.at(organisms_[write].id)=write;if(living){const auto& organism=organisms_[write];for(const auto& cell:organism.cells){const auto [x,y]=rotated(cell.x,cell.y,organism.rotation);ownerSlots_[wrappedIndex(organism.x+x,organism.y+y)]=uint32_t(write);}}}++write;}organisms_.resize(write);
  for(size_t index=0;index<activeSlots_.size();++index){const auto old=activeSlots_[index];activeSlots_[index]=old<count?pruneSlotRemap_[old]:SIZE_MAX;if(activeSlots_[index]!=SIZE_MAX)organisms_[activeSlots_[index]].activeIndex=index;}
}

void NativeSimulation::eat(Organism& organism, CellType mouth, int x, int y) {
  for(const auto [dx,dy]:directions){const int index=wrappedIndex(x+dx,y+dy);const auto resource=ResourceType(world.resources[index]);
    const bool decomposer=mouth==CellType::DecomposerMouth;const bool compatible=((mouth==CellType::Mouth||mouth==CellType::PlantMouth)&&resource==ResourceType::Plant)||(mouth==CellType::ScavengerMouth&&resource==ResourceType::Carrion)||(decomposer&&resource==ResourceType::Carrion)||(mouth==CellType::MineralMouth&&resource==ResourceType::Mineral);
    if(compatible&&world.resourceAmount[index]>0){const int consumed=std::min(100,int(world.resourceAmount[index]));world.resourceAmount[index]-=uint16_t(consumed);if(resource==ResourceType::Mineral)organism.minerals+=consumed;else if(decomposer){const int gained=consumed*DecomposerEnergy/100;organism.energy+=gained;organism.carrionConsumed+=consumed;organism.decompositionEnergy+=gained;carrionProcessed_+=consumed;const int released=consumed*3/4;organism.nutrientsReleased+=released;for(const auto& [nx,ny]:directions){const int target=wrappedIndex(index%width_+nx,index/width_+ny);const auto terrain=TerrainType(world.terrain[target]);const int capacity=terrain==TerrainType::Fertile?2400:terrain==TerrainType::Plains?1600:terrain==TerrainType::Desert?500:0;world.nutrients[target]=uint16_t(std::min(capacity,int(world.nutrients[target])+released/4));markDirtyIndex(target,4);}carbonDioxide_=std::min(.20,carbonDioxide_+consumed*2e-9);}else organism.energy+=int(std::floor(consumed/100.0*(resource==ResourceType::Plant?PlantEnergy:CarrionEnergy)));if(!world.resourceAmount[index])world.resources[index]=uint8_t(ResourceType::None);markDirtyIndex(index,2);}
    else if(CellType(cells[index])==CellType::Food){cells[index]=uint8_t(CellType::Empty);owners[index]=-1;organism.energy+=PlantEnergy;markDirtyIndex(index,1);}
  }
}
void NativeSimulation::produce(Organism& organism, int x, int y) {
  if(organism.isMover||organism.energy<ProduceCost)return;
  const int origin=wrappedIndex(x,y);const auto terrain=TerrainType(world.terrain[origin]);const int row=wrapY(y);
  const double productivity=terrain==TerrainType::Fertile?1.35:terrain==TerrainType::Desert?.3:terrain==TerrainType::Plains?1.0:0.0,nutrientFactor=.2+.8*std::min(1.0,world.nutrients[origin]/800.0);
  const double moistureFactor=moisture.empty()?1.0:.7+double(moisture[wrappedIndex(x,y)])/1000.0;
  if(random_()>=foodChance_*climateFertility_[row]*productivity*nutrientFactor*moistureFactor)return;
  const auto [dx,dy]=directions[int(random_()*4.0)];const int index=wrappedIndex(x+dx,y+dy);if(passable(world,index)&&HabitatType(world.habitat[index])!=HabitatType::Bridge){organism.energy-=ProduceCost;world.nutrients[origin]=uint16_t(std::max(0,int(world.nutrients[origin])-10));world.resources[index]=uint8_t(ResourceType::Plant);world.resourceAmount[index]=uint16_t(std::min(65535,int(world.resourceAmount[index])+50));++plantProduced_;markDirtyIndex(origin,4);markDirtyIndex(index,2);}
}
Organism* NativeSimulation::organismById(int id){const auto found=slotById_.find(id);return found==slotById_.end()?nullptr:&organisms_[found->second];}
const Organism* NativeSimulation::organismById(int id) const {const auto found=slotById_.find(id);return found==slotById_.end()?nullptr:&organisms_[found->second];}
BodyCell* NativeSimulation::localCellAt(Organism& organism,int x,int y){for(auto& cell:organism.cells){const auto [rx,ry]=rotated(cell.x,cell.y,organism.rotation);if(wrapX(organism.x+rx)==wrapX(x)&&wrapY(organism.y+ry)==wrapY(y))return &cell;}return nullptr;}
void NativeSimulation::harm(Organism& organism){++organism.damage;if(mortalityEnabled_&&organism.damage>=int(organism.cells.size()))die(organism,DeathCause::Combat);}
void NativeSimulation::harmAt(Organism& organism,int index){if(CellType(cells[index])==CellType::Armor){auto* armor=localCellAt(organism,index%width_,index/width_);if(armor&&armor->durability){--armor->durability;if(armor->durability<=0){armor->type=CellType::Inert;cells[index]=uint8_t(CellType::Inert);markDirtyIndex(index,1);refreshCapabilities(organism);}return;}}harm(organism);}
void NativeSimulation::attack(Organism& attacker,BodyCell& weapon,int x,int y){if(!weapon.durability)return;for(const auto& [dx,dy]:directions){const int index=wrappedIndex(x+dx,y+dy);if(CellType(cells[index])==CellType::Armor)continue;auto* victim=organismById(owners[index]);if(!victim||victim==&attacker||!victim->living)continue;const bool mutual=CellType(cells[index])==CellType::Killer;if(attacker.energy<AttackCost)return;attacker.energy-=AttackCost;--weapon.durability;const int before=victim->damage;harmAt(*victim,index);if(victim->damage>before)attacker.energy+=240;if(mutual)harm(attacker);if(weapon.durability<=0){weapon.type=CellType::Inert;const int origin=wrappedIndex(x,y);cells[origin]=uint8_t(CellType::Inert);markDirtyIndex(origin,1);refreshCapabilities(attacker);return;}}}
bool NativeSimulation::isClear(const Organism& organism,int x,int y,int rotation) const {
  return isClearBody(organism.cells,organism.id,x,y,rotation);
}
bool NativeSimulation::isClearBody(std::span<const BodyCell> body,int id,int x,int y,int rotation) const {
  for(const auto& cell:body){const auto [rx,ry]=rotated(cell.x,cell.y,rotation);const int index=wrappedIndex(x+rx,y+ry);if(!passable(world,index))return false;if(owners[index]!=id&&CellType(cells[index])!=CellType::Empty)return false;}return true;
}
bool NativeSimulation::straightPath(int x1,int y1,int x2,int y2,const Organism& parent) const {
  const auto open=[&](int x,int y){const int index=wrappedIndex(x,y);return passable(world,index)&&(CellType(cells[index])==CellType::Empty||CellType(cells[index])==CellType::Food||owners[index]==parent.id);};
  if(x1==x2){for(int y=std::min(y1,y2);y<std::max(y1,y2);++y)if(!open(x1,y))return false;return true;}for(int x=std::min(x1,x2);x<std::max(x1,x2);++x)if(!open(x,y1))return false;return true;
}
void NativeSimulation::features(const Organism& organism,std::vector<uint16_t>& result) const {
  result.clear();if(result.capacity()<64)result.reserve(64);int square=0;
  for(int ring=1;ring<=std::min(MaximumSensorRadius,organism.perceptionRadius);++ring)for(int dy=-ring;dy<=ring;++dy)for(int dx=-ring;dx<=ring;++dx)if((dx||dy)&&std::max(std::abs(dx),std::abs(dy))==ring){const int current=square++;const int index=wrappedIndex(organism.x+dx,organism.y+dy);int category=6,owner=-1;if(index>=0){const auto type=CellType(cells[index]);owner=owners[index];const uint32_t ownerSlot=ownerSlots_.size()==owners.size()?ownerSlots_[index]:UINT32_MAX;const Organism* neighbor=owner>=0&&owner!=organism.id&&type!=CellType::Killer?(ownerSlot<organisms_.size()&&organisms_[ownerSlot].id==owner?&organisms_[ownerSlot]:organismById(owner)):nullptr;category=owner==organism.id?3:type==CellType::Killer?5:neighbor&&neighbor->isMover?7+neighbor->direction:owner>=0?4:type==CellType::Food?1:type==CellType::Wall?2:0;}
    const int mask=organism.senseChannels;const bool enabled=(category==5||category==6)?mask&Danger:category>=7&&category<=10?mask&Heading:category==1?mask&Resources:mask&Occupancy;if(enabled)result.push_back(uint16_t(current*FeatureCategories+category));
    if(mask&Resources){const auto resource=index>=0?ResourceType(world.resources[index]):ResourceType::None;if(resource==ResourceType::Carrion)result.push_back(uint16_t(current*FeatureCategories+12));else if(resource==ResourceType::Mineral)result.push_back(uint16_t(current*FeatureCategories+13));else if(resource==ResourceType::Plant&&category!=1)result.push_back(uint16_t(current*FeatureCategories+1));}
    if((mask&Terrain)&&index>=0)result.push_back(uint16_t(current*FeatureCategories+15+world.terrain[index]));
    if((mask&SoilNutrients)&&index>=0&&world.nutrients[index]>=400)result.push_back(uint16_t(NutrientFeatureOffset+current));
    if((mask&CarrionDensity)&&index>=0&&ResourceType(world.resources[index])==ResourceType::Carrion&&world.resourceAmount[index]>=200)result.push_back(uint16_t(CarrionDensityFeatureOffset+current));
  }
  constexpr int stateOffset=PositionCount*FeatureCategories,climateOffset=stateOffset+8;const int mask=organism.senseChannels;
  if(mask&Internal){result.push_back(uint16_t(stateOffset+(organism.energy>=int(organism.cells.size())*6*EnergyScale?1:0)));result.push_back(uint16_t(stateOffset+2+(organism.damage>0?1:0)));result.push_back(uint16_t(stateOffset+4+organism.direction));}
  const int row=std::clamp(organism.y,0,height_-1);if(mask&Temperature){result.push_back(uint16_t(climateOffset+climateBand_[row]));result.push_back(uint16_t(climateOffset+5+climateGradient_[row]+1));}if(mask&Fertility){result.push_back(uint16_t(climateOffset+8+int(std::floor(cyclePhase(ticks_)*4.0))%4));result.push_back(uint16_t(climateOffset+12+std::min(4,int(std::floor((climateFertility_[row]-.35)*5.0)))));}
}
bool NativeSimulation::attemptMove(Organism& organism){const auto [dx,dy]=directions[organism.direction];const int nextX=wrapX(organism.x+dx),nextY=wrapY(organism.y+dy);if(!isClear(organism,nextX,nextY,organism.rotation))return false;const int from=wrappedIndex(organism.x,organism.y),to=wrappedIndex(nextX,nextY);const int climb=std::max(0,int(world.elevation[to])-int(world.elevation[from]));organism.energy=std::max(0,organism.energy-climb/16-(HabitatType(world.habitat[to])==HabitatType::Bridge?3:0));clearBody(organism);organism.x=nextX;organism.y=nextY;placeBody(organism);return true;}
bool NativeSimulation::attemptRotate(Organism& organism){const int rotation=int(random_()*4.0);if(!isClear(organism,organism.x,organism.y,rotation))return false;clearBody(organism);organism.rotation=rotation;organism.direction=int(random_()*4.0);organism.moveCount=0;placeBody(organism);return true;}
void NativeSimulation::mutate(Organism& organism) {
  const int choice=int(random_()*100.0);
  constexpr std::array<CellType,8> types{CellType::PlantMouth,CellType::ScavengerMouth,CellType::MineralMouth,CellType::DecomposerMouth,CellType::Producer,CellType::Mover,CellType::Killer,CellType::Armor};
  const auto randomType=[&]{return types[int(random_()*types.size())];};
  const auto mineralCost=[](CellType type){return type==CellType::Killer?250:type==CellType::Armor?180:0;};
  if(choice<=33){auto base=organism.cells[int(random_()*organism.cells.size())];constexpr std::array<std::pair<int,int>,8> offsets{{{0,-1},{0,1},{-1,0},{1,0},{-1,-1},{1,1},{-1,1},{1,-1}}};const auto [dx,dy]=offsets[int(random_()*8.0)];const auto type=randomType();const int cost=mineralCost(type);if(organism.minerals>=cost&&std::none_of(organism.cells.begin(),organism.cells.end(),[&](const BodyCell& cell){return cell.x==base.x+dx&&cell.y==base.y+dy;})){organism.minerals-=cost;organism.cells.push_back({type,base.x+dx,base.y+dy,type==CellType::Killer?KillerDurability:type==CellType::Armor?ArmorDurability:0});organism.mutations.push_back(std::string("Added ")+cellName(type)+" cell");++organism.birthDistance;refreshCapabilities(organism);}}
  else if(choice<=66){auto& cell=organism.cells[int(random_()*organism.cells.size())];const auto before=cell.type;const auto type=randomType();const int cost=mineralCost(type);if(organism.minerals<cost)return;organism.minerals-=cost;cell={type,cell.x,cell.y,type==CellType::Killer?KillerDurability:type==CellType::Armor?ArmorDurability:0};organism.mutations.push_back(std::string("Changed ")+cellName(before)+" to "+cellName(type));refreshCapabilities(organism);}
  else if(organism.cells.size()>1){const size_t index=size_t(std::floor(random_()*organism.cells.size()));if(organism.cells[index].x||organism.cells[index].y){organism.mutations.push_back(std::string("Removed ")+cellName(organism.cells[index].type)+" cell");organism.cells.erase(organism.cells.begin()+index);}refreshCapabilities(organism);}
  if(organism.isMover&&random_()*100.0<=10){organism.moveRange=std::max(1,organism.moveRange+int(random_()*4.0)-2);organism.mutations.push_back("Movement range changed to "+std::to_string(organism.moveRange));}
  if(random_()*100.0<=10){organism.birthDistance=std::max(1,organism.birthDistance+int(random_()*5.0)-2);organism.mutations.push_back("Birth distance changed to "+std::to_string(organism.birthDistance));}
  if(organism.isMover&&random_()<.1){const auto perception=clampPerception(organism.perceptionRadius+(random_()<.5?-1:1),organism.senseChannels);organism.perceptionRadius=perception.radius;organism.senseChannels=perception.channels;organism.neuralCost=perception.cost;organism.mutations.push_back("Perception radius changed to "+std::to_string(perception.radius));}
  if(organism.isMover&&random_()<.1){const int bit=1<<int(random_()*10.0);const auto perception=clampPerception(organism.perceptionRadius,organism.senseChannels^bit);organism.perceptionRadius=perception.radius;organism.senseChannels=perception.channels;organism.neuralCost=perception.cost;organism.mutations.push_back("Sensory channel mask changed");}
}
void NativeSimulation::reproduce(Organism& parent) {
  const int growthMinerals=std::max(0,int(parent.cells.size())-3)*100;if(parent.minerals<growthMinerals)return;
  // Copy inherited state only. Copying then clearing the parent's strings and sensory
  // buffers allocated several heaps for every failed birth in a crowded plant colony.
  Organism child;child.generation=parent.generation;child.mutability=parent.mutability;child.neuralMutability=parent.neuralMutability;child.birthDistance=parent.birthDistance;child.moveRange=parent.moveRange;child.isProducer=parent.isProducer;child.isMover=parent.isMover;child.isConsumer=parent.isConsumer;child.isAttacker=parent.isAttacker;child.thermalStress=parent.thermalStress;child.oxygenTolerance=parent.oxygenTolerance;child.stressRecovery=parent.stressRecovery;child.frailty=parent.frailty;child.perceptionRadius=parent.perceptionRadius;child.senseChannels=parent.senseChannels;child.neuralCost=parent.neuralCost;child.totalDescendants=parent.totalDescendants;child.lastOutputs=parent.lastOutputs;child.activeIndex=parent.activeIndex;child.brainGenomeId=parent.brainGenomeId;child.speciesId=parent.speciesId;child.brain=parent.brain;
  child.id=nextId_++;child.parentId=parent.id;++child.generation;child.birthTick=ticks_;child.energy=int(std::floor(parent.energy*.35));child.minerals=int(std::floor(parent.minerals*.25));
  child.carrionConsumed=child.nutrientsReleased=child.decompositionEnergy=0;child.rotation=int(random_()*4.0);
  child.mutability=std::max(1,child.mutability+(random_()<=.5?1:-1));if(random_()*100.0<=parent.mutability){child.cells=parent.cells;mutate(child);}
  bool neuralMutation=false;
  if(child.isMover&&child.brain&&random_()*100.0<=child.neuralMutability){neuralMutation=true;Nnue::captureMutation(random_,neuralMutationScratch_);child.brainGenomeId=nextBrainGenomeId_++;}
  const bool changedNeuralRate=random_()<.1;if(changedNeuralRate){child.neuralMutability=std::clamp(child.neuralMutability+(random_()<.5?-.5:.5),.5,50.0);}
  const auto geneRoll=[&](uint64_t salt){uint64_t value=uint64_t(child.id)^salt;value=(value^(value>>30))*0xbf58476d1ce4e5b9ULL;value=(value^(value>>27))*0x94d049bb133111ebULL;value^=value>>31;return value;};const auto geneDelta=[&](uint64_t salt){return double(geneRoll(salt)&0xffff)/65535.0-.5;};
  const bool changedOxygen=geneRoll(0x4f32544f4c455241ULL)%100<12;if(changedOxygen){child.oxygenTolerance=std::clamp(child.oxygenTolerance+geneDelta(0x101)*.012,.08,.16);}
  const bool changedRecovery=geneRoll(0x5245434f56455259ULL)%100<12;if(changedRecovery){child.stressRecovery=std::clamp(child.stressRecovery+geneDelta(0x202)*.004,.002,.025);}
  const bool changedFrailty=geneRoll(0x465241494c545931ULL)%100<12;if(changedFrailty){child.frailty=std::clamp(child.frailty+geneDelta(0x303)*.20,.35,2.0);}
  const auto [dx,dy]=directions[int(random_()*4.0)];const int offset=int(random_()*3.0);child.x=wrapX(parent.x+dx*(parent.birthDistance+offset));child.y=wrapY(parent.y+dy*(parent.birthDistance+offset));
  const int transferredEnergy=child.energy,transferredMinerals=child.minerals;
  const bool canPlace=isClearBody(child.cells.empty()?std::span<const BodyCell>(parent.cells):std::span<const BodyCell>(child.cells),child.id,child.x,child.y,child.rotation)&&straightPath(child.x,child.y,parent.x,parent.y,parent);parent.energy=std::max(0,parent.energy-transferredEnergy);parent.minerals=std::max(0,parent.minerals-transferredMinerals);
  if(canPlace){if(child.cells.empty())child.cells=parent.cells;child.mutations.insert(child.mutations.begin(),"Body mutation rate changed to "+std::to_string(child.mutability)+"%");if(neuralMutation)child.mutations.push_back("NNUE weights mutated");if(changedNeuralRate)child.mutations.push_back("Neural mutation rate changed");if(changedOxygen)child.mutations.push_back("Oxygen tolerance changed to "+std::to_string(child.oxygenTolerance));if(changedRecovery)child.mutations.push_back("Respiratory recovery changed to "+std::to_string(child.stressRecovery));if(changedFrailty)child.mutations.push_back("Frailty changed to "+std::to_string(child.frailty));if(neuralMutation){child.brain=std::make_shared<Nnue>(*child.brain);child.brain->applyMutation(neuralMutationScratch_);}if(child.brain){child.brain->materialize();(void)child.brain->signature();}++parent.directOffspring;parent.minerals-=growthMinerals;largest_=std::max(largest_,int(child.cells.size()));child.activeIndex=activeSlots_.size();activeSlots_.push_back(organisms_.size());organisms_.push_back(std::move(child));slotById_[organisms_.back().id]=organisms_.size()-1;placeBody(organisms_.back());registerAtmosphere(organisms_.back(),1);++births_;}
}
void NativeSimulation::die(Organism& organism,DeathCause cause){if(!organism.living)return;registerAtmosphere(organism,-1);organism.deathTick=ticks_;organism.deathCauseCode=cause;organism.deathCause=deathCauseName(cause);++deadCount_;++deathsByCause_[size_t(cause)];if(organism.speciesId>0)for(auto& species:species_)if(species.id==organism.speciesId){species.recentDeathCauses[species.recentDeathCursor]=uint8_t(cause);species.recentDeathCursor=uint8_t((species.recentDeathCursor+1)%species.recentDeathCauses.size());species.recentDeathCount=uint8_t(std::min<size_t>(species.recentDeathCount+1,species.recentDeathCauses.size()));break;}for(const auto& cell:organism.cells){const auto [rx,ry]=rotated(cell.x,cell.y,organism.rotation);const int index=wrappedIndex(organism.x+rx,organism.y+ry);if(owners[index]==organism.id){cells[index]=uint8_t(CellType::Empty);owners[index]=-1;world.resources[index]=uint8_t(ResourceType::Carrion);world.resourceAmount[index]=uint16_t(std::min(65535,int(world.resourceAmount[index])+200+organism.energy/std::max(1,int(organism.cells.size()))/20));markDirtyIndex(index,3);}}organism.living=false;if(organism.activeIndex<activeSlots_.size())activeSlots_[organism.activeIndex]=SIZE_MAX;}
void NativeSimulation::processRespiratoryMortality(){if(!mortalityEnabled_||ticks_-lastRespiratoryDeathTick_<10)return;respiratoryCandidates_.clear();respiratoryCandidates_.reserve(std::max(respiratoryCandidates_.capacity(),activeSlots_.size()));for(const size_t slot:activeSlots_){if(slot==SIZE_MAX||slot>=organisms_.size())continue;const auto& organism=organisms_[slot];if(organism.living&&organism.respiratoryRisk>=.05)respiratoryCandidates_.push_back({organism.respiratoryRisk,organism.id});}if(respiratoryCandidates_.empty())return;const auto lowerPriority=[](const RespiratoryCandidate& left,const RespiratoryCandidate& right){return left.risk<right.risk||(left.risk==right.risk&&left.id>right.id);};std::make_heap(respiratoryCandidates_.begin(),respiratoryCandidates_.end(),lowerPriority);if(auto* victim=organismById(respiratoryCandidates_.front().id)){die(*victim,DeathCause::RespiratoryFailure);++respiratoryDeaths_;lastRespiratoryDeathTick_=ticks_;}}
NativeSimulation::PreparedPhysiology NativeSimulation::preparePhysiology(const Organism& organism) const {
  PreparedPhysiology result;const int row=wrapY(organism.y);result.temperature=climateTemperature_[row];result.discomfort=std::max(0.0,std::abs(result.temperature-.5)-.15);
  const bool desert=TerrainType(world.terrain[wrappedIndex(organism.x,organism.y)])==TerrainType::Desert;
  result.energy=std::max(0,organism.energy-int(organism.cells.size())*CellBaseCost-int(std::count_if(organism.cells.begin(),organism.cells.end(),[](const BodyCell& cell){return cell.type==CellType::DecomposerMouth;}))*3-int(std::round(result.discomfort*30.0))-(desert?4:0));
  result.stressDelta=result.temperature>=.35&&result.temperature<=.65?-.004:.001+std::abs(result.temperature-(result.temperature<.35?.35:.65))*.012;
  result.agePressure=1.0+.25*std::clamp(double(organism.lifetime+1)/std::max(1.0,double(organism.cells.size()*lifespan_)),0.0,1.0);result.energyPressure=result.energy<int(organism.cells.size())*2*EnergyScale?1.35:1.0;result.bodyPressure=1.0+.04*std::log2(std::max(1.0,double(organism.cells.size())));return result;
}
void NativeSimulation::updateOrganism(Organism& organism,size_t slot) {
  const bool sampled=profileTicks_&&(organism.id&63)==0;using Clock=std::chrono::steady_clock;auto stamp=sampled?Clock::now():Clock::time_point{};const auto detail=[&](double& value){if(sampled){const auto now=Clock::now();value+=64*std::chrono::duration<double,std::milli>(now-stamp).count();stamp=now;}};
  if(!organism.living)return;const auto prepared=slot<preparedPhysiology_.size()?preparedPhysiology_[slot]:preparePhysiology(organism);++organism.lifetime;organism.energy=prepared.energy;
  const double temperature=prepared.temperature,discomfort=prepared.discomfort;organism.thermalStress=std::max(0.0,organism.thermalStress+prepared.stressDelta);
  if(organism.thermalStress>=1){organism.thermalStress-=1;if(mortalityEnabled_){++organism.damage;if(organism.damage>=int(organism.cells.size())){die(organism,DeathCause::ThermalStress);return;}}}
  const double co2Tolerance=.07+(.12-organism.oxygenTolerance)*.5,oxygenDeficit=std::max(0.0,organism.oxygenTolerance-atmosphericOxygen_),co2Excess=std::max(0.0,carbonDioxide_-co2Tolerance);const double agePressure=prepared.agePressure,energyPressure=prepared.energyPressure,temperaturePressure=1.0+discomfort,bodyPressure=prepared.bodyPressure,respiratoryLoad=(oxygenDeficit*.10+co2Excess*.05)*agePressure*energyPressure*temperaturePressure*bodyPressure;organism.hypoxiaStress=std::clamp(organism.hypoxiaStress+(respiratoryLoad>0?respiratoryLoad:-organism.stressRecovery),0.0,1.0);organism.respiratoryRisk=std::max(0.0,organism.hypoxiaStress-.65)*organism.frailty*agePressure*energyPressure;
  if(mortalityEnabled_&&organism.lifetime>int(organism.cells.size())*lifespan_){die(organism,DeathCause::OldAge);return;}
  if(organism.energy<=0){organism.energy=0;if(mortalityEnabled_){++organism.damage;if(organism.damage>=int(organism.cells.size())){die(organism,DeathCause::Starvation);return;}}}
  if(birthOxygenCache_!=atmosphericOxygen_||birthCarbonCache_!=carbonDioxide_){const double oxygenHealth=std::clamp((atmosphericOxygen_-.10)/.08,0.0,1.0),carbonHealth=std::clamp((.10-carbonDioxide_)/.06,0.0,1.0),atmosphereHealth=std::min(oxygenHealth,carbonHealth);birthThresholdCache_=uint64_t(std::round(atmosphereHealth*atmosphereHealth*1023.0));birthOxygenCache_=atmosphericOxygen_;birthCarbonCache_=carbonDioxide_;}
  const uint64_t reproductionDraw=(uint64_t(organism.id)*0x9e3779b97f4a7c15ULL+uint64_t(ticks_)*0xbf58476d1ce4e5b9ULL)>>54;const bool atmosphereAllowsBirth=reproductionDraw<birthThresholdCache_;
  detail(tickWork_.physiology);if(reproductionEnabled_&&atmosphereAllowsBirth&&(!populationTarget_||int(activeSlots_.size())<populationTarget_)&&organism.energy>=int(organism.cells.size())*6*EnergyScale)reproduce(organism);detail(tickWork_.reproduction);
  if(organism.isConsumer||organism.isProducer||organism.isAttacker)for(auto& local:organism.cells){const auto [rx,ry]=rotated(local.x,local.y,organism.rotation);const int x=organism.x+rx,y=organism.y+ry;if(local.type==CellType::Mouth||local.type==CellType::PlantMouth||local.type==CellType::ScavengerMouth||local.type==CellType::MineralMouth||local.type==CellType::DecomposerMouth)eat(organism,local.type,x,y);else if(local.type==CellType::Producer)produce(organism,x,y);else if(local.type==CellType::Killer)attack(organism,local,x,y);}
  detail(tickWork_.feeding);if(!organism.living||!organism.isMover||!organism.brain)return;const int interval=std::clamp((organism.neuralCost/16+int(organism.neuralCost%16>0)),1,16);if((ticks_+organism.id)%interval==0){++nnueEvaluations_;organism.energy=std::max(0,organism.energy-(organism.neuralCost/20+int(organism.neuralCost%20>0)));features(organism,organism.lastFeatures);organism.lastOutputs=organism.brain->evaluate(organism.lastFeatures);int best=0;for(int action=1;action<OutputSize;++action)if(organism.lastOutputs[action]>organism.lastOutputs[best])best=action;organism.lastAction=best;captureDecision(organism);organism.direction=best;}if(organism.direction>=4){detail(tickWork_.neural);return;}++organism.moveCount;organism.energy=std::max(0,organism.energy-MoveCost);attemptMove(organism);if(organism.moveCount>organism.moveRange)attemptRotate(organism);detail(tickWork_.neural);
}
GenomeDescriptor NativeSimulation::describeGenome(const Organism& organism) const {
  GenomeDescriptor result;result.brainGenomeId=organism.brainGenomeId;result.neuralSignature=organism.brain?organism.brain->signature():0;result.perceptionRadius=uint8_t(organism.perceptionRadius);result.senseChannels=organism.senseChannels;result.moveRange=uint16_t(std::clamp(organism.moveRange,0,65535));result.birthDistance=uint16_t(std::clamp(organism.birthDistance,0,65535));result.oxygenTolerance=uint16_t(std::round(organism.oxygenTolerance*10'000));result.stressRecovery=uint16_t(std::round(organism.stressRecovery*10'000));result.frailty=uint16_t(std::round(organism.frailty*1'000));result.mutability=uint16_t(std::clamp(organism.mutability,0,65535));result.neuralMutability=uint16_t(std::clamp(int(std::round(organism.neuralMutability*100)),0,65535));
  for(const auto& cell:organism.cells){const int type=std::clamp(int(cell.type),0,12);if(result.cellCounts[type]<255)++result.cellCounts[type];if(cell.type==CellType::Mouth||cell.type==CellType::PlantMouth)result.dietMask|=1;else if(cell.type==CellType::ScavengerMouth)result.dietMask|=2;else if(cell.type==CellType::MineralMouth)result.dietMask|=4;else if(cell.type==CellType::DecomposerMouth)result.dietMask|=8;}
  uint64_t canonical=UINT64_MAX;std::array<uint64_t,256> encoded;const size_t count=std::min(encoded.size(),organism.cells.size());for(int rotation=0;rotation<4;++rotation){int minX=INT_MAX,minY=INT_MAX;for(size_t index=0;index<count;++index){const auto [x,y]=rotated(organism.cells[index].x,organism.cells[index].y,rotation);minX=std::min(minX,x);minY=std::min(minY,y);}for(size_t index=0;index<count;++index){const auto [x,y]=rotated(organism.cells[index].x,organism.cells[index].y,rotation);encoded[index]=(uint64_t(uint8_t(organism.cells[index].type))<<32)|(uint64_t(uint16_t(x-minX))<<16)|uint16_t(y-minY);}std::sort(encoded.begin(),encoded.begin()+count);uint64_t hash=1469598103934665603ULL;for(size_t index=0;index<count;++index){hash^=encoded[index];hash*=1099511628211ULL;}canonical=std::min(canonical,hash);}result.bodyHash=canonical;return result;
}
void NativeSimulation::addEvolutionEvent(EvolutionEventType type,int speciesId,float value,const std::string& label){if(evolutionEvents_.size()>=20'000)evolutionEvents_.erase(evolutionEvents_.begin(),evolutionEvents_.begin()+1'000);evolutionEvents_.push_back({type,ticks_,speciesId,value,label});++atlasRevision_;}
void NativeSimulation::classifySpecies(bool useWorkers){
  classificationBuffer_.clear();classificationBuffer_.reserve(std::max(classificationBuffer_.capacity(),activeSlots_.size()));for(const size_t slot:activeSlots_)if(slot!=SIZE_MAX&&slot<organisms_.size()&&organisms_[slot].living)classificationBuffer_.push_back(&organisms_[slot]);const auto byId=[](const Organism* left,const Organism* right){return left->id<right->id;};if(!std::is_sorted(classificationBuffer_.begin(),classificationBuffer_.end(),byId))std::sort(classificationBuffer_.begin(),classificationBuffer_.end(),byId);for(auto& species:species_)if(!species.extinct){species.population=0;species.habitatPopulation.fill(0);species.averageElevation=0;}
  if(useWorkers&&parallelAnalysis_&&classificationBuffer_.size()>2048&&!analysisPool_)analysisPool_=std::make_shared<AnalysisPool>();
  classificationDescriptors_.resize(classificationBuffer_.size());
  const auto describe=[&](size_t begin,size_t end){for(size_t index=begin;index<end;++index)classificationDescriptors_[index]=describeGenome(*classificationBuffer_[index]);};
  if(useWorkers&&parallelAnalysis_&&analysisPool_)analysisPool_->run(classificationBuffer_.size(),describe);else describe(0,classificationBuffer_.size());
  classificationCache_.begin(classificationBuffer_.size());classificationJobs_.clear();classificationMatches_.resize(classificationDescriptors_.size());size_t matchIndex=0;++classificationJobEpoch_;
  for(const auto& descriptor:classificationDescriptors_){auto& cached=classificationCache_.query(descriptor);classificationMatches_[matchIndex++]=&cached;if(cached.jobEpoch!=classificationJobEpoch_){cached.jobEpoch=classificationJobEpoch_;classificationJobs_.push_back(&cached);}}
  const auto search=[&](size_t begin,size_t end){for(size_t job=begin;job<end;++job){auto& cached=*classificationJobs_[job];for(size_t index=0;index<species_.size();++index){const auto& species=species_[index];if(species.extinct||(cached.distance==0&&cached.best!=SIZE_MAX&&species.id>=species_[cached.best].id))continue;const double distance=genomeDistanceBounded(cached.key,species.representative,std::min(.22,cached.distance));if(distance<cached.distance||(distance==cached.distance&&cached.best!=SIZE_MAX&&species.id<species_[cached.best].id)){cached.distance=distance;cached.best=index;}}cached.checked=species_.size();}};
  if(useWorkers&&parallelAnalysis_&&analysisPool_)analysisPool_->run(classificationJobs_.size(),search);else search(0,classificationJobs_.size());
  size_t descriptorIndex=0;
  for(Organism* organism:classificationBuffer_){const size_t current=descriptorIndex++;const auto descriptor=classificationDescriptors_[current];auto& cached=*classificationMatches_[current];if(cached.best!=SIZE_MAX&&(cached.best>=species_.size()||species_[cached.best].extinct)){cached.best=SIZE_MAX;cached.checked=0;cached.distance=2;}for(size_t index=cached.checked;index<species_.size();++index){const auto& species=species_[index];if(species.extinct||(cached.distance==0&&cached.best!=SIZE_MAX&&species.id>=species_[cached.best].id))continue;const double distance=genomeDistanceBounded(descriptor,species.representative,std::min(.22,cached.distance));if(distance<cached.distance||(distance==cached.distance&&cached.best!=SIZE_MAX&&species.id<species_[cached.best].id)){cached.distance=distance;cached.best=index;}}cached.checked=species_.size();double bestDistance=cached.distance;SpeciesSummary* best=cached.best==SIZE_MAX?nullptr:&species_[cached.best];if(!best||bestDistance>.22){int parentSpecies=-1;if(const auto* parent=organismById(organism->parentId))parentSpecies=parent->speciesId;species_.push_back({nextSpeciesId_++,parentSpecies,organism->id,0,0,ticks_,ticks_,0,0,uint8_t(world.terrain[wrappedIndex(organism->x,organism->y)]),descriptor.dietMask,false,"",{},0,0,descriptor});best=&species_.back();if(parentSpecies>=0)for(auto& candidate:species_)if(candidate.id==parentSpecies){++candidate.descendantSpecies;break;}addEvolutionEvent(EvolutionEventType::Speciation,best->id,0,"Species #"+std::to_string(best->id)+" emerged");}organism->speciesId=best->id;++best->population;const int home=wrappedIndex(organism->x,organism->y);++best->habitatPopulation[world.habitat[home]];best->averageElevation+=world.elevation[home];best->peakPopulation=std::max(best->peakPopulation,best->population);best->lastTick=ticks_;best->missingScans=0;}
  for(auto& species:species_)if(!species.extinct&&species.population==0&&++species.missingScans>=2){species.extinct=true;std::array<int,7> causes{};for(size_t index=0;index<species.recentDeathCount;++index)++causes[species.recentDeathCauses[index]];const auto dominant=std::max_element(causes.begin()+1,causes.end());species.extinctionCause=*dominant>0?deathCauseName(DeathCause(std::distance(causes.begin(),dominant))):"Population loss";addEvolutionEvent(EvolutionEventType::Extinction,species.id,0,"Species #"+std::to_string(species.id)+" became extinct: "+species.extinctionCause);}
  if(species_.size()>2'048){
    std::vector<int> protectedIds;for(int id=selectedSpeciesId_;id>=0;){protectedIds.push_back(id);const auto found=std::find_if(species_.begin(),species_.end(),[&](const SpeciesSummary& species){return species.id==id;});id=found==species_.end()?-1:found->parentId;}
    while(species_.size()>2'048){auto oldest=species_.end();for(auto candidate=species_.begin();candidate!=species_.end();++candidate){if(!candidate->extinct||std::find(protectedIds.begin(),protectedIds.end(),candidate->id)!=protectedIds.end())continue;if(oldest==species_.end()||candidate->lastTick<oldest->lastTick||(candidate->lastTick==oldest->lastTick&&candidate->id<oldest->id))oldest=candidate;}if(oldest==species_.end())break;species_.erase(oldest);classificationCache_.invalidate();}
  }
  for(auto& species:species_)if(species.population){species.averageElevation/=species.population;species.dominantHabitat=uint8_t(std::max_element(species.habitatPopulation.begin(),species.habitatPopulation.end())-species.habitatPopulation.begin());}
  ++atlasRevision_;
}
void NativeSimulation::sampleEcology(){
  EcologySample sample;
  const auto current=metrics();
  sample.cavePopulation=current.caveOrganisms;sample.bridgePopulation=current.bridgeOrganisms;sample.averageElevation=float(current.averageElevation);sample.rainfall=rainfall();double wet=0;for(auto value:moisture)wet+=value;sample.averageMoisture=moisture.empty()?0:float(wet/moisture.size());
  sample.tick=ticks_;sample.population=current.organisms;sample.births=births_-sampledBirths_;sampledBirths_=births_;
  for(size_t index=0;index<sample.deaths.size();++index){sample.deaths[index]=deathsByCause_[index]-sampledDeaths_[index];sampledDeaths_[index]=deathsByCause_[index];}
  sample.plantEaters=current.plantEaters;sample.scavengers=current.scavengers;sample.mineralEaters=current.mineralEaters;sample.decomposers=current.decomposers;sample.predators=current.predators;sample.criticalRespiratory=current.criticalRespiratory;sample.carrionProcessed=uint32_t(carrionProcessed_-sampledCarrionProcessed_);sampledCarrionProcessed_=carrionProcessed_;sample.plantProduced=uint32_t(plantProduced_-sampledPlantProduced_);sampledPlantProduced_=plantProduced_;
  sample.averageEnergy=float(current.averageEnergy);sample.averageRespiratoryStress=float(current.averageRespiratoryStress);sample.averageOxygenTolerance=float(current.averageOxygenTolerance);
  sample.oxygen=float(current.oxygen);sample.carbonDioxide=float(current.carbonDioxide);sample.temperature=float(current.temperature);sample.fertility=float(current.fertility);sample.nnueEvaluations=current.nnueEvaluations;sample.memoryEstimate=current.memoryEstimate;
  double bodies=0,perception=0,recovery=0,frailty=0;
  for(const size_t slot:activeSlots_)if(slot!=SIZE_MAX&&slot<organisms_.size()&&organisms_[slot].living){const auto& organism=organisms_[slot];bodies+=organism.cells.size();perception+=organism.neuralCost;recovery+=organism.stressRecovery;frailty+=organism.frailty;}
  if(sample.population){sample.averageBodySize=float(bodies/sample.population);sample.averagePerceptionCost=float(perception/sample.population);sample.averageStressRecovery=float(recovery/sample.population);sample.averageFrailty=float(frailty/sample.population);}
  for(size_t index=0;index<world.resources.size();++index){const uint32_t amount=world.resourceAmount[index];if(ResourceType(world.resources[index])==ResourceType::Plant)sample.plant+=amount;else if(ResourceType(world.resources[index])==ResourceType::Carrion)sample.carrion+=amount;else if(ResourceType(world.resources[index])==ResourceType::Mineral)sample.mineral+=amount;sample.soilNutrients+=world.nutrients[index];sample.nutrientPoorTiles+=world.nutrients[index]<100&&TerrainType(world.terrain[index])!=TerrainType::Water&&TerrainType(world.terrain[index])!=TerrainType::Mountain;}
  sample.species=int(std::count_if(species_.begin(),species_.end(),[](const SpeciesSummary& species){return !species.extinct&&species.population>0;}));
  if(timeline_.size()>=24'000)timeline_.erase(timeline_.begin(),timeline_.begin()+1'000);
  if(!timeline_.empty()&&sample.population>=20){const auto dominant=[](const EcologySample& s){const std::array<int,3> counts{s.population-s.cavePopulation-s.bridgePopulation,s.cavePopulation,s.bridgePopulation};return int(std::max_element(counts.begin(),counts.end())-counts.begin());};const int habitat=dominant(sample);if(habitat!=dominant(timeline_.back())){constexpr const char* labels[]={"Surface","Cave","Bridge"};addEvolutionEvent(EvolutionEventType::HabitatShift,-1,float(habitat),std::string("Population habitat shift: ")+labels[habitat]);}}
  timeline_.push_back(sample);
  const int season=int(std::floor(cyclePhase(ticks_)*4))%4;
  if(season!=lastSeason_){lastSeason_=season;addEvolutionEvent(EvolutionEventType::Season,-1,float(season),"Season changed");}
  if(sample.population>lastPopulationPeak_){lastPopulationPeak_=sample.population;addEvolutionEvent(EvolutionEventType::PopulationPeak,-1,float(sample.population),"Population reached "+std::to_string(sample.population));}
  const int population600=timeline_.size()>10?timeline_[timeline_.size()-11].population:sample.population;
  crashBaseline_=population600;
  if(!crashActive_&&sample.population*5<=population600*4){crashActive_=true;crashMinimum_=sample.population;addEvolutionEvent(EvolutionEventType::Crash,-1,float(sample.population),"Population crash");}
  else if(crashActive_){crashMinimum_=std::min(crashMinimum_,sample.population);if(sample.population*5>=crashMinimum_*6){crashActive_=false;addEvolutionEvent(EvolutionEventType::Recovery,-1,float(sample.population),"Population recovery");}}
  const std::array<uint32_t,3> resources{sample.plant,sample.carrion,sample.mineral};
  for(size_t resource=0;resource<resources.size();++resource){
    std::vector<uint32_t> history;const size_t begin=timeline_.size()>400?timeline_.size()-400:0;history.reserve(timeline_.size()-begin);
    for(size_t index=begin;index+1<timeline_.size();++index)history.push_back(resource==0?timeline_[index].plant:resource==1?timeline_[index].carrion:timeline_[index].mineral);
    if(history.empty())continue;const size_t middle=history.size()/2;std::nth_element(history.begin(),history.begin()+middle,history.end());
    const bool low=resources[resource]*4<history[middle];resourceShortageRuns_[resource]=low?uint8_t(std::min(255,int(resourceShortageRuns_[resource])+1)):0;
    if(resourceShortageRuns_[resource]>=5&&!resourceShortageActive_[resource]){resourceShortageActive_[resource]=true;constexpr const char* names[]{"Plant","Carrion","Mineral"};addEvolutionEvent(EvolutionEventType::ResourceShortage,-1,float(resources[resource]),std::string(names[resource])+" shortage");}else if(!low)resourceShortageActive_[resource]=false;
  }
  const bool atmosphereStressed=sample.population>0&&sample.criticalRespiratory*20>sample.population;
  if(atmosphereStressed&&!atmosphereEventActive_)addEvolutionEvent(EvolutionEventType::AtmosphericStress,-1,float(sample.criticalRespiratory),"Atmospheric stress");
  const bool carrionHigh=sample.carrion>std::max<uint32_t>(100'000,sample.plant/2);if(carrionHigh&&!carrionEventActive_)addEvolutionEvent(EvolutionEventType::CarrionAccumulation,-1,float(sample.carrion),"Carrion accumulation");carrionEventActive_=carrionHigh;
  const bool nutrientLow=sample.nutrientPoorTiles*3>world.nutrients.size();if(nutrientLow&&!nutrientCollapseActive_)addEvolutionEvent(EvolutionEventType::NutrientCollapse,-1,float(sample.nutrientPoorTiles),"Soil nutrient collapse");else if(!nutrientLow&&nutrientCollapseActive_)addEvolutionEvent(EvolutionEventType::NutrientRecovery,-1,float(sample.soilNutrients),"Soil nutrient recovery");nutrientCollapseActive_=nutrientLow;
  if(sample.decomposers>0&&lastDecomposerPopulation_==0)addEvolutionEvent(EvolutionEventType::DecomposerEmergence,-1,float(sample.decomposers),"Decomposer strategy emerged");else if(sample.decomposers==0&&lastDecomposerPopulation_>0)addEvolutionEvent(EvolutionEventType::DecomposerExtinction,-1,0,"Decomposer strategy became extinct");lastDecomposerPopulation_=sample.decomposers;
  atmosphereEventActive_=atmosphereStressed;++atlasRevision_;
}
AtlasSnapshot NativeSimulation::atlasSnapshot() const{return{atlasRevision_,selectedSpeciesId_,timeline_,species_,evolutionEvents_};}
void NativeSimulation::step(int count) {
  for(int iteration=0;iteration<count;++iteration){
    using Clock=std::chrono::steady_clock;auto mark=profileTicks_?Clock::now():Clock::time_point{};
    if(profileTicks_)tickWork_={};
    const auto phase=[&](double& destination){if(profileTicks_){const auto now=Clock::now();destination=std::chrono::duration<double,std::milli>(now-mark).count();mark=now;}};
    ++ticks_;updateClimateCache();const size_t activeCount=activeSlots_.size();const int deathsBefore=deadCount_;
    if(parallelAnalysis_&&(activeCount>2048||world.nutrients.size()>=262144)&&!analysisPool_)analysisPool_=std::make_shared<AnalysisPool>();preparedPhysiology_.resize(organisms_.size());
    const auto prepare=[&](size_t begin,size_t end){for(size_t index=begin;index<end;++index){const size_t slot=activeSlots_[index];if(slot!=SIZE_MAX&&slot<organisms_.size()&&organisms_[slot].living)preparedPhysiology_[slot]=preparePhysiology(organisms_[slot]);}};
    if(parallelAnalysis_&&analysisPool_)analysisPool_->run(activeCount,prepare);else prepare(0,activeCount);
    for(size_t index=0;index<activeCount;++index){
      const size_t slot=activeSlots_[index];if(slot==SIZE_MAX||slot>=organisms_.size()||!organisms_[slot].living)continue;const size_t before=organisms_.size();updateOrganism(organisms_[slot],slot);
      for(size_t childIndex=before;childIndex<organisms_.size();++childIndex){
        int ancestorId=organisms_[childIndex].parentId;
        while(ancestorId>=0){auto* ancestor=organismById(ancestorId);if(!ancestor)break;++ancestor->totalDescendants;ancestorId=ancestor->parentId;}
      }
    }
    phase(tickWork_.organisms);processRespiratoryMortality();if(deadCount_!=deathsBefore){activeSlots_.erase(std::remove(activeSlots_.begin(),activeSlots_.end(),SIZE_MAX),activeSlots_.end());for(size_t index=0;index<activeSlots_.size();++index)organisms_[activeSlots_[index]].activeIndex=index;}
    const bool regionsChanged=(ticks_&7)==0||deadCount_!=deathsBefore||activeSlots_.size()!=activeCount;const size_t recordsBeforePrune=organisms_.size();
    updateAtmosphere();
    const auto environment=[&]{if(ticks_%30==0)updateNutrients();if(ticks_%120==0)updateDrainage();};
    const auto lineage=[&]{if(deadCount_>lineageLimit_)pruneLineage();if(regionsChanged||recordsBeforePrune!=organisms_.size())rebuildRegionIndex();record_=std::max(record_,int(activeSlots_.size()));};
    if(parallelAnalysis_&&analysisPool_&&ticks_%300==0){
      // Soil/drainage touch no organism or species data. Living ancestor chains
      // survive pruning, so classifying before dead-leaf compaction is equivalent.
      double environmentMs=0;std::exception_ptr failure;
      std::jthread background([&]{const auto began=Clock::now();try{environment();}catch(...){failure=std::current_exception();}environmentMs=std::chrono::duration<double,std::milli>(Clock::now()-began).count();});
      const auto atlasBegan=Clock::now();classifySpecies();const double atlasMs=std::chrono::duration<double,std::milli>(Clock::now()-atlasBegan).count();
      const auto lineageBegan=Clock::now();lineage();const double lineageMs=std::chrono::duration<double,std::milli>(Clock::now()-lineageBegan).count();background.join();if(failure)std::rethrow_exception(failure);
      const auto sampleBegan=Clock::now();sampleEcology();if(profileTicks_){tickWork_.environment=environmentMs;tickWork_.lineage=lineageMs;tickWork_.atlas=atlasMs+std::chrono::duration<double,std::milli>(Clock::now()-sampleBegan).count();}
    }else{
      environment();phase(tickWork_.environment);lineage();phase(tickWork_.lineage);if(ticks_%300==0)classifySpecies();if(ticks_%60==0)sampleEcology();phase(tickWork_.atlas);
    }
  }
}
NativeMetrics NativeSimulation::metrics() const {
  double energy=0,mutation=0,stress=0,oxygenTolerance=0,respiratoryStress=0;int living=0,plantEaters=0,scavengers=0,mineralEaters=0,decomposers=0,predators=0,criticalRespiratory=0;
  size_t memory=moisture.capacity()*sizeof(uint16_t)+(drainageNutrientScratch_.capacity()+drainageWaterScratch_.capacity())*sizeof(int)+cells.capacity()*sizeof(uint8_t)+owners.capacity()*sizeof(int32_t)+(world.terrain.capacity()+world.resources.capacity()+world.elevation.capacity()+world.habitat.capacity())*sizeof(uint8_t)+(world.resourceAmount.capacity()+world.nutrients.capacity())*sizeof(uint16_t)+organisms_.capacity()*sizeof(Organism);int caveOrganisms=0,bridgeOrganisms=0;double elevation=0;
  for(size_t region=0;region<regionBatches_.size();++region){if(!regionAwake_[region])continue;for(const size_t slot:regionBatches_[region]){const auto& organism=organisms_[slot];energy+=organism.energy;mutation+=organism.mutability;stress+=organism.thermalStress;oxygenTolerance+=organism.oxygenTolerance;respiratoryStress+=organism.hypoxiaStress;criticalRespiratory+=organism.respiratoryRisk>=.05;++living;const int home=wrappedIndex(organism.x,organism.y);caveOrganisms+=HabitatType(world.habitat[home])==HabitatType::Cave;bridgeOrganisms+=HabitatType(world.habitat[home])==HabitatType::Bridge;elevation+=world.elevation[home];memory+=organism.cells.capacity()*sizeof(BodyCell)+organism.mutations.capacity()*sizeof(std::string)+organism.lastFeatures.capacity()*sizeof(uint16_t);for(const auto& cell:organism.cells){if(cell.type==CellType::Mouth||cell.type==CellType::PlantMouth)++plantEaters;else if(cell.type==CellType::ScavengerMouth)++scavengers;else if(cell.type==CellType::MineralMouth)++mineralEaters;else if(cell.type==CellType::DecomposerMouth)++decomposers;else if(cell.type==CellType::Killer)++predators;}}}
  const auto climate=climateAt(height_/2,height_,ticks_);const int awake=int(std::count(regionAwake_.begin(),regionAwake_.end(),uint8_t(1))),dirty=int(std::count(dirtyTiles_.begin(),dirtyTiles_.end(),uint8_t(1)));return{living,record_,resets_,ticks_,largest_,living?energy/living/EnergyScale:0,living?mutation/living:0,living?stress/living:0,climate.temperature,climate.fertility,atmosphericOxygen_,carbonDioxide_,living?oxygenTolerance/living:0,living?respiratoryStress/living:0,plantEaters,scavengers,mineralEaters,decomposers,predators,nnueEvaluations_,respiratoryDeaths_,criticalRespiratory,caveOrganisms,bridgeOrganisms,living?elevation/living:0,memory,awake,int(regionAwake_.size())-awake,dirty};
}
}
