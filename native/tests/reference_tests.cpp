#include "reference.hpp"
#include "nnue.hpp"
#include "simulation.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#undef near
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace life {
struct NativeSimulationTestAccess {
  static void atmosphere(NativeSimulation& simulation,double oxygen,double carbonDioxide){simulation.atmosphericOxygen_=oxygen;simulation.carbonDioxide_=carbonDioxide;}
  static void tick(NativeSimulation& simulation,int tick){simulation.ticks_=tick;}
  static void respiratoryMortality(NativeSimulation& simulation){simulation.processRespiratoryMortality();}
  static void update(NativeSimulation& simulation,Organism& organism){simulation.updateOrganism(organism);}
  static std::vector<Organism>& organisms(NativeSimulation& simulation){return simulation.organisms_;}
};
}

namespace {
int failures = 0;
void check(bool condition, const char* message) { if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; } }
void near(double actual, double expected, double tolerance, const char* message) { if(std::abs(actual-expected)>tolerance){std::cerr<<"FAIL: "<<message<<" (actual "<<actual<<", expected "<<expected<<")\n";++failures;} }
std::string stateHash(const life::NativeSimulation& simulation){BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};DWORD objectBytes=0,resultBytes=0;BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0);BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectBytes),sizeof(objectBytes),&resultBytes,0);std::vector<UCHAR> object(objectBytes),result(32);BCryptCreateHash(algorithm,&hash,object.data(),DWORD(object.size()),nullptr,0,0);const auto append=[&](const void* data,size_t bytes){BCryptHashData(hash,const_cast<PUCHAR>(static_cast<const UCHAR*>(data)),DWORD(bytes),0);};append(simulation.cells.data(),simulation.cells.size());append(simulation.owners.data(),simulation.owners.size()*sizeof(int32_t));append(simulation.world.terrain.data(),simulation.world.terrain.size());append(simulation.world.resources.data(),simulation.world.resources.size());append(simulation.world.resourceAmount.data(),simulation.world.resourceAmount.size()*sizeof(uint16_t));BCryptFinishHash(hash,result.data(),DWORD(result.size()),0);BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);std::ostringstream encoded;for(const auto byte:result)encoded<<std::hex<<std::setw(2)<<std::setfill('0')<<int(byte);return encoded.str();}
}

