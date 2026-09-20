#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace life {
enum class EvolutionEventType : uint8_t { Season, PopulationPeak, Crash, Recovery, ResourceShortage, AtmosphericStress, Speciation, Extinction, Regeneration };

struct GenomeDescriptor {
  uint64_t bodyHash{},neuralSignature{},brainGenomeId{};
  std::array<uint8_t,12> cellCounts{};
  uint8_t dietMask{},perceptionRadius{},senseChannels{};
  uint16_t moveRange{},birthDistance{},oxygenTolerance{},stressRecovery{},frailty{},mutability{},neuralMutability{};
};

struct SpeciesSummary {
  int id{},parentId{-1},representativeId{-1},population{},peakPopulation{},firstTick{},lastTick{},missingScans{},descendantSpecies{};
  uint8_t dominantTerrain{},dietMask{};
  bool extinct{};
  std::string extinctionCause;
  GenomeDescriptor representative;
};

struct EcologySample {
  int tick{},population{},births{},plantEaters{},scavengers{},mineralEaters{},predators{},species{},criticalRespiratory{};
  std::array<int,7> deaths{};
  uint32_t plant{},carrion{},mineral{};
  float averageEnergy{},averageBodySize{},averagePerceptionCost{},averageRespiratoryStress{},averageOxygenTolerance{},averageStressRecovery{},averageFrailty{},oxygen{},carbonDioxide{},temperature{},fertility{};
  uint64_t nnueEvaluations{},memoryEstimate{};
};

struct EvolutionEvent {
  EvolutionEventType type{};
  int tick{},speciesId{-1};
  float value{};
  std::string label;
};

struct AtlasSnapshot {
  uint64_t revision{};
  int selectedSpecies{-1};
  std::vector<EcologySample> timeline;
  std::vector<SpeciesSummary> species;
  std::vector<EvolutionEvent> events;
};

double genomeDistance(const GenomeDescriptor& left,const GenomeDescriptor& right);
const char* evolutionEventName(EvolutionEventType type);
}
