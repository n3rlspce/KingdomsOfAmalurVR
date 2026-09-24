#pragma once
#include <atomic>
#include "../tracking/incoming_damage_feedback.hpp"
#include "../tracking/melee_hit_identity.hpp"
// Global scope after player_rig, melee_native and native_action_haptics.
namespace incoming_damage_feedback {
inline melee_native::Resolve original{};
inline bool installed{};
inline std::atomic<uint64_t> eventCounter{0};
inline bool snapshot(amalur::IncomingDamageSnapshot& out){
 const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
 if(!player||(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94))return false;
 const auto owner=uint32_t(player_rig::word(player+0x1ec));const auto entity=player_rig::resolve(owner);
 if(!owner||!entity||player_rig::word(entity+0x38)!=owner||!(player_rig::word(entity+0x10c)&1))return false;
 const auto health=melee_native::component(owner,1);if(!health)return false;
 out={player,entity,health,owner,int32_t(player_rig::word(health+0x48))};
 return player_rig::resolve(owner)==entity;
}
inline bool targets(const void* raw,uint32_t owner){
 if(!raw)return false;
 // Borrow only: never construct/copy a HitArray, whose destructor frees records.
 const auto hits=reinterpret_cast<const melee_native::HitArray*>(raw);
 if(!hits->data||!hits->count||hits->count>hits->capacity||hits->count>4096)return false;
 for(unsigned i=0;i<hits->count;++i)
  if(amalur::meleeHitActor(uint32_t(player_rig::word(reinterpret_cast<uintptr_t>(hits->data)+i*0x70+0x14)))==owner)return true;
 return false;
}
inline void __fastcall resolve(void* self,void*,uint32_t flags,void* hits,const float* from,const float* to,int32_t talent,uint32_t key){
 amalur::IncomingDamageSnapshot before{},after{};bool observe=false;
 __try {observe=snapshot(before)&&targets(hits,before.owner);}
 __except(EXCEPTION_EXECUTE_HANDLER){observe=false;}
 // Preserve native exception behavior and every original argument exactly.
 original(self,flags,hits,from,to,talent,key);
 __try {
  if(observe&&snapshot(after)&&amalur::confirmedIncomingDamage(before,after,true))
   native_action_haptics::emit(amalur::ActionFeedbackKind::Damage,before.owner,
    (uint64_t(1)<<62)|(++eventCounter));
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline void install(){
 if(installed||!melee_native::ready)return;
 __try {
  constexpr unsigned char prefix[]{0x55,0x8b,0xec,0x83,0xe4,0xf8,0xb8,0x64,0x12,0,0};
  if(!melee_native::signature(0xb9d520,prefix,sizeof(prefix)))return;
  installed=hook(reinterpret_cast<unsigned char*>(gameBase+0xb9d520),reinterpret_cast<void*>(&resolve),
   reinterpret_cast<void**>(&original),"Confirmed incoming physical damage");
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
}