int main() {
  static_assert(int(life::CellType::Inert) == 11);
  static_assert(int(life::Action::Wait) == 4);
  static_assert(life::InputSize == life::PositionCount * life::FeatureCategories + 25);
  life::Mulberry32 random(334462);
  const double expected[] = {0.023944327142089605, 0.84153357916511595, 0.60139716230332851, 0.56424767896533012, 0.24601307534612715, 0.67428731429390609, 0.79913979861885309, 0.17321427632123232};
  for (double value : expected) near(random(), value, 0.0, "Mulberry32 sequence differs from TypeScript");
  const auto climate = life::climateAt(30, 60, 100);
  near(climate.phase, 0.004166666666666667, 1e-16, "climate phase mismatch");
  near(climate.temperature, 0.499778161455018, 1e-15, "climate temperature mismatch");
  near(climate.fertility, 0.8497781614550179, 1e-15, "climate fertility mismatch");
  check(climate.season == 0, "season mismatch");
  const auto first = life::generateWorld(96, 60, 524114809), second = life::generateWorld(96, 60, 524114809);
  check(first.terrain == second.terrain, "terrain generation is not deterministic");
  check(first.resources == second.resources, "resource generation is not deterministic");
  check(first.resourceAmount == second.resourceAmount, "resource amounts are not deterministic");
  check(first.terrain != life::generateWorld(96, 60, 524114810).terrain, "world seed does not affect terrain");
  const auto defaultPerception = life::clampPerception(2, life::DefaultSenseChannels);
  check(defaultPerception.radius == 2 && defaultPerception.channels == 239 && defaultPerception.cost == 168, "default perception mismatch");
  const auto clampedPerception = life::clampPerception(4, 255);
  check(clampedPerception.radius == 3 && clampedPerception.channels == 255 && clampedPerception.cost == 384, "perception budget mismatch");
  life::Mulberry32 neuralRandom(334462u ^ 0x4e4e5545u);
  life::Nnue brain(neuralRandom);
  const std::array<uint16_t, 7> features{0, 21, 65, 203, 1601, 1606, 1612};
  const auto& outputs = brain.evaluate(features);
  const double expectedOutputs[] = {0.023251008242368698, -0.02539653144776821, 0.07016514986753464, -0.05609821155667305, 0.008624186739325523};
  for (int index = 0; index < life::OutputSize; ++index) near(outputs[index], expectedOutputs[index], 1e-7, "NNUE output mismatch");
  check(brain.action(features) == 2, "NNUE action mismatch");
  life::NativeSimulation editable(32,20,.2,500,41,91);
  int editableIndex=-1;
  for(int index=0;index<int(editable.owners.size());++index)if(editable.owners[index]<0&&life::TerrainType(editable.world.terrain[index])!=life::TerrainType::Water&&life::TerrainType(editable.world.terrain[index])!=life::TerrainType::Mountain){editableIndex=index;break;}
  check(editableIndex>=0,"editable terrain cell unavailable");
  if(editableIndex>=0){const int x=editableIndex%32,y=editableIndex/32;check(editable.paintTerrain(x,y,life::TerrainType::Desert),"terrain painting failed");check(life::TerrainType(editable.world.terrain[editableIndex])==life::TerrainType::Desert,"terrain paint did not persist");check(editable.paintResource(x,y,life::ResourceType::Mineral,777),"resource painting failed");check(life::ResourceType(editable.world.resources[editableIndex])==life::ResourceType::Mineral&&editable.world.resourceAmount[editableIndex]==777,"resource paint did not persist");}
  check(editable.paintTerrain(16,10,life::TerrainType::Mountain),"impassable terrain paint failed");check(editable.organismAt(16,10)==nullptr,"impassable terrain did not kill occupying organism");check(editable.paintResource(16,10,life::ResourceType::Carrion,321)&&editable.world.resourceAmount[10*32+16]==321,"resource painting on barrier diverged from reference");
  const auto priorTerrain=editable.world.terrain;editable.regenerate(42);check(editable.metrics().ticks==0&&editable.metrics().organisms==1,"regeneration did not reset simulation");check(editable.world.terrain!=priorTerrain,"regeneration seed did not replace terrain");check(editable.organismAt(16,10)!=nullptr,"organism inspection lookup failed");
  life::NativeSimulation benchmarkPopulation(256,160,.2,500,73,19);check(benchmarkPopulation.seedBenchmarkMovers(1'000)==1'000,"benchmark population did not reach 1,000 rich organisms");check(benchmarkPopulation.metrics().organisms==1'000&&benchmarkPopulation.metrics().predators>0&&benchmarkPopulation.metrics().plantEaters>0&&benchmarkPopulation.metrics().scavengers>0&&benchmarkPopulation.metrics().mineralEaters>0,"rich benchmark ecology mix mismatch");
  {
    life::NativeSimulation health(64,40,.0,50'000,73,19);check(health.seedBenchmarkMovers(12)==12,"health test population setup failed");auto& organisms=life::NativeSimulationTestAccess::organisms(health);for(auto& organism:organisms){organism.respiratoryRisk=.10;organism.hypoxiaStress=.80;}organisms[5].respiratoryRisk=.90;const int highestRiskId=organisms[5].id;life::NativeSimulationTestAccess::tick(health,10);life::NativeSimulationTestAccess::respiratoryMortality(health);check(!organisms[5].living&&organisms[5].deathCause=="Respiratory failure","highest respiratory risk did not die first");check(health.respiratoryDeathCount()==1,"first respiratory death was not counted");life::NativeSimulationTestAccess::tick(health,19);life::NativeSimulationTestAccess::respiratoryMortality(health);check(health.respiratoryDeathCount()==1,"respiratory deaths were not limited to one per ten ticks");life::NativeSimulationTestAccess::tick(health,20);life::NativeSimulationTestAccess::respiratoryMortality(health);check(health.respiratoryDeathCount()==2,"eligible respiratory death did not resume after ten ticks");std::vector<int> respiratoryDeathTicks;for(const auto& organism:organisms)if(organism.deathCause=="Respiratory failure")respiratoryDeathTicks.push_back(organism.deathTick);std::sort(respiratoryDeathTicks.begin(),respiratoryDeathTicks.end());for(size_t index=1;index<respiratoryDeathTicks.size();++index)check(respiratoryDeathTicks[index]-respiratoryDeathTicks[index-1]>=10,"respiratory death spacing fell below ten ticks");check(highestRiskId>0,"invalid respiratory victim id");
  }
  {
    life::NativeSimulation health(32,20,.0,50'000,41,91);auto& founder=life::NativeSimulationTestAccess::organisms(health).front();life::NativeSimulationTestAccess::atmosphere(health,.06,.15);life::NativeSimulationTestAccess::update(health,founder);const double stressed=founder.hypoxiaStress;check(stressed>0&&founder.living,"respiratory stress did not accumulate gradually");life::NativeSimulationTestAccess::atmosphere(health,.21,.0004);life::NativeSimulationTestAccess::update(health,founder);check(founder.hypoxiaStress<stressed&&health.respiratoryDeathCount()==0,"healthy atmosphere did not recover stress without respiratory death");
  }
  life::NativeSimulation simulation(96,60,.2,500,524114809,334462);
  check(stateHash(simulation)=="343b32ea4840d95cab47b17cdeedd776b582c30c053fcd4cab74e0fe4f75ca88","native tick-0 whole-state hash mismatch");
  check(simulation.metrics().organisms==1&&simulation.metrics().averageEnergy==12.0,"native reset state mismatch");
  const auto founder=simulation.inspect(1);check(founder.has_value()&&founder->generation==0&&founder->ancestors.empty()&&founder->descendants.empty()&&founder->diet=="Plant","native founder inspection mismatch");
  simulation.step();
  check(stateHash(simulation)=="8dac5314a81b1de7799ab2091dd9754a99c1c7f3d6b35f05ae1794db1f5b1de2","native tick-1 whole-state hash mismatch");
  check(simulation.metrics().ticks==1,"native simulation clock mismatch");
  near(simulation.metrics().averageEnergy,11.989,1e-12,"native first-tick energy mismatch");
  simulation.step(9);
  check(stateHash(simulation)=="53372fe6b4c9faf497fba2f1492145c889e8f4eb671787a5a60c69947b6047eb","native tick-10 whole-state hash mismatch");
  check(simulation.metrics().organisms==1,"native tick-10 population mismatch");
  near(simulation.metrics().averageEnergy,15.925,1e-12,"native tick-10 energy mismatch");
  simulation.step(90);
  check(stateHash(simulation)=="b3eff30756ba7a169bda6653b88b6b154157d8c2807db063c1535547ec9eb5c1","native tick-100 whole-state hash mismatch");
  if(simulation.metrics().organisms!=39){std::cerr<<"FAIL: native tick-100 population mismatch (actual "<<simulation.metrics().organisms<<", expected 39)\n";++failures;}
  near(simulation.metrics().averageEnergy,13.923538461538461,1e-12,"native tick-100 energy mismatch");
  const auto evolvedFounder=simulation.inspect(1);check(evolvedFounder.has_value()&&evolvedFounder->totalDescendants>0,"native lineage descendants were not recorded");
  check(evolvedFounder.has_value()&&evolvedFounder->oxygenTolerance>=.08&&evolvedFounder->oxygenTolerance<=.16&&evolvedFounder->stressRecovery>=.002&&evolvedFounder->frailty>=.35,"observatory physiology fields are invalid");
  const auto descendant=std::find_if(simulation.organisms().begin(),simulation.organisms().end(),[](const life::Organism& organism){return organism.parentId>=0;});if(descendant!=simulation.organisms().end()){const auto childInspection=simulation.inspect(descendant->id);check(childInspection.has_value()&&!childInspection->ancestors.empty()&&!childInspection->mutations.empty(),"native descendant ancestry or mutations missing");}
  const bool hasHealthMutation=std::any_of(simulation.organisms().begin(),simulation.organisms().end(),[](const life::Organism& organism){return std::any_of(organism.mutations.begin(),organism.mutations.end(),[](const std::string& mutation){return mutation.starts_with("Oxygen tolerance")||mutation.starts_with("Respiratory recovery")||mutation.starts_with("Frailty");});});check(hasHealthMutation,"heritable physiology mutations were not recorded in lineage");
  simulation.step(900);
  if(simulation.metrics().organisms!=768){std::cerr<<"FAIL: native tick-1000 population mismatch (actual "<<simulation.metrics().organisms<<", expected 768)\n";++failures;}
  simulation.step(1'000);const auto toroidalMetrics=simulation.metrics();check(toroidalMetrics.organisms>0&&std::isfinite(toroidalMetrics.averageEnergy),"toroidal long-run ecology became invalid");check(toroidalMetrics.awakeRegions>0&&toroidalMetrics.awakeRegions+toroidalMetrics.sleepingRegions==6,"region sleep accounting mismatch");
  life::NativeSimulation replay(96,60,.2,500,524114809,334462);replay.step(2'000);check(stateHash(replay)==stateHash(simulation),"toroidal simulation replay is not deterministic");check(replay.metrics().organisms==simulation.metrics().organisms&&replay.metrics().nnueEvaluations==simulation.metrics().nnueEvaluations,"toroidal replay metrics diverged");
  replay.takeDirtyTiles();check(replay.takeDirtyTiles().empty(),"dirty tiles were not drained");check(replay.paintResource(0,0,life::ResourceType::Plant,50)&&replay.takeDirtyTiles().size()==1,"resource edit did not dirty exactly one tile");
  life::NativeSimulation prunedLineage(96,60,.2,500,524114809,334462,5);prunedLineage.select(1);prunedLineage.step(2'000);check(prunedLineage.metrics().organisms==simulation.metrics().organisms,"lineage pruning changed living population");near(prunedLineage.metrics().averageEnergy,simulation.metrics().averageEnergy,1e-12,"lineage pruning changed simulation energy");check(prunedLineage.lineageRecordCount()<simulation.lineageRecordCount(),"lineage pruning did not remove dead leaves");check(prunedLineage.inspect(1).has_value(),"selected lineage record was pruned");
  if (!failures) std::cout << "Native deterministic ecology and spatial contract passed.\n";
  return failures ? 1 : 0;
}
