#pragma once
#include <cstdint>
namespace amalur {
struct IncomingDamageSnapshot {
 uintptr_t player{},entity{},healthPart{};uint32_t owner{};int32_t health{};
};
inline bool confirmedIncomingDamage(const IncomingDamageSnapshot& before,const IncomingDamageSnapshot& after,bool targeted){
 return targeted&&before.player&&before.entity&&before.healthPart&&before.owner&&before.health>0
  &&after.player==before.player&&after.entity==before.entity&&after.healthPart==before.healthPart
  &&after.owner==before.owner&&after.health>=0&&after.health<before.health;
}
}
