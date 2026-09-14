#pragma once
#include "reference.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <vector>

namespace life {
enum SenseChannel : uint8_t { Occupancy=1, Heading=2, Danger=4, Resources=8, Terrain=16, Temperature=32, Fertility=64, Internal=128 };
inline constexpr uint8_t DefaultSenseChannels = Occupancy | Heading | Danger | Resources | Internal | Temperature | Fertility;
struct Perception { int radius; uint8_t channels; int cost; };
inline Perception clampPerception(int radius, int channels) {
  int safe = std::clamp(radius, 1, MaximumSensorRadius); uint8_t mask = uint8_t(channels & 255);
  const auto cost = [&] { return ((safe * 2 + 1) * (safe * 2 + 1) - 1) * std::max(1, std::popcount(mask)); };
  while (cost() > MaximumInferenceBudget && safe > 1) --safe;
  for (int bit = 128; cost() > MaximumInferenceBudget && bit >= 1; bit >>= 1) if (mask & bit) mask &= uint8_t(~bit);
  return {safe, mask, cost()};
}

class Nnue {
  std::vector<std::array<float, HiddenSize>> inputWeights;
  std::array<float, HiddenSize> hiddenBias{}, accumulator{};
  std::array<std::array<float, HiddenSize>, OutputSize> outputWeights{};
  std::array<float, OutputSize> outputBias{}, output{};
  std::vector<uint16_t> active;
  static bool trainedFeature(int index) {
    return (index < 24 * FeatureCategories && index % FeatureCategories < 11) ||
      (index >= PositionCount * FeatureCategories && index < PositionCount * FeatureCategories + 8);
  }
  static float weight(Mulberry32& random, double scale) { return float((random() * 2.0 - 1.0) * scale); }
  void apply(int feature, float sign) {
    if (feature < 0 || feature >= int(inputWeights.size())) return;
    for (int hidden = 0; hidden < HiddenSize; ++hidden)
      accumulator[hidden] = float(accumulator[hidden] + sign * inputWeights[feature][hidden]);
  }
public:
  explicit Nnue(Mulberry32& random) : inputWeights(InputSize), active() {
    for (int input = 0; input < InputSize; ++input)
      for (float& value : inputWeights[input]) value = trainedFeature(input) ? weight(random, .12) : 0.0f;
    for (float& value : hiddenBias) value = weight(random, .05);
    for (auto& row : outputWeights) for (float& value : row) value = weight(random, .18);
    for (float& value : outputBias) value = weight(random, .05);
    accumulator = hiddenBias;
  }
  const std::array<float, OutputSize>& evaluate(std::span<const uint16_t> features) {
    size_t oldIndex = 0, newIndex = 0;
    while (oldIndex < active.size() || newIndex < features.size()) {
      const int oldFeature = oldIndex < active.size() ? active[oldIndex] : InputSize;
      const int newFeature = newIndex < features.size() ? features[newIndex] : InputSize;
      if (oldFeature == newFeature) { ++oldIndex; ++newIndex; }
      else if (oldFeature < newFeature) { apply(oldFeature, -1.0f); ++oldIndex; }
      else { apply(newFeature, 1.0f); ++newIndex; }
    }
    active.assign(features.begin(), features.end());
    for (int action = 0; action < OutputSize; ++action) {
      double value = outputBias[action];
      for (int hidden = 0; hidden < HiddenSize; ++hidden)
        value += double(std::max(0.0f, accumulator[hidden])) * outputWeights[action][hidden];
      output[action] = float(value);
    }
    return output;
  }
  int action(std::span<const uint16_t> features) {
    const auto& scores = evaluate(features); int best = 0;
    for (int index = 1; index < OutputSize; ++index) if (scores[index] > scores[best]) best = index;
    return best;
  }
};
}
