#include "simulation.hpp"
#include <stdexcept>
#include <iostream>
#include <utility>
namespace life {struct NativeSimulationTestAccess {
 static Organism& first(NativeSimulation& w){for(auto& o:w.organisms_)if(o.living&&std::as_const(o).research->development.stage==LifeStage::Adult)return o;return w.organisms_.front();}
 static bool develop(NativeSimulation& w,Organism& o){return w.updateDevelopment(o);}
 static void signals(NativeSimulation& w){w.updateSignals();}
 static void disease(NativeSimulation& w){w.updateDisease();}
 static void mortality(NativeSimulation& w){w.processDiseaseMortality();}
 static void tick(NativeSimulation& w,int tick){w.ticks_=tick;}
 static void features(NativeSimulation& w,Organism& o){w.features(o,o.lastFeatures);}
 static void egg(NativeSimulation& w,Organism& o){w.clearBody(o);o.research->development.matureBody={{CellType::Producer,0,0,0},{CellType::PlantMouth,1,0,0}};o.energy=100000;o.minerals=100000;o.cells={{CellType::Egg,0,0,4}};o.research->development.stage=LifeStage::Egg;o.research->genes.incubation=2;o.research->genes.growthInterval=1;w.refreshCapabilities(o);const auto [dx,dy]=w.rotated(1,0,o.rotation);w.world.terrain[w.wrappedIndex(o.x+dx,o.y+dy)]=uint8_t(TerrainType::Mountain);w.placeBody(o);}
 static void unblock(NativeSimulation& w,Organism& o){const auto [x,y]=w.rotated(1,0,o.rotation);w.world.terrain[w.wrappedIndex(o.x+x,o.y+y)]=uint8_t(TerrainType::Plains);}
 static void blockNext(NativeSimulation& w,Organism& o){const auto& c=o.research->development.matureBody[o.cells.size()];const auto [x,y]=w.rotated(c.x,c.y,o.rotation);w.world.terrain[w.wrappedIndex(o.x+x,o.y+y)]=uint8_t(TerrainType::Mountain);}
 static void debug(const NativeSimulation& a,const NativeSimulation& b){std::cerr<<"RNG "<<a.random_.checkpoint()<<"/"<<b.random_.checkpoint()<<" development "<<a.developmentRandom_.checkpoint()<<"/"<<b.developmentRandom_.checkpoint()<<" disease "<<a.diseaseRandom_.checkpoint()<<"/"<<b.diseaseRandom_.checkpoint()<<"\n";for(size_t i=0;i<a.cells.size();++i)if(a.cells[i]!=b.cells[i]||a.owners[i]!=b.owners[i]||a.world.resourceAmount[i]!=b.world.resourceAmount[i]||a.world.nutrients[i]!=b.world.nutrients[i])std::cerr<<"cell "<<i<<" "<<int(a.cells[i])<<"/"<<int(b.cells[i])<<" resource "<<a.world.resourceAmount[i]<<"/"<<b.world.resourceAmount[i]<<" nutrients "<<a.world.nutrients[i]<<"/"<<b.world.nutrients[i]<<"\n";}
 static void contacts(NativeSimulation& w){w.cells.assign(w.cells.size(),0);w.owners.assign(w.owners.size(),-1);for(size_t i=0;i<w.organisms_.size();++i){auto& o=w.organisms_[i];o.x=4+int(i);o.y=4;o.cells={{CellType::Mover,0,0,0}};o.research->development={};o.research->genes.resistance=0;o.research->genes.infectionRecovery=.001;w.placeBody(o);}}
 static Organism& at(NativeSimulation& w,size_t index){return w.organisms_.at(index);}
 static void produce(NativeSimulation& world,Organism& organism){
   world.updateClimateCache();
   world.produce(organism,4,4);
 }
 static void risk(NativeSimulation& w){for(auto& o:w.organisms_)if(o.living)o.research->infection.risk=.2;}
};}
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
 life::NativeSimulation production(32,24,10.,500,53,17);
 std::fill(production.world.terrain.begin(),production.world.terrain.end(),uint8_t(life::TerrainType::Fertile));
 std::fill(production.world.nutrients.begin(),production.world.nutrients.end(),800);
 life::Organism producer;
 producer.cells={{life::CellType::Mover,0,0,0},{life::CellType::Producer,1,0,0}};
 producer.isMover=true;
 producer.energy=100000;
 require(!production.moverProducersEnabled(),"Mover producers must default off");
 life::NativeSimulationTestAccess::produce(production,producer);
 require(producer.energy==100000,"Disabled mover spent energy producing food");
 production.executeCommand({0,0,life::CommandKind::Configure,life::NativeSimulation::MoverProducerSetting,0,1});
 life::NativeSimulationTestAccess::produce(production,producer);
 require(producer.energy<100000,"Enabled mover did not produce food");
 const auto settingsPath=std::filesystem::current_path()/"mover-producers.lfesave";
 production.save(settingsPath);
 life::NativeSimulation settingsReload(32,24,.2,500,53,17);
 settingsReload.load(settingsPath);
 require(settingsReload.moverProducersEnabled(),"Mover producer setting was lost on load");
 std::filesystem::remove(settingsPath);
 production.executeCommand({0,0,life::CommandKind::Configure,life::NativeSimulation::MoverProducerSetting,0,0});
 producer.isMover=false;
 producer.energy=100000;
 life::NativeSimulationTestAccess::produce(production,producer);
 require(producer.energy<100000,"Stationary producer stopped producing food");
 life::NativeSimulation w(48,32,.2,500,3,17);w.seedBenchmarkMovers(30);auto& o=life::NativeSimulationTestAccess::first(w);const int id=o.id;life::NativeSimulationTestAccess::egg(w,o);require(!o.isMover,"Egg moved");for(int i=0;i<8&&std::as_const(o).research->development.stage==life::LifeStage::Egg;++i)life::NativeSimulationTestAccess::develop(w,o);require(std::as_const(o).research->development.stage!=life::LifeStage::Egg&&o.id==id,"Hatch changed identity");require(o.cells.size()==1,"Blocked juvenile grew through a mountain");life::NativeSimulationTestAccess::unblock(w,o);o.energy=100000;o.minerals=100000;for(int i=0;i<20&&std::as_const(o).research->development.stage!=life::LifeStage::Adult;++i)life::NativeSimulationTestAccess::develop(w,o);require(o.id==id&&o.cells.size()==2&&std::as_const(o).research->development.stage==life::LifeStage::Adult&&o.energy==99750,"Growth did not pay cost or mature");
 life::NativeSimulation signals(32,24,.2,500,3,17);signals.seedBenchmarkMovers(1);auto& emitter=life::NativeSimulationTestAccess::first(signals);emitter.research->genes.emission={16,0};emitter.energy=100000;const int energy=emitter.energy;life::NativeSimulationTestAccess::signals(signals);require(emitter.energy==energy-32,"Signal emission energy wrong");uint64_t before=0;for(auto value:signals.signalLayers()[0])before+=value;require(before>0,"Signal absent");emitter.research->genes.emission={0,0};life::NativeSimulationTestAccess::signals(signals);uint64_t after=0;for(auto value:signals.signalLayers()[0])after+=value;require(after<before,"Signal diffusion created mass");emitter.senseChannels|=life::SignalA|life::SignalB;life::NativeSimulationTestAccess::features(signals,emitter);require(std::any_of(emitter.lastFeatures.begin(),emitter.lastFeatures.end(),[](int f){return f>=life::SignalAFeatureOffset&&f<life::StageFeatureOffset;}),"Signal sensory rows absent");
 life::Mulberry32 rng(9);life::Nnue brain(rng);for(int row=life::ResearchFeatureOffset;row<life::InputSize;++row)require(brain.featureRowZero(row),"Untrained ecology weight was nonzero");const auto budget=life::clampPerception(4,4095);require(budget.cost<=life::MaximumInferenceBudget,"Receptors exceeded inference budget");
 life::NativeSimulation disease(48,32,.2,500,3,17);disease.seedBenchmarkMovers(20);life::NativeSimulationTestAccess::risk(disease);int previous=0,last=-10;for(int tick=0;tick<100;++tick){life::NativeSimulationTestAccess::tick(disease,tick);life::NativeSimulationTestAccess::mortality(disease);const int deaths=disease.researchEcology().diseaseDeaths;if(deaths>previous){require(tick-last>=10&&deaths==previous+1,"Disease death pacing failed");last=tick;previous=deaths;}}require(previous==10,"Severe disease queue did not resolve");
 life::NativeSimulation local(32,24,.2,500,3,17);local.seedBenchmarkMovers(2);life::NativeSimulationTestAccess::contacts(local);auto& source=life::NativeSimulationTestAccess::at(local,0);auto& host=life::NativeSimulationTestAccess::at(local,1);source.research->infection.load=1;for(int tick=30;tick<=3000&&std::as_const(host).research->infection.load==0;tick+=30){life::NativeSimulationTestAccess::tick(local,tick);source.research->infection.load=1;life::NativeSimulationTestAccess::disease(local);}require(std::as_const(host).research->infection.load>0,"Local contact never transmitted disease");host.research->genes.resistance=1;host.research->genes.infectionRecovery=.1;host.research->infection.load=.01;life::NativeSimulationTestAccess::disease(local);require(std::as_const(host).research->infection.load==0&&std::as_const(host).research->infection.protectedUntil>local.tickCount(),"Recovery did not provide temporary immunity");life::NativeSimulationTestAccess::tick(local,local.tickCount()+30);source.research->infection.load=1;life::NativeSimulationTestAccess::disease(local);require(std::as_const(host).research->infection.load==0,"Protected host was reinfected");
 life::NativeSimulation replay(64,40,.2,500,4,9);replay.seedBenchmarkMovers(90);replay.step(180);auto copy=replay.freeze();replay.step(360);copy->step(360);require(copy->stateDigest()==replay.stateDigest(),"New biology replay differs");const auto path=std::filesystem::current_path()/"biology-v3.lfesave";replay.save(path);copy->load(path);require(copy->stateDigest()==replay.stateDigest(),"New biology archive differs");for(int step=0;step<120;++step){copy->step();replay.step();if(copy->stateDigest()!=replay.stateDigest()){life::NativeSimulationTestAccess::debug(*copy,replay);std::cerr<<"Diverged at "<<replay.tickCount()<<"\n";for(size_t i=0;i<std::min(copy->organisms().size(),replay.organisms().size());++i){const auto& a=copy->organisms()[i];const auto& b=replay.organisms()[i];if(a.energy!=b.energy||a.x!=b.x||a.y!=b.y||a.lastOutputs!=b.lastOutputs||a.research->infection.load!=b.research->infection.load)std::cerr<<a.id<<" energy "<<a.energy<<"/"<<b.energy<<" pos "<<a.x<<","<<a.y<<"/"<<b.x<<","<<b.y<<" infection "<<a.research->infection.load<<"/"<<b.research->infection.load<<"\n";}break;}}require(copy->stateDigest()==replay.stateDigest(),"New biology continuation differs");std::filesystem::remove(path);std::cout<<"Development, chemical energy/diffusion, v5 zero rows, disease pacing and replay passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
