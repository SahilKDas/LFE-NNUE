#include "simulation.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace life {
namespace { constexpr std::array<std::pair<int,int>,4> directions{{{0,-1},{0,1},{-1,0},{1,0}}}; }

NativeSimulation::NativeSimulation(int width, int height, double foodChance, int lifespan, uint32_t worldSeed, uint32_t randomSeed)
  : width_(width), height_(height), worldSeed_(worldSeed), random_(randomSeed), foodChance_(foodChance), lifespan_(lifespan),
    world(generateWorld(width, height, worldSeed)), cells(width * height), owners(width * height, -1) { organisms_.reserve(100'000);reset(); }

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
  std::fill(cells.begin(),cells.end(),uint8_t(CellType::Empty));std::fill(owners.begin(),owners.end(),-1);organisms_.clear();ticks_=0;++resets_;
  Organism organism;organism.id=nextId_++;organism.x=width_/2;organism.y=height_/2;
  organism.cells={{CellType::Mouth,0,0,0},{CellType::Producer,-1,-1,0},{CellType::Producer,1,1,0}};
  refreshCapabilities(organism);largest_=std::max(largest_,int(organism.cells.size()));organisms_.push_back(std::move(organism));placeBody(organisms_.back());
}
void NativeSimulation::eat(Organism& organism, CellType mouth, int x, int y) {
  for(const auto [dx,dy]:directions){const int index=safeIndex(x+dx,y+dy);if(index<0)continue;const auto resource=ResourceType(world.resources[index]);
    const bool compatible=((mouth==CellType::Mouth||mouth==CellType::PlantMouth)&&resource==ResourceType::Plant)||(mouth==CellType::ScavengerMouth&&resource==ResourceType::Carrion)||(mouth==CellType::MineralMouth&&resource==ResourceType::Mineral);
    if(compatible&&world.resourceAmount[index]>0){const int consumed=std::min(100,int(world.resourceAmount[index]));world.resourceAmount[index]-=uint16_t(consumed);if(resource==ResourceType::Mineral)organism.minerals+=consumed;else organism.energy+=int(std::floor(consumed/100.0*(resource==ResourceType::Plant?PlantEnergy:CarrionEnergy)));if(!world.resourceAmount[index])world.resources[index]=uint8_t(ResourceType::None);}
    else if(CellType(cells[index])==CellType::Food){cells[index]=uint8_t(CellType::Empty);owners[index]=-1;organism.energy+=PlantEnergy;}
  }
}
void NativeSimulation::produce(Organism& organism, int x, int y) {
  const int origin=safeIndex(x,y);const auto terrain=origin>=0?TerrainType(world.terrain[origin]):TerrainType::Water;const auto climate=climateAt(std::clamp(y,0,height_-1),height_,ticks_);
  const double productivity=terrain==TerrainType::Fertile?1.35:terrain==TerrainType::Desert?.3:terrain==TerrainType::Plains?1.0:0.0;
  if((organism.isMover)||organism.energy<ProduceCost||random_()>=foodChance_*climate.fertility*productivity)return;
  const auto [dx,dy]=directions[int(std::floor(random_()*4.0))];const int index=safeIndex(x+dx,y+dy);if(index>=0&&TerrainType(world.terrain[index])!=TerrainType::Water&&TerrainType(world.terrain[index])!=TerrainType::Mountain){organism.energy-=ProduceCost;world.resources[index]=uint8_t(ResourceType::Plant);world.resourceAmount[index]=uint16_t(std::min(65535,int(world.resourceAmount[index])+50));}
}
bool NativeSimulation::isClear(const Organism& organism,int x,int y,int rotation) const {
  for(const auto& cell:organism.cells){const auto [rx,ry]=rotated(cell.x,cell.y,rotation);const int index=safeIndex(x+rx,y+ry);if(index<0)return false;const auto terrain=TerrainType(world.terrain[index]);if(terrain==TerrainType::Water||terrain==TerrainType::Mountain)return false;if(owners[index]!=organism.id&&CellType(cells[index])!=CellType::Empty)return false;}return true;
}
bool NativeSimulation::straightPath(int x1,int y1,int x2,int y2,const Organism& parent) const {
  const auto passable=[&](int x,int y){const int index=safeIndex(x,y);if(index<0)return false;const auto terrain=TerrainType(world.terrain[index]);return terrain!=TerrainType::Water&&terrain!=TerrainType::Mountain&&(CellType(cells[index])==CellType::Empty||CellType(cells[index])==CellType::Food||owners[index]==parent.id);};
  if(x1==x2){for(int y=std::min(y1,y2);y<std::max(y1,y2);++y)if(!passable(x1,y))return false;return true;}for(int x=std::min(x1,x2);x<std::max(x1,x2);++x)if(!passable(x,y1))return false;return true;
}
std::vector<uint16_t> NativeSimulation::features(const Organism& organism) const {
  std::vector<uint16_t> result;result.reserve(64);int square=0;
  for(int ring=1;ring<=MaximumSensorRadius;++ring)for(int dy=-ring;dy<=ring;++dy)for(int dx=-ring;dx<=ring;++dx)if((dx||dy)&&std::max(std::abs(dx),std::abs(dy))==ring){const int current=square++;if(std::max(std::abs(dx),std::abs(dy))>organism.perceptionRadius)continue;const int index=safeIndex(organism.x+dx,organism.y+dy);int category=6,owner=-1;if(index>=0){const auto type=CellType(cells[index]);owner=owners[index];const Organism* neighbor=nullptr;if(owner>=0&&owner!=organism.id&&size_t(owner-1)<organisms_.size()&&organisms_[owner-1].id==owner)neighbor=&organisms_[owner-1];category=owner==organism.id?3:type==CellType::Killer?5:neighbor&&neighbor->isMover?7+neighbor->direction:owner>=0?4:type==CellType::Food?1:type==CellType::Wall?2:0;}
    const int mask=organism.senseChannels;const bool enabled=(category==5||category==6)?mask&Danger:category>=7&&category<=10?mask&Heading:category==1?mask&Resources:mask&Occupancy;if(enabled)result.push_back(uint16_t(current*FeatureCategories+category));
    if(mask&Resources){const auto resource=index>=0?ResourceType(world.resources[index]):ResourceType::None;if(resource==ResourceType::Carrion)result.push_back(uint16_t(current*FeatureCategories+12));else if(resource==ResourceType::Mineral)result.push_back(uint16_t(current*FeatureCategories+13));else if(resource==ResourceType::Plant&&category!=1)result.push_back(uint16_t(current*FeatureCategories+1));}
    if((mask&Terrain)&&index>=0)result.push_back(uint16_t(current*FeatureCategories+15+world.terrain[index]));
  }
  constexpr int stateOffset=PositionCount*FeatureCategories,climateOffset=stateOffset+8;const int mask=organism.senseChannels;
  if(mask&Internal){result.push_back(uint16_t(stateOffset+(organism.energy>=int(organism.cells.size())*6*EnergyScale?1:0)));result.push_back(uint16_t(stateOffset+2+(organism.damage>0?1:0)));result.push_back(uint16_t(stateOffset+4+organism.direction));}
  const auto climate=climateAt(std::clamp(organism.y,0,height_-1),height_,ticks_);if(mask&Temperature){result.push_back(uint16_t(climateOffset+climate.temperatureBand));result.push_back(uint16_t(climateOffset+5+climate.gradient+1));}if(mask&Fertility){result.push_back(uint16_t(climateOffset+8+int(std::floor(cyclePhase(ticks_)*4.0))%4));result.push_back(uint16_t(climateOffset+12+std::min(4,int(std::floor((climate.fertility-.35)*5.0)))));}return result;
}
bool NativeSimulation::attemptMove(Organism& organism){const auto [dx,dy]=directions[organism.direction];if(!isClear(organism,organism.x+dx,organism.y+dy,organism.rotation))return false;clearBody(organism);organism.x+=dx;organism.y+=dy;placeBody(organism);return true;}
bool NativeSimulation::attemptRotate(Organism& organism){const int rotation=int(std::floor(random_()*4.0));if(!isClear(organism,organism.x,organism.y,rotation))return false;clearBody(organism);organism.rotation=rotation;organism.direction=int(std::floor(random_()*4.0));organism.moveCount=0;placeBody(organism);return true;}
void NativeSimulation::mutate(Organism& organism) {
  const int choice=int(std::floor(random_()*100.0));
  constexpr std::array<CellType,7> types{CellType::PlantMouth,CellType::ScavengerMouth,CellType::MineralMouth,CellType::Producer,CellType::Mover,CellType::Killer,CellType::Armor};
  const auto randomType=[&]{return types[int(std::floor(random_()*types.size()))];};
  const auto mineralCost=[](CellType type){return type==CellType::Killer?250:type==CellType::Armor?180:0;};
  if(choice<=33){auto base=organism.cells[int(std::floor(random_()*organism.cells.size()))];constexpr std::array<std::pair<int,int>,8> offsets{{{0,-1},{0,1},{-1,0},{1,0},{-1,-1},{1,1},{-1,1},{1,-1}}};const auto [dx,dy]=offsets[int(std::floor(random_()*8.0))];const auto type=randomType();const int cost=mineralCost(type);if(organism.minerals>=cost&&std::none_of(organism.cells.begin(),organism.cells.end(),[&](const BodyCell& cell){return cell.x==base.x+dx&&cell.y==base.y+dy;})){organism.minerals-=cost;organism.cells.push_back({type,base.x+dx,base.y+dy,type==CellType::Killer?KillerDurability:type==CellType::Armor?ArmorDurability:0});++organism.birthDistance;refreshCapabilities(organism);}}
  else if(choice<=66){auto& cell=organism.cells[int(std::floor(random_()*organism.cells.size()))];const auto type=randomType();const int cost=mineralCost(type);if(organism.minerals<cost)return;organism.minerals-=cost;cell={type,cell.x,cell.y,type==CellType::Killer?KillerDurability:type==CellType::Armor?ArmorDurability:0};refreshCapabilities(organism);}
  else if(organism.cells.size()>1){const size_t index=size_t(std::floor(random_()*organism.cells.size()));if(organism.cells[index].x||organism.cells[index].y)organism.cells.erase(organism.cells.begin()+index);refreshCapabilities(organism);}
  if(organism.isMover&&random_()*100.0<=10){organism.moveRange=std::max(1,organism.moveRange+int(std::floor(random_()*4.0))-2);}
  if(random_()*100.0<=10)organism.birthDistance=std::max(1,organism.birthDistance+int(std::floor(random_()*5.0))-2);
  if(organism.isMover&&random_()<.1){const auto perception=clampPerception(organism.perceptionRadius+(random_()<.5?-1:1),organism.senseChannels);organism.perceptionRadius=perception.radius;organism.senseChannels=perception.channels;organism.neuralCost=perception.cost;}
  if(organism.isMover&&random_()<.1){const int bit=1<<int(std::floor(random_()*8.0));const auto perception=clampPerception(organism.perceptionRadius,organism.senseChannels^bit);organism.perceptionRadius=perception.radius;organism.senseChannels=perception.channels;organism.neuralCost=perception.cost;}
}
void NativeSimulation::reproduce(Organism& parent) {
  const int growthMinerals=std::max(0,int(parent.cells.size())-3)*100;if(parent.minerals<growthMinerals)return;
  Organism child=parent;if(parent.brain)child.brain=std::make_shared<Nnue>(*parent.brain);child.id=nextId_++;child.parentId=parent.id;++child.generation;child.birthTick=ticks_;child.energy=int(std::floor(parent.energy*.35));child.minerals=int(std::floor(parent.minerals*.25));child.lifetime=child.damage=child.moveCount=0;child.direction=child.rotation=0;child.living=true;
  child.rotation=int(std::floor(random_()*4.0));
  child.mutability=std::max(1,child.mutability+(random_()<=.5?1:-1));if(random_()*100.0<=parent.mutability)mutate(child);
  if(child.isMover&&child.brain&&random_()*100.0<=child.neuralMutability)child.brain->mutate(random_);
  if(random_()<.1)child.neuralMutability=std::clamp(child.neuralMutability+(random_()<.5?-.5:.5),.5,50.0);
  const auto [dx,dy]=directions[int(std::floor(random_()*4.0))];const int offset=int(std::floor(random_()*3.0));child.x=parent.x+dx*(parent.birthDistance+offset);child.y=parent.y+dy*(parent.birthDistance+offset);
  const int transferredEnergy=child.energy,transferredMinerals=child.minerals;
  if(isClear(child,child.x,child.y,child.rotation)&&straightPath(child.x,child.y,parent.x,parent.y,parent)){parent.minerals-=growthMinerals;largest_=std::max(largest_,int(child.cells.size()));organisms_.push_back(std::move(child));placeBody(organisms_.back());}
  parent.energy=std::max(0,parent.energy-transferredEnergy);parent.minerals=std::max(0,parent.minerals-transferredMinerals);
}
void NativeSimulation::die(Organism& organism){if(!organism.living)return;for(const auto& cell:organism.cells){const auto [rx,ry]=rotated(cell.x,cell.y,organism.rotation);const int index=safeIndex(organism.x+rx,organism.y+ry);if(index>=0&&owners[index]==organism.id){cells[index]=uint8_t(CellType::Empty);owners[index]=-1;world.resources[index]=uint8_t(ResourceType::Carrion);world.resourceAmount[index]=uint16_t(std::min(65535,int(world.resourceAmount[index])+200+organism.energy/std::max(1,int(organism.cells.size()))/20));}}organism.living=false;}
void NativeSimulation::updateOrganism(Organism& organism) {
  if(!organism.living)return;++organism.lifetime;organism.energy-=int(organism.cells.size())*CellBaseCost;
  const auto climate=climateAt(std::clamp(organism.y,0,height_-1),height_,ticks_);const int home=safeIndex(organism.x,organism.y);const bool desert=home>=0&&TerrainType(world.terrain[home])==TerrainType::Desert;
  const double discomfort=std::max(0.0,std::abs(climate.temperature-.5)-.15);organism.energy=std::max(0,organism.energy-int(std::round(discomfort*30.0))-(desert?4:0));
  const double stressDelta=climate.temperature>=.35&&climate.temperature<=.65?-.004:.001+std::abs(climate.temperature-(climate.temperature<.35?.35:.65))*.012;organism.thermalStress=std::max(0.0,organism.thermalStress+stressDelta);
  if(organism.thermalStress>=1){organism.thermalStress-=1;++organism.damage;if(organism.damage>=int(organism.cells.size())){die(organism);return;}}
  if(organism.lifetime>int(organism.cells.size())*lifespan_){die(organism);return;}
  if(organism.energy<=0){organism.energy=0;++organism.damage;if(organism.damage>=int(organism.cells.size())){die(organism);return;}}
  if(organism.energy>=int(organism.cells.size())*6*EnergyScale)reproduce(organism);
  if(organism.isConsumer||organism.isProducer||organism.isAttacker)for(auto& local:organism.cells){const auto [rx,ry]=rotated(local.x,local.y,organism.rotation);const int x=organism.x+rx,y=organism.y+ry;if(local.type==CellType::Mouth||local.type==CellType::PlantMouth||local.type==CellType::ScavengerMouth||local.type==CellType::MineralMouth)eat(organism,local.type,x,y);else if(local.type==CellType::Producer)produce(organism,x,y);}
  if(!organism.living||!organism.isMover||!organism.brain)return;const int interval=std::clamp(int(std::ceil(organism.neuralCost/16.0)),1,16);if((ticks_+organism.id)%interval==0){++nnueEvaluations_;organism.energy=std::max(0,organism.energy-int(std::ceil(organism.neuralCost/20.0)));const auto sensed=features(organism);organism.direction=organism.brain->action(sensed);}if(organism.direction>=4)return;++organism.moveCount;organism.energy=std::max(0,organism.energy-MoveCost);attemptMove(organism);if(organism.moveCount>organism.moveRange)attemptRotate(organism);
}
void NativeSimulation::step(int count) { for(int iteration=0;iteration<count;++iteration){++ticks_;const size_t activeCount=organisms_.size();for(size_t index=0;index<activeCount;++index)updateOrganism(organisms_[index]);record_=std::max(record_,int(organisms_.size()));} }
NativeMetrics NativeSimulation::metrics() const { double energy=0;int living=0;for(const auto& organism:organisms_)if(organism.living){energy+=organism.energy;++living;}const auto climate=climateAt(height_/2,height_,ticks_);return{living,record_,resets_,ticks_,largest_,living?energy/living/EnergyScale:0,climate.temperature,climate.fertility,nnueEvaluations_}; }
}
