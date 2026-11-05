#pragma once
#include "nnue.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <array>
#include <memory>
namespace life {
struct BodyCell {CellType type;int x,y;int durability{};};
enum class CommandKind:uint8_t {Terrain,Resource,Nutrients,Regenerate,Reset,Configure,Introduce};
struct SimulationCommand {uint64_t sequence{};int tick{};CommandKind kind{};int x{},y{},value{},amount{};std::string artifact;};
struct ResearchGenes {
  uint16_t incubation{},investment{350},shell{4},growthInterval{30};
  std::array<uint8_t,2> emission{};
  double resistance{.35},infectionRecovery{.01};
  bool operator==(const ResearchGenes&)const=default;
};
enum class LifeStage:uint8_t {Adult,Egg,Juvenile};
struct Development {LifeStage stage{LifeStage::Adult};int ticks{},growthTicks{};std::vector<BodyCell> matureBody;};
struct Infection {double load{},stress{},risk{};int protectedUntil{};};
struct ResearchState {ResearchGenes genes;Development development;Infection infection;std::string importedFrom;};
class ResearchStorage {
 std::unique_ptr<ResearchState> value_;
public:
 ResearchStorage()=default;ResearchStorage(const ResearchStorage& other){if(other.value_)value_=std::make_unique<ResearchState>(*other.value_);}
 ResearchStorage& operator=(const ResearchStorage& other){if(this!=&other)value_=other.value_?std::make_unique<ResearchState>(*other.value_):nullptr;return *this;}
 ResearchStorage(ResearchStorage&&) noexcept=default;ResearchStorage& operator=(ResearchStorage&&) noexcept=default;
 ResearchState* operator->(){if(!value_)value_=std::make_unique<ResearchState>();return value_.get();}
 const ResearchState* operator->()const{static const ResearchState empty;return value_?value_.get():&empty;}
 bool allocated()const{return bool(value_);}
};
struct ScopedOrganism {uint64_t run{},branch{1};int id{-1};};
struct GenomeRecord {
  uint32_t version{1};uint64_t brainId{};ScopedOrganism source;int generation{},birthTick{},deathTick{-1},offspring{},descendants{};
  std::string name,notes,provenance;std::vector<BodyCell> body;
  int moveRange{4},birthDistance{4},mutability{5},radius{2};uint16_t channels{DefaultSenseChannels};
  double neuralMutability{8},oxygenTolerance{.12},respiratoryRecovery{.01},frailty{1};ResearchGenes genes;
  std::shared_ptr<const Nnue::Weights> neural;
};
struct WeightDifference {uint32_t row{};uint32_t changed{};double magnitude{};};
struct GenomeComparison {std::vector<std::string> traits;std::vector<WeightDifference> rows;uint64_t changedWeights{};double magnitude{};};
enum class JournalKind:uint8_t {Ecology,Development,Infection,Recovery,DiseaseDeath,Experiment,Introduction,HistoryGap};
struct JournalEntry {uint64_t sequence{},run{},branch{1};int firstTick{},lastTick{},organism{-1},species{-1},count{1};JournalKind kind{};std::string label,evidence;};
struct ResearchEcology {int tick{},eggs{},juveniles{},infected{},immune{},diseaseDeaths{},hatched{},matured{};uint64_t signalA{},signalB{};};
struct BranchSummary {uint64_t id{},parent{};std::string name;int forkTick{},tick{},population{},births{},deaths{};bool active{};std::vector<ResearchEcology> ecology;};
struct CheckpointInfo {uint64_t id{},branch{1},command{};int tick{};uint64_t bytes{};std::filesystem::path path;};
struct LibraryItem {std::string name,notes;std::filesystem::path path;bool world{};int population{},tick{},cells{};uint64_t genome{};};
struct ResearchSnapshot {
  uint64_t revision{};uint64_t branch{1};int tick{},presentTick{},seekTick{};bool preview{},busy{};double progress{},captureMs{};std::string status;
  uint64_t memoryBytes{},diskBytes{};std::vector<CheckpointInfo> checkpoints;std::vector<BranchSummary> branches;std::vector<JournalEntry> journal;std::vector<LibraryItem> library;
  std::shared_ptr<const GenomeRecord> left,right;GenomeComparison comparison;ResearchEcology ecology;
};
inline const char* stageName(LifeStage stage){return stage==LifeStage::Egg?"Egg":stage==LifeStage::Juvenile?"Juvenile":"Adult";}
}
