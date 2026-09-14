#pragma once
#include "nnue.hpp"
#include "reference.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace life {
struct BodyCell { CellType type; int x, y; int durability{}; };
struct Organism {
  int id{}, parentId{-1}, generation{}, birthTick{}, x{}, y{};
  std::vector<BodyCell> cells;
  int energy{StartEnergy}, minerals{}, lifetime{}, damage{}, mutability{5}, neuralMutability{8};
  int birthDistance{4}, moveRange{4}, moveCount{}, direction{}, rotation{};
  bool living{true}, isProducer{}, isMover{}, isConsumer{}, isAttacker{};
  double thermalStress{};
  int perceptionRadius{DefaultSensorRadius}; uint8_t senseChannels{DefaultSenseChannels}; int neuralCost{168};
};

struct NativeMetrics { int organisms{}, record{}, generation{}, ticks{}, largest{}; double averageEnergy{}, temperature{}, fertility{}; };

class NativeSimulation {
  int width_, height_; uint32_t worldSeed_; Mulberry32 random_; int nextId_{1}, ticks_{}, resets_{}, record_{}, largest_{};
  double foodChance_; int lifespan_; std::vector<Organism> organisms_;
  int safeIndex(int x, int y) const;
  std::pair<int,int> rotated(int x, int y, int direction) const;
  void placeBody(const Organism& organism);
  void refreshCapabilities(Organism& organism);
  void updateOrganism(Organism& organism);
  void eat(Organism& organism, CellType mouth, int x, int y);
  void produce(Organism& organism, int x, int y);
public:
  WorldLayers world;
  std::vector<uint8_t> cells;
  std::vector<int32_t> owners;
  NativeSimulation(int width, int height, double foodChance, int lifespan, uint32_t worldSeed, uint32_t randomSeed);
  void reset();
  void step(int count = 1);
  NativeMetrics metrics() const;
  const std::vector<Organism>& organisms() const { return organisms_; }
};
}
