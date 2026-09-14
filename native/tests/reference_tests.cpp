#include "reference.hpp"
#include "nnue.hpp"
#include <cmath>
#include <iostream>

namespace {
int failures = 0;
void check(bool condition, const char* message) { if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; } }
void near(double actual, double expected, double tolerance, const char* message) { check(std::abs(actual - expected) <= tolerance, message); }
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
  if (!failures) std::cout << "Native deterministic substrate matches the frozen TypeScript contract.\n";
  return failures ? 1 : 0;
}
