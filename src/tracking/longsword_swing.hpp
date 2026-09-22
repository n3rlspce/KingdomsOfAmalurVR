#pragma once
#include "melee_swing.hpp"
namespace amalur {
// Longsword-only tuning. Existing MeleeSwing defaults for other weapons stay intact.
inline bool sampleLongswordSwing(MeleeSwing& detector,mgs5vr::Vec3 tip,uint64_t tick,unsigned generation,bool eligible,bool preparing){
 return detector.sample(tip,tick,generation,eligible,1.0f,30,!preparing,.55f,60);
}
}
