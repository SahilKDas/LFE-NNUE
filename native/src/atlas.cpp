#include "atlas.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace life {
double genomeDistance(const GenomeDescriptor& left,const GenomeDescriptor& right){
  int cellDelta=0,cellTotal=0;for(size_t index=0;index<left.cellCounts.size();++index){cellDelta+=std::abs(int(left.cellCounts[index])-int(right.cellCounts[index]));cellTotal+=std::max(int(left.cellCounts[index]),int(right.cellCounts[index]));}
  const double topology=left.bodyHash==right.bodyHash?0.0:std::min(1.0,.35+double(cellDelta)/std::max(1,cellTotal));
  const double neural=double(std::popcount(left.neuralSignature^right.neuralSignature))/64.0;
  const double behavior=(std::abs(int(left.perceptionRadius)-int(right.perceptionRadius))/3.0+double(std::popcount(uint8_t(left.senseChannels^right.senseChannels)))/8.0+std::min(1.0,std::abs(int(left.moveRange)-int(right.moveRange))/8.0)+std::min(1.0,std::abs(int(left.birthDistance)-int(right.birthDistance))/8.0))/4.0;
  const double physiology=((left.dietMask==right.dietMask?0.0:1.0)+std::abs(int(left.oxygenTolerance)-int(right.oxygenTolerance))/800.0+std::abs(int(left.stressRecovery)-int(right.stressRecovery))/230.0+std::abs(int(left.frailty)-int(right.frailty))/1650.0)/4.0;
  return topology*.40+neural*.30+behavior*.15+physiology*.15;
}
const char* evolutionEventName(EvolutionEventType type){constexpr const char* names[]={"Season","Population peak","Population crash","Recovery","Resource shortage","Atmospheric stress","Speciation","Extinction","World regeneration"};return names[size_t(type)];}
}
