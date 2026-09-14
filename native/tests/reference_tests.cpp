#include "reference.hpp"
#include "nnue.hpp"
#include "simulation.hpp"
#include <cmath>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message) { if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; } }
void near(double actual, double expected, double tolerance, const char* message) { if(std::abs(actual-expected)>tolerance){std::cerr<<"FAIL: "<<message<<" (actual "<<actual<<", expected "<<expected<<")\n";++failures;} }
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
  const auto priorTerrain=editable.world.terrain;editable.regenerate(42);check(editable.metrics().ticks==0&&editable.metrics().organisms==1,"regeneration did not reset simulation");check(editable.world.terrain!=priorTerrain,"regeneration seed did not replace terrain");check(editable.organismAt(16,10)!=nullptr,"organism inspection lookup failed");
  life::NativeSimulation benchmarkPopulation(160,100,.2,500,73,19);check(benchmarkPopulation.seedBenchmarkMovers(10'000)==10'000,"benchmark population did not reach 10,000 movers");check(benchmarkPopulation.metrics().organisms==10'000,"benchmark mover metrics mismatch");
  life::NativeSimulation simulation(96,60,.2,500,524114809,334462);
  check(simulation.metrics().organisms==1&&simulation.metrics().averageEnergy==12.0,"native reset state mismatch");
  const auto founder=simulation.inspect(1);check(founder.has_value()&&founder->generation==0&&founder->ancestors.empty()&&founder->descendants.empty()&&founder->diet=="Plant","native founder inspection mismatch");
  simulation.step();
  check(simulation.metrics().ticks==1,"native simulation clock mismatch");
  near(simulation.metrics().averageEnergy,11.989,1e-12,"native first-tick energy mismatch");
  simulation.step(9);
  check(simulation.metrics().organisms==1,"native tick-10 population mismatch");
  near(simulation.metrics().averageEnergy,15.925,1e-12,"native tick-10 energy mismatch");
  simulation.step(90);
  if(simulation.metrics().organisms!=39){std::cerr<<"FAIL: native tick-100 population mismatch (actual "<<simulation.metrics().organisms<<", expected 39)\n";++failures;}
  near(simulation.metrics().averageEnergy,13.923538461538461,1e-12,"native tick-100 energy mismatch");
  const auto evolvedFounder=simulation.inspect(1);check(evolvedFounder.has_value()&&evolvedFounder->totalDescendants>0,"native lineage descendants were not recorded");
  const auto descendant=std::find_if(simulation.organisms().begin(),simulation.organisms().end(),[](const life::Organism& organism){return organism.parentId>=0;});if(descendant!=simulation.organisms().end()){const auto childInspection=simulation.inspect(descendant->id);check(childInspection.has_value()&&!childInspection->ancestors.empty()&&!childInspection->mutations.empty(),"native descendant ancestry or mutations missing");}
  simulation.step(900);
  if(simulation.metrics().organisms!=795){std::cerr<<"FAIL: native tick-1000 population mismatch (actual "<<simulation.metrics().organisms<<", expected 795)\n";++failures;}
  near(simulation.metrics().averageEnergy,15.238471698113209,1e-12,"native tick-1000 energy mismatch");
  check(simulation.metrics().nnueEvaluations==36,"native tick-1000 NNUE evaluation count mismatch");
  constexpr int populations[]={803,817,822,823,820,776,751,755,776,786};
  constexpr int evaluations[]={45,54,63,72,86,124,160,213,272,351};
  for(int checkpoint=0;checkpoint<10;++checkpoint){simulation.step(100);const auto current=simulation.metrics();if(current.organisms!=populations[checkpoint]){std::cerr<<"FAIL: native tick-"<<(1100+checkpoint*100)<<" population mismatch (actual "<<current.organisms<<", expected "<<populations[checkpoint]<<")\n";++failures;}if(current.nnueEvaluations!=evaluations[checkpoint]){std::cerr<<"FAIL: native tick-"<<(1100+checkpoint*100)<<" NNUE count mismatch (actual "<<current.nnueEvaluations<<", expected "<<evaluations[checkpoint]<<")\n";++failures;}}
  check(simulation.metrics().record==823,"native tick-2000 population record mismatch");
  near(simulation.metrics().averageEnergy,18.353055979643766,1e-12,"native tick-2000 energy mismatch");
  life::NativeSimulation prunedLineage(96,60,.2,500,524114809,334462,5);prunedLineage.select(1);prunedLineage.step(2'000);check(prunedLineage.metrics().organisms==simulation.metrics().organisms,"lineage pruning changed living population");near(prunedLineage.metrics().averageEnergy,simulation.metrics().averageEnergy,1e-12,"lineage pruning changed simulation energy");check(prunedLineage.lineageRecordCount()<simulation.lineageRecordCount(),"lineage pruning did not remove dead leaves");check(prunedLineage.inspect(1).has_value(),"selected lineage record was pruned");
  if (!failures) std::cout << "Native deterministic substrate matches the frozen TypeScript contract.\n";
  return failures ? 1 : 0;
}
