#pragma once
#include "melee_native_family.hpp"
#include "melee_swing_event.hpp"
namespace amalur {
struct MeleeVfxSelection {uint32_t selector{},attachment{},durationMs{};};
inline constexpr MeleeVfxSelection meleeVfxSelection(uint32_t model,uint32_t attack,uint32_t flags,bool heavy,unsigned hand){
 if(hand>1||(hand==1&&(model!=1520||heavy)))return {};
 // Frozen native reference: attack81/flags1, keys4e1c and4e29,
 // traces893/934: selector0032dcd4, attachment006666f1, duration400.
 // Resolve the current weapon resource; never reuse captured asset88/bindings.
 if(heavy)return knownLongswordModel(model)&&attack==81&&flags==1
     ?MeleeVfxSelection{0x0032dcd4,0x006666f1,400}:MeleeVfxSelection{};
 if(knownLongswordModel(model)){
   if(!supportedLongswordDamage(attack,flags)||attack==81)return {};
 }else if(!supportedNativeFamilyAttack(model,attack,flags))return {};
 return {0x0032dcd4,hand?0x00ceac76u:0x00858053u,model==1520?160u:200u};
}
inline MeleeVfxSelection meleeVfxSelection(const MeleeSwingEvent& e){return meleeVfxSelection(e.asset,e.attackAsset,e.attackFlags,e.heavy,e.hand);}
// Update receives current identity/timestamp separately from the committed event.
// Preserve its exact attachment throughout the trail, not just at allocation.
struct MeleeVfxPoseIdentity {
 MeleeSwingEvent event{};
 void clear(){event={};}
 void committed(const MeleeSwingEvent& e){event=e;}
 MeleeSwingEvent current(uint32_t owner,uint32_t weapon,uint32_t model,unsigned generation,uint64_t now)const{
   auto p=event;
   if(p.owner!=owner||p.weapon!=weapon||p.asset!=model||p.generation!=generation)p={};
   p.owner=owner;p.weapon=weapon;p.asset=model;p.generation=generation;p.tick=now;return p;
 }
};
}
