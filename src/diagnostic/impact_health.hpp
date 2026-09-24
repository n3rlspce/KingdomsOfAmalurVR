#pragma once
// Read-only health confirmation around the synchronous native physical resolver.
// Dedup-array growth alone is NOT damage. Despawn/read failure never implies a kill.
namespace impact_health {
struct Entry {uint32_t actor{};int32_t health{};};
struct Snapshot {Entry actors[64]{};unsigned count{};};
inline bool read(uint32_t actor,int32_t& value){
 __try {const auto entity=player_rig::resolve(actor);if(!entity)return false;
  const auto part=melee_native::component(actor,1);if(!part)return false;
  value=int32_t(player_rig::word(part+0x48));return player_rig::resolve(actor)==entity;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline Snapshot before(const melee_native::HitArray& hits,uint32_t owner){
 Snapshot s;
 __try {if(!hits.data||hits.count>hits.capacity||hits.count>4096)return s;
  for(unsigned i=0;i<hits.count&&s.count<64;++i){
   const auto actor=amalur::meleeHitActor(player_rig::word(reinterpret_cast<uintptr_t>(hits.data)+i*0x70+0x14));
   if(!actor||actor==owner)continue;bool seen=false;for(unsigned j=0;j<s.count;++j)seen|=s.actors[j].actor==actor;
   int32_t hp{};if(!seen&&read(actor,hp)&&hp>0)s.actors[s.count++]={actor,hp};
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){return Snapshot{};}
 return s;
}
inline bool decreased(const Snapshot& s){
 for(unsigned i=0;i<s.count;++i){int32_t hp{};if(read(s.actors[i].actor,hp)&&hp<s.actors[i].health)return true;}
 return false;
}
}
