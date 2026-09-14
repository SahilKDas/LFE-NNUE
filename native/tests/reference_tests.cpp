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
  life::NativeSimulation benchmarkPopulation(160,100,.2,500,73,19);check(benchmarkPopulation.seedBenchmarkMovers(10'000)==10'000,"benchmark population did not reach 10,000 movers");check(benchmarkPopulation.metrics().organisms==10'000,"benchmark mover metrics mismatch");
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
  const auto descendant=std::find_if(simulation.organisms().begin(),simulation.organisms().end(),[](const life::Organism& organism){return organism.parentId>=0;});if(descendant!=simulation.organisms().end()){const auto childInspection=simulation.inspect(descendant->id);check(childInspection.has_value()&&!childInspection->ancestors.empty()&&!childInspection->mutations.empty(),"native descendant ancestry or mutations missing");}
  simulation.step(900);
  check(stateHash(simulation)=="7e193fae948fceed932a84e374186116c0d9f81b5939f2fde94c1f968499e9df","native tick-1000 whole-state hash mismatch");
  if(simulation.metrics().organisms!=795){std::cerr<<"FAIL: native tick-1000 population mismatch (actual "<<simulation.metrics().organisms<<", expected 795)\n";++failures;}
  near(simulation.metrics().averageEnergy,15.238471698113209,1e-12,"native tick-1000 energy mismatch");
  check(simulation.metrics().nnueEvaluations==36,"native tick-1000 NNUE evaluation count mismatch");
  constexpr int populations[]={803,817,822,823,820,776,751,755,776,786};
  constexpr int evaluations[]={45,54,63,72,86,124,160,213,272,351};
  constexpr const char* stateHashes[]={"0620bd632dbc12919081bbe42754b53f23d9efb88723f4aa4ae0ad7b7e744c94","efc631f1f8c0758fbe4b9fa885f40f0c45f4bc4766fc0299ab57464d13ed8acd","4423b5009c84cc041890c692c95e9b74df85173ee31fbf5bc536d0f45c0536c5","8100cb17b045d09a75a19555bacf072c16cb284bd6fadaa68a68317394faa7c2","ed6cdffde0882cdecd6dc911731b73d7c6d99ad9b9778f82d1295fee5c108e7c","05faef52d76a4a3a3694156ebcebfb55251c7e523566177c9595653889008a1f","ebb5071367e9ff24c104fe1538e64efc5021e5e3b6078041c94f958c2df64ca5","6226d538cfb232633acaf79e057d625636188a92be9e71c0885c399a0554fb15","ac9a5dead1a765d5647b222ed149b3dca278f3c96f4e13ce30a4ede4df91d75c","a4c99e71792f8f0c25e97a6d9e89db30ccd715bc041c04c8b037bd925e3a4e8b"};
  for(int checkpoint=0;checkpoint<10;++checkpoint){simulation.step(100);const auto current=simulation.metrics();check(stateHash(simulation)==stateHashes[checkpoint],"native whole-state checkpoint hash mismatch");if(current.organisms!=populations[checkpoint]){std::cerr<<"FAIL: native tick-"<<(1100+checkpoint*100)<<" population mismatch (actual "<<current.organisms<<", expected "<<populations[checkpoint]<<")\n";++failures;}if(current.nnueEvaluations!=evaluations[checkpoint]){std::cerr<<"FAIL: native tick-"<<(1100+checkpoint*100)<<" NNUE count mismatch (actual "<<current.nnueEvaluations<<", expected "<<evaluations[checkpoint]<<")\n";++failures;}}
  check(simulation.metrics().record==823,"native tick-2000 population record mismatch");
  near(simulation.metrics().averageEnergy,18.353055979643766,1e-12,"native tick-2000 energy mismatch");
  life::NativeSimulation prunedLineage(96,60,.2,500,524114809,334462,5);prunedLineage.select(1);prunedLineage.step(2'000);check(prunedLineage.metrics().organisms==simulation.metrics().organisms,"lineage pruning changed living population");near(prunedLineage.metrics().averageEnergy,simulation.metrics().averageEnergy,1e-12,"lineage pruning changed simulation energy");check(prunedLineage.lineageRecordCount()<simulation.lineageRecordCount(),"lineage pruning did not remove dead leaves");check(prunedLineage.inspect(1).has_value(),"selected lineage record was pruned");
  if (!failures) std::cout << "Native deterministic substrate matches the frozen TypeScript contract.\n";
  return failures ? 1 : 0;
}
