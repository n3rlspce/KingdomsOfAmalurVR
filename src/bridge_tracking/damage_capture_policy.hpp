#pragma once
#include <cstdint>
namespace amalur {
struct DamageCaptureBudget {
 uint64_t start{};unsigned used{},dropped{};
 bool take(uint64_t now,unsigned& previousDropped){previousDropped=0;
  if(!start||now<start||now-start>=1000){previousDropped=dropped;start=now;used=dropped=0;}
  if(used>=64){++dropped;return false;}++used;return true;
 }
};
inline bool damageCaptureRuntime(uint32_t expectedOwner,uint32_t actualOwner,uint32_t expectedIndex,uint32_t actualIndex,uint32_t asset){
 return expectedOwner&&expectedOwner==actualOwner&&expectedIndex&&expectedIndex==actualIndex&&asset>=2&&asset<1000000;
}
inline uint32_t captureHashWord(uint32_t hash,uint32_t word){return (hash^word)*16777619u;}
}
