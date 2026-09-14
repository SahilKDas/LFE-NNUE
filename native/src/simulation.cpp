#include "simulation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

namespace life {
namespace {
constexpr std::array<std::pair<int,int>,4> directions{{{0,-1},{0,1},{-1,0},{1,0}}};
const char* cellName(CellType type){constexpr const char* names[]={"empty","food","wall","mouth","producer","mover","killer","armor","plant mouth","scavenger mouth","mineral mouth","inert tissue"};const int index=int(type);return index>=0&&index<int(std::size(names))?names[index]:"unknown";}
}

NativeSimulation::NativeSimulation(int width, int height, double foodChance, int lifespan, uint32_t worldSeed, uint32_t randomSeed, int lineageLimit)
  : width_(width), height_(height), worldSeed_(worldSeed), random_(randomSeed), foodChance_(foodChance), lifespan_(lifespan),lineageLimit_(std::max(0,lineageLimit)),
    climateTemperature_(height),climateFertility_(height),climateGradient_(height),climateBand_(height),world(generateWorld(width, height, worldSeed)), cells(width * height), owners(width * height, -1) { organisms_.reserve(100'000);reset(); }

void NativeSimulation::updateClimateCache(){const double wave=std::sin(cyclePhase(ticks_)*std::numbers::pi*2.0),step=2.0/std::max(1,height_-1);const int direction=std::abs(wave*step)<.002?0:wave>0?-1:1;for(int y=0;y<height_;++y){const double latitude=1.0-2.0*y/std::max(1,height_-1),value=std::clamp(.5+.5*latitude*wave,0.0,1.0);climateTemperature_[y]=float(value);climateFertility_[y]=float(.35+value);climateGradient_[y]=int8_t(direction);climateBand_[y]=uint8_t(std::min(4,int(std::floor(value*5.0))));}}

int NativeSimulation::safeIndex(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_ ? y * width_ + x : -1; }
std::pair<int,int> NativeSimulation::rotated(int x, int y, int direction) const {
  if (direction == 1) return {-x,-y}; if (direction == 2) return {y,-x}; if (direction == 3) return {-y,x}; return {x,y};
}
void NativeSimulation::placeBody(const Organism& organism) {
  for (const auto& cell : organism.cells) { const auto [rx,ry]=rotated(cell.x,cell.y,organism.rotation); const int index=safeIndex(organism.x+rx,organism.y+ry); if(index>=0){cells[index]=uint8_t(cell.type);owners[index]=organism.id;} }
}
void NativeSimulation::clearBody(const Organism& organism){for(const auto& cell:organism.cells){const auto [rx,ry]=rotated(cell.x,cell.y,organism.rotation);const int index=safeIndex(organism.x+rx,organism.y+ry);if(index>=0&&owners[index]==organism.id){cells[index]=uint8_t(CellType::Empty);owners[index]=-1;}}}
void NativeSimulation::refreshCapabilities(Organism& organism) {
  organism.isProducer=organism.isMover=organism.isConsumer=organism.isAttacker=false;
  for(const auto& cell:organism.cells){organism.isProducer|=cell.type==CellType::Producer;organism.isMover|=cell.type==CellType::Mover;organism.isConsumer|=cell.type==CellType::Mouth||cell.type==CellType::PlantMouth||cell.type==CellType::ScavengerMouth||cell.type==CellType::MineralMouth;organism.isAttacker|=cell.type==CellType::Killer&&cell.durability>0;}
  if(organism.isMover&&!organism.brain)organism.brain=std::make_shared<Nnue>(random_);else if(!organism.isMover)organism.brain.reset();
}
void NativeSimulation::reset() {
  reproductionEnabled_=mortalityEnabled_=true;selectedId_=-1;deadCount_=0;std::fill(cells.begin(),cells.end(),uint8_t(CellType::Empty));std::fill(owners.begin(),owners.end(),-1);organisms_.clear();slotById_.clear();ticks_=0;++resets_;
  Organism organism;organism.id=nextId_++;organism.x=width_/2;organism.y=height_/2;
  organism.cells={{CellType::Mouth,0,0,0},{CellType::Producer,-1,-1,0},{CellType::Producer,1,1,0}};
  refreshCapabilities(organism);largest_=std::max(largest_,int(organism.cells.size()));organisms_.push_back(std::move(organism));slotById_[organisms_.back().id]=organisms_.size()-1;placeBody(organisms_.back());
}

void NativeSimulation::regenerate(uint32_t seed) {
  worldSeed_ = seed;
  world = generateWorld(width_, height_, worldSeed_);
  reset();
}

int NativeSimulation::seedBenchmarkMovers(int count) {
  reproductionEnabled_=mortalityEnabled_=false;
  std::fill(cells.begin(),cells.end(),uint8_t(CellType::Empty));
  std::fill(owners.begin(),owners.end(),-1);
  organisms_.clear();slotById_.clear();ticks_=0;record_=largest_=nnueEvaluations_=deadCount_=0;selectedId_=-1;
  auto sharedBrain=std::make_shared<Nnue>(random_);
  organisms_.reserve(std::max<size_t>(organisms_.capacity(),size_t(count)));
  constexpr std::array<uint8_t,4> channelPatterns{DefaultSenseChannels,255,uint8_t(Occupancy|Danger|Resources),uint8_t(Heading|Temperature|Fertility|Internal)};
  int candidate=0;
  for(int y=2;y<height_-2&&int(organisms_.size())<count;y+=3)for(int x=2;x<width_-2&&int(organisms_.size())<count;x+=3,++candidate){
    Organism organism;organism.id=nextId_++;organism.x=x;organism.y=y;organism.energy=1'000'000'000;organism.minerals=10'000;organism.brain=sharedBrain;
    organism.cells.push_back({CellType::Mover,0,0,0});
    const CellType diet=candidate%3==0?CellType::PlantMouth:candidate%3==1?CellType::ScavengerMouth:CellType::MineralMouth;organism.cells.push_back({diet,1,0,0});
    if(candidate%4==0)organism.cells.push_back({CellType::Killer,-1,0,KillerDurability});
    if(candidate%5==0)organism.cells.push_back({CellType::Armor,0,1,ArmorDurability});
    if(candidate%7==0)organism.cells.push_back({CellType::Producer,0,-1,0});
    const auto perception=clampPerception(1+candidate%4,channelPatterns[candidate%channelPatterns.size()]);organism.perceptionRadius=perception.radius;organism.senseChannels=perception.channels;organism.neuralCost=perception.cost;refreshCapabilities(organism);
    if(!isClear(organism,x,y,0))continue;
    largest_=std::max(largest_,int(organism.cells.size()));organisms_.push_back(std::move(organism));slotById_[organisms_.back().id]=organisms_.size()-1;placeBody(organisms_.back());
  }
  record_=int(organisms_.size());updateClimateCache();return int(organisms_.size());
}

bool NativeSimulation::paintTerrain(int x, int y, TerrainType terrain) {
  const int index = safeIndex(x, y);
  if (index < 0) return false;
  if(owners[index]>=0&&(terrain==TerrainType::Water||terrain==TerrainType::Mountain))if(auto* organism=organismById(owners[index]))die(*organism);
  world.terrain[index] = uint8_t(terrain);
  if (terrain == TerrainType::Water || terrain == TerrainType::Mountain) {
    world.resources[index] = uint8_t(ResourceType::None);
    world.resourceAmount[index] = 0;
  }
  return true;
}

bool NativeSimulation::paintResource(int x, int y, ResourceType resource, uint16_t amount) {
  const int index = safeIndex(x, y);
  if (index < 0) return false;
  world.resources[index] = uint8_t(resource);
  world.resourceAmount[index] = resource == ResourceType::None ? 0 : amount;
  return true;
}

const Organism* NativeSimulation::organismAt(int x, int y) const {
  const int index = safeIndex(x, y);
  return index < 0 ? nullptr : organismById(owners[index]);
}

std::optional<OrganismInspection> NativeSimulation::inspect(int id) const {
  const auto findAny=[&](int sought)->const Organism*{const auto found=std::find_if(organisms_.begin(),organisms_.end(),[&](const Organism& value){return value.id==sought;});return found==organisms_.end()?nullptr:&*found;};
  const Organism* organism=findAny(id);if(!organism)return std::nullopt;OrganismInspection result;
  result.id=organism->id;result.parentId=organism->parentId;result.generation=organism->generation;result.birthTick=organism->birthTick;result.deathTick=organism->deathTick;result.age=organism->lifetime;result.cellCount=int(organism->cells.size());result.energy=organism->energy;result.minerals=organism->minerals;result.damage=organism->damage;result.mutability=organism->mutability;result.neuralMutability=organism->neuralMutability;result.alive=organism->living;result.isMover=organism->isMover;result.thermalStress=organism->thermalStress;result.perceptionRadius=organism->perceptionRadius;result.senseChannels=organism->senseChannels;result.neuralCost=organism->neuralCost;result.mutations=organism->mutations;result.senses=organism->lastFeatures;result.outputs=organism->lastOutputs;result.hasOutputs=organism->lastAction>=0;constexpr const char* actions[]={"Up","Down","Left","Right","Wait"};result.action=organism->lastAction>=0&&organism->lastAction<5?actions[organism->lastAction]:"Static";
  for(int parent=organism->parentId;parent>=0;){const auto* ancestor=findAny(parent);if(!ancestor)break;result.ancestors.insert(result.ancestors.begin(),{ancestor->id,ancestor->generation,ancestor->living});parent=ancestor->parentId;}
  for(const auto& candidate:organisms_){if(candidate.id==id)continue;for(int parent=candidate.parentId;parent>=0;){if(parent==id){result.descendants.push_back({candidate.id,candidate.generation,candidate.living});break;}const auto* ancestor=findAny(parent);if(!ancestor)break;parent=ancestor->parentId;}}
  result.totalDescendants=organism->totalDescendants;
  result.totalDescendants=int(result.descendants.size());const int row=std::clamp(organism->y,0,height_-1),index=safeIndex(organism->x,organism->y);result.temperature=climateTemperature_[row];result.fertility=climateFertility_[row];constexpr const char* terrainNames[]={"Plains","Fertile","Desert","Water","Mountain"};constexpr const char* resourceNames[]={"None","Plant","Carrion","Mineral"};result.terrain=index>=0?terrainNames[world.terrain[index]]:"Unknown";result.resource=index>=0?resourceNames[world.resources[index]]:"None";
  bool plant=false,carrion=false,mineral=false;for(const auto& cell:organism->cells){plant|=cell.type==CellType::Mouth||cell.type==CellType::PlantMouth;carrion|=cell.type==CellType::ScavengerMouth;mineral|=cell.type==CellType::MineralMouth;if(cell.type==CellType::Killer)result.attackDurability+=cell.durability;if(cell.type==CellType::Armor)result.armorDurability+=cell.durability;}if(plant)result.diet="Plant";if(carrion)result.diet+=(result.diet.empty()?"":" + ")+std::string("Carrion");if(mineral)result.diet+=(result.diet.empty()?"":" + ")+std::string("Mineral");if(result.diet.empty())result.diet="None";result.metabolicCost=result.cellCount*CellBaseCost+(organism->isMover?MoveCost:0)+organism->neuralCost/20.0;return result;
}

void NativeSimulation::pruneLineage() {
  const int DeadRecordLimit=lineageLimit_;
  if(deadCount_<=DeadRecordLimit)return;
  std::unordered_map<int,size_t> allSlots;allSlots.reserve(organisms_.size());
  std::unordered_map<int,int> childCounts;childCounts.reserve(organisms_.size());
  for(size_t index=0;index<organisms_.size();++index){allSlots[organisms_[index].id]=index;if(organisms_[index].parentId>=0)++childCounts[organisms_[index].parentId];}
  std::unordered_set<int> protectedIds;protectedIds.reserve(organisms_.size());
  const auto protect=[&](int id){while(id>=0&&protectedIds.insert(id).second){const auto found=allSlots.find(id);if(found==allSlots.end())break;id=organisms_[found->second].parentId;}};
  for(const auto& organism:organisms_)if(organism.living)protect(organism.id);protect(selectedId_);
  std::unordered_set<int> removed;removed.reserve(size_t(deadCount_-DeadRecordLimit));bool progress=true;
  while(deadCount_>DeadRecordLimit&&progress){progress=false;for(const auto& organism:organisms_){if(deadCount_<=DeadRecordLimit)break;if(organism.living||protectedIds.contains(organism.id)||removed.contains(organism.id)||childCounts[organism.id]>0)continue;removed.insert(organism.id);--deadCount_;if(organism.parentId>=0)--childCounts[organism.parentId];progress=true;}}
  if(removed.empty())return;
  organisms_.erase(std::remove_if(organisms_.begin(),organisms_.end(),[&](const Organism& organism){return removed.contains(organism.id);}),organisms_.end());
  slotById_.clear();for(size_t index=0;index<organisms_.size();++index)if(organisms_[index].living)slotById_[organisms_[index].id]=index;
}

void NativeSimulation::eat(Organism& organism, CellType mouth, int x, int y) {
  for(const auto [dx,dy]:directions){const int index=safeIndex(x+dx,y+dy);if(index<0)continue;const auto resource=ResourceType(world.resources[index]);
    const bool compatible=((mouth==CellType::Mouth||mouth==CellType::PlantMouth)&&resource==ResourceType::Plant)||(mouth==CellType::ScavengerMouth&&resource==ResourceType::Carrion)||(mouth==CellType::MineralMouth&&resource==ResourceType::Mineral);
    if(compatible&&world.resourceAmount[index]>0){const int consumed=std::min(100,int(world.resourceAmount[index]));world.resourceAmount[index]-=uint16_t(consumed);if(resource==ResourceType::Mineral)organism.minerals+=consumed;else organism.energy+=int(std::floor(consumed/100.0*(resource==ResourceType::Plant?PlantEnergy:CarrionEnergy)));if(!world.resourceAmount[index])world.resources[index]=uint8_t(ResourceType::None);}
    else if(CellType(cells[index])==CellType::Food){cells[index]=uint8_t(CellType::Empty);owners[index]=-1;organism.energy+=PlantEnergy;}
  }
}
void NativeSimulation::produce(Organism& organism, int x, int y) {
  const int origin=safeIndex(x,y);const auto terrain=origin>=0?TerrainType(world.terrain[origin]):TerrainType::Water;const int row=std::clamp(y,0,height_-1);
  const double productivity=terrain==TerrainType::Fertile?1.35:terrain==TerrainType::Desert?.3:terrain==TerrainType::Plains?1.0:0.0;
  if((organism.isMover)||organism.energy<ProduceCost||random_()>=foodChance_*climateFertility_[row]*productivity)return;
  const auto [dx,dy]=directions[int(std::floor(random_()*4.0))];const int index=safeIndex(x+dx,y+dy);if(index>=0&&TerrainType(world.terrain[index])!=TerrainType::Water&&TerrainType(world.terrain[index])!=TerrainType::Mountain){organism.energy-=ProduceCost;world.resources[index]=uint8_t(ResourceType::Plant);world.resourceAmount[index]=uint16_t(std::min(65535,int(world.resourceAmount[index])+50));}
}
Organism* NativeSimulation::organismById(int id){const auto found=slotById_.find(id);return found==slotById_.end()?nullptr:&organisms_[found->second];}
const Organism* NativeSimulation::organismById(int id) const {const auto found=slotById_.find(id);return found==slotById_.end()?nullptr:&organisms_[found->second];}
BodyCell* NativeSimulation::localCellAt(Organism& organism,int x,int y){for(auto& cell:organism.cells){const auto [rx,ry]=rotated(cell.x,cell.y,organism.rotation);if(organism.x+rx==x&&organism.y+ry==y)return &cell;}return nullptr;}
void NativeSimulation::harm(Organism& organism){++organism.damage;if(mortalityEnabled_&&organism.damage>=int(organism.cells.size()))die(organism);}
void NativeSimulation::harmAt(Organism& organism,int index){if(CellType(cells[index])==CellType::Armor){auto* armor=localCellAt(organism,index%width_,index/width_);if(armor&&armor->durability){--armor->durability;if(armor->durability<=0){armor->type=CellType::Inert;cells[index]=uint8_t(CellType::Inert);refreshCapabilities(organism);}return;}}harm(organism);}
void NativeSimulation::attack(Organism& attacker,BodyCell& weapon,int x,int y){if(!weapon.durability)return;for(const auto& [dx,dy]:directions){const int index=safeIndex(x+dx,y+dy);if(index<0||CellType(cells[index])==CellType::Armor)continue;auto* victim=organismById(owners[index]);if(!victim||victim==&attacker||!victim->living)continue;const bool mutual=CellType(cells[index])==CellType::Killer;if(attacker.energy<AttackCost)return;attacker.energy-=AttackCost;--weapon.durability;const int before=victim->damage;harmAt(*victim,index);if(victim->damage>before)attacker.energy+=240;if(mutual)harm(attacker);if(weapon.durability<=0){weapon.type=CellType::Inert;const int origin=safeIndex(x,y);if(origin>=0)cells[origin]=uint8_t(CellType::Inert);refreshCapabilities(attacker);return;}}}
bool NativeSimulation::isClear(const Organism& organism,int x,int y,int rotation) const {
  for(const auto& cell:organism.cells){const auto [rx,ry]=rotated(cell.x,cell.y,rotation);const int index=safeIndex(x+rx,y+ry);if(index<0)return false;const auto terrain=TerrainType(world.terrain[index]);if(terrain==TerrainType::Water||terrain==TerrainType::Mountain)return false;if(owners[index]!=organism.id&&CellType(cells[index])!=CellType::Empty)return false;}return true;
}
bool NativeSimulation::straightPath(int x1,int y1,int x2,int y2,const Organism& parent) const {
  const auto passable=[&](int x,int y){const int index=safeIndex(x,y);if(index<0)return false;const auto terrain=TerrainType(world.terrain[index]);return terrain!=TerrainType::Water&&terrain!=TerrainType::Mountain&&(CellType(cells[index])==CellType::Empty||CellType(cells[index])==CellType::Food||owners[index]==parent.id);};
  if(x1==x2){for(int y=std::min(y1,y2);y<std::max(y1,y2);++y)if(!passable(x1,y))return false;return true;}for(int x=std::min(x1,x2);x<std::max(x1,x2);++x)if(!passable(x,y1))return false;return true;
}
std::vector<uint16_t> NativeSimulation::features(const Organism& organism) const {
  std::vector<uint16_t> result;result.reserve(64);int square=0;
  for(int ring=1;ring<=MaximumSensorRadius;++ring)for(int dy=-ring;dy<=ring;++dy)for(int dx=-ring;dx<=ring;++dx)if((dx||dy)&&std::max(std::abs(dx),std::abs(dy))==ring){const int current=square++;if(std::max(std::abs(dx),std::abs(dy))>organism.perceptionRadius)continue;const int index=safeIndex(organism.x+dx,organism.y+dy);int category=6,owner=-1;if(index>=0){const auto type=CellType(cells[index]);owner=owners[index];const Organism* neighbor=owner>=0&&owner!=organism.id?organismById(owner):nullptr;category=owner==organism.id?3:type==CellType::Killer?5:neighbor&&neighbor->isMover?7+neighbor->direction:owner>=0?4:type==CellType::Food?1:type==CellType::Wall?2:0;}
    const int mask=organism.senseChannels;const bool enabled=(category==5||category==6)?mask&Danger:category>=7&&category<=10?mask&Heading:category==1?mask&Resources:mask&Occupancy;if(enabled)result.push_back(uint16_t(current*FeatureCategories+category));
    if(mask&Resources){const auto resource=index>=0?ResourceType(world.resources[index]):ResourceType::None;if(resource==ResourceType::Carrion)result.push_back(uint16_t(current*FeatureCategories+12));else if(resource==ResourceType::Mineral)result.push_back(uint16_t(current*FeatureCategories+13));else if(resource==ResourceType::Plant&&category!=1)result.push_back(uint16_t(current*FeatureCategories+1));}
    if((mask&Terrain)&&index>=0)result.push_back(uint16_t(current*FeatureCategories+15+world.terrain[index]));
  }
  constexpr int stateOffset=PositionCount*FeatureCategories,climateOffset=stateOffset+8;const int mask=organism.senseChannels;
  if(mask&Internal){result.push_back(uint16_t(stateOffset+(organism.energy>=int(organism.cells.size())*6*EnergyScale?1:0)));result.push_back(uint16_t(stateOffset+2+(organism.damage>0?1:0)));result.push_back(uint16_t(stateOffset+4+organism.direction));}
  const int row=std::clamp(organism.y,0,height_-1);if(mask&Temperature){result.push_back(uint16_t(climateOffset+climateBand_[row]));result.push_back(uint16_t(climateOffset+5+climateGradient_[row]+1));}if(mask&Fertility){result.push_back(uint16_t(climateOffset+8+int(std::floor(cyclePhase(ticks_)*4.0))%4));result.push_back(uint16_t(climateOffset+12+std::min(4,int(std::floor((climateFertility_[row]-.35)*5.0)))));}return result;
}
bool NativeSimulation::attemptMove(Organism& organism){const auto [dx,dy]=directions[organism.direction];if(!isClear(organism,organism.x+dx,organism.y+dy,organism.rotation))return false;clearBody(organism);organism.x+=dx;organism.y+=dy;placeBody(organism);return true;}
bool NativeSimulation::attemptRotate(Organism& organism){const int rotation=int(std::floor(random_()*4.0));if(!isClear(organism,organism.x,organism.y,rotation))return false;clearBody(organism);organism.rotation=rotation;organism.direction=int(std::floor(random_()*4.0));organism.moveCount=0;placeBody(organism);return true;}
void NativeSimulation::mutate(Organism& organism) {
  const int choice=int(std::floor(random_()*100.0));
  constexpr std::array<CellType,7> types{CellType::PlantMouth,CellType::ScavengerMouth,CellType::MineralMouth,CellType::Producer,CellType::Mover,CellType::Killer,CellType::Armor};
  const auto randomType=[&]{return types[int(std::floor(random_()*types.size()))];};
  const auto mineralCost=[](CellType type){return type==CellType::Killer?250:type==CellType::Armor?180:0;};
  if(choice<=33){auto base=organism.cells[int(std::floor(random_()*organism.cells.size()))];constexpr std::array<std::pair<int,int>,8> offsets{{{0,-1},{0,1},{-1,0},{1,0},{-1,-1},{1,1},{-1,1},{1,-1}}};const auto [dx,dy]=offsets[int(std::floor(random_()*8.0))];const auto type=randomType();const int cost=mineralCost(type);if(organism.minerals>=cost&&std::none_of(organism.cells.begin(),organism.cells.end(),[&](const BodyCell& cell){return cell.x==base.x+dx&&cell.y==base.y+dy;})){organism.minerals-=cost;organism.cells.push_back({type,base.x+dx,base.y+dy,type==CellType::Killer?KillerDurability:type==CellType::Armor?ArmorDurability:0});organism.mutations.push_back(std::string("Added ")+cellName(type)+" cell");++organism.birthDistance;refreshCapabilities(organism);}}
  else if(choice<=66){auto& cell=organism.cells[int(std::floor(random_()*organism.cells.size()))];const auto before=cell.type;const auto type=randomType();const int cost=mineralCost(type);if(organism.minerals<cost)return;organism.minerals-=cost;cell={type,cell.x,cell.y,type==CellType::Killer?KillerDurability:type==CellType::Armor?ArmorDurability:0};organism.mutations.push_back(std::string("Changed ")+cellName(before)+" to "+cellName(type));refreshCapabilities(organism);}
  else if(organism.cells.size()>1){const size_t index=size_t(std::floor(random_()*organism.cells.size()));if(organism.cells[index].x||organism.cells[index].y){organism.mutations.push_back(std::string("Removed ")+cellName(organism.cells[index].type)+" cell");organism.cells.erase(organism.cells.begin()+index);}refreshCapabilities(organism);}
  if(organism.isMover&&random_()*100.0<=10){organism.moveRange=std::max(1,organism.moveRange+int(std::floor(random_()*4.0))-2);organism.mutations.push_back("Movement range changed to "+std::to_string(organism.moveRange));}
  if(random_()*100.0<=10){organism.birthDistance=std::max(1,organism.birthDistance+int(std::floor(random_()*5.0))-2);organism.mutations.push_back("Birth distance changed to "+std::to_string(organism.birthDistance));}
  if(organism.isMover&&random_()<.1){const auto perception=clampPerception(organism.perceptionRadius+(random_()<.5?-1:1),organism.senseChannels);organism.perceptionRadius=perception.radius;organism.senseChannels=perception.channels;organism.neuralCost=perception.cost;organism.mutations.push_back("Perception radius changed to "+std::to_string(perception.radius));}
  if(organism.isMover&&random_()<.1){const int bit=1<<int(std::floor(random_()*8.0));const auto perception=clampPerception(organism.perceptionRadius,organism.senseChannels^bit);organism.perceptionRadius=perception.radius;organism.senseChannels=perception.channels;organism.neuralCost=perception.cost;organism.mutations.push_back("Sensory channel mask changed");}
}
void NativeSimulation::reproduce(Organism& parent) {
  const int growthMinerals=std::max(0,int(parent.cells.size())-3)*100;if(parent.minerals<growthMinerals)return;
  Organism child=parent;if(parent.brain)child.brain=std::make_shared<Nnue>(*parent.brain);child.id=nextId_++;child.parentId=parent.id;++child.generation;child.birthTick=ticks_;child.deathTick=-1;child.energy=int(std::floor(parent.energy*.35));child.minerals=int(std::floor(parent.minerals*.25));child.lifetime=child.damage=child.moveCount=0;child.direction=child.rotation=0;child.living=true;child.mutations.clear();child.lastFeatures.clear();child.lastAction=-1;
  child.rotation=int(std::floor(random_()*4.0));
  child.mutability=std::max(1,child.mutability+(random_()<=.5?1:-1));child.mutations.push_back("Body mutation rate changed to "+std::to_string(child.mutability)+"%");if(random_()*100.0<=parent.mutability)mutate(child);
  if(child.isMover&&child.brain&&random_()*100.0<=child.neuralMutability){child.brain->mutate(random_);child.mutations.push_back("NNUE weights mutated");}
  if(random_()<.1){child.neuralMutability=std::clamp(child.neuralMutability+(random_()<.5?-.5:.5),.5,50.0);child.mutations.push_back("Neural mutation rate changed");}
  const auto [dx,dy]=directions[int(std::floor(random_()*4.0))];const int offset=int(std::floor(random_()*3.0));child.x=parent.x+dx*(parent.birthDistance+offset);child.y=parent.y+dy*(parent.birthDistance+offset);
  const int transferredEnergy=child.energy,transferredMinerals=child.minerals;
  if(isClear(child,child.x,child.y,child.rotation)&&straightPath(child.x,child.y,parent.x,parent.y,parent)){parent.minerals-=growthMinerals;largest_=std::max(largest_,int(child.cells.size()));organisms_.push_back(std::move(child));slotById_[organisms_.back().id]=organisms_.size()-1;placeBody(organisms_.back());}
  parent.energy=std::max(0,parent.energy-transferredEnergy);parent.minerals=std::max(0,parent.minerals-transferredMinerals);
}
void NativeSimulation::die(Organism& organism){if(!organism.living)return;organism.deathTick=ticks_;++deadCount_;for(const auto& cell:organism.cells){const auto [rx,ry]=rotated(cell.x,cell.y,organism.rotation);const int index=safeIndex(organism.x+rx,organism.y+ry);if(index>=0&&owners[index]==organism.id){cells[index]=uint8_t(CellType::Empty);owners[index]=-1;world.resources[index]=uint8_t(ResourceType::Carrion);world.resourceAmount[index]=uint16_t(std::min(65535,int(world.resourceAmount[index])+200+organism.energy/std::max(1,int(organism.cells.size()))/20));}}organism.living=false;slotById_.erase(organism.id);}
void NativeSimulation::updateOrganism(Organism& organism) {
  if(!organism.living)return;++organism.lifetime;organism.energy-=int(organism.cells.size())*CellBaseCost;
  const int climateRow=std::clamp(organism.y,0,height_-1);const double temperature=climateTemperature_[climateRow];const int home=safeIndex(organism.x,organism.y);const bool desert=home>=0&&TerrainType(world.terrain[home])==TerrainType::Desert;
  const double discomfort=std::max(0.0,std::abs(temperature-.5)-.15);organism.energy=std::max(0,organism.energy-int(std::round(discomfort*30.0))-(desert?4:0));
  const double stressDelta=temperature>=.35&&temperature<=.65?-.004:.001+std::abs(temperature-(temperature<.35?.35:.65))*.012;organism.thermalStress=std::max(0.0,organism.thermalStress+stressDelta);
  if(organism.thermalStress>=1){organism.thermalStress-=1;if(mortalityEnabled_){++organism.damage;if(organism.damage>=int(organism.cells.size())){die(organism);return;}}}
  if(mortalityEnabled_&&organism.lifetime>int(organism.cells.size())*lifespan_){die(organism);return;}
  if(organism.energy<=0){organism.energy=0;if(mortalityEnabled_){++organism.damage;if(organism.damage>=int(organism.cells.size())){die(organism);return;}}}
  if(reproductionEnabled_&&organism.energy>=int(organism.cells.size())*6*EnergyScale)reproduce(organism);
  if(organism.isConsumer||organism.isProducer||organism.isAttacker)for(auto& local:organism.cells){const auto [rx,ry]=rotated(local.x,local.y,organism.rotation);const int x=organism.x+rx,y=organism.y+ry;if(local.type==CellType::Mouth||local.type==CellType::PlantMouth||local.type==CellType::ScavengerMouth||local.type==CellType::MineralMouth)eat(organism,local.type,x,y);else if(local.type==CellType::Producer)produce(organism,x,y);else if(local.type==CellType::Killer)attack(organism,local,x,y);}
  if(!organism.living||!organism.isMover||!organism.brain)return;const int interval=std::clamp(int(std::ceil(organism.neuralCost/16.0)),1,16);if((ticks_+organism.id)%interval==0){++nnueEvaluations_;organism.energy=std::max(0,organism.energy-int(std::ceil(organism.neuralCost/20.0)));organism.lastFeatures=features(organism);organism.lastOutputs=organism.brain->evaluate(organism.lastFeatures);int best=0;for(int action=1;action<OutputSize;++action)if(organism.lastOutputs[action]>organism.lastOutputs[best])best=action;organism.lastAction=best;organism.direction=best;}if(organism.direction>=4)return;++organism.moveCount;organism.energy=std::max(0,organism.energy-MoveCost);attemptMove(organism);if(organism.moveCount>organism.moveRange)attemptRotate(organism);
}
void NativeSimulation::step(int count) {
  for(int iteration=0;iteration<count;++iteration){
    ++ticks_;updateClimateCache();const size_t activeCount=organisms_.size();
    for(size_t index=0;index<activeCount;++index){
      const size_t before=organisms_.size();updateOrganism(organisms_[index]);
      for(size_t childIndex=before;childIndex<organisms_.size();++childIndex){
        int ancestorId=organisms_[childIndex].parentId;
        while(ancestorId>=0){const auto ancestor=std::find_if(organisms_.begin(),organisms_.end(),[&](const Organism& value){return value.id==ancestorId;});if(ancestor==organisms_.end())break;++ancestor->totalDescendants;ancestorId=ancestor->parentId;}
      }
    }
    if(deadCount_>lineageLimit_)pruneLineage();int living=0;for(const auto& organism:organisms_)living+=organism.living;record_=std::max(record_,living);
  }
}
NativeMetrics NativeSimulation::metrics() const {
  double energy=0,mutation=0,stress=0;int living=0,plantEaters=0,scavengers=0,mineralEaters=0,predators=0;
  size_t memory=cells.capacity()*sizeof(uint8_t)+owners.capacity()*sizeof(int32_t)+world.terrain.capacity()*sizeof(uint8_t)+world.resources.capacity()*sizeof(uint8_t)+world.resourceAmount.capacity()*sizeof(uint16_t)+organisms_.capacity()*sizeof(Organism);
  for(const auto& organism:organisms_)if(organism.living){energy+=organism.energy;mutation+=organism.mutability;stress+=organism.thermalStress;++living;memory+=organism.cells.capacity()*sizeof(BodyCell)+organism.mutations.capacity()*sizeof(std::string)+organism.lastFeatures.capacity()*sizeof(uint16_t);for(const auto& cell:organism.cells){if(cell.type==CellType::Mouth||cell.type==CellType::PlantMouth)++plantEaters;else if(cell.type==CellType::ScavengerMouth)++scavengers;else if(cell.type==CellType::MineralMouth)++mineralEaters;else if(cell.type==CellType::Killer)++predators;}}
  const auto climate=climateAt(height_/2,height_,ticks_);return{living,record_,resets_,ticks_,largest_,living?energy/living/EnergyScale:0,living?mutation/living:0,living?stress/living:0,climate.temperature,climate.fertility,plantEaters,scavengers,mineralEaters,predators,nnueEvaluations_,memory};
}
}
