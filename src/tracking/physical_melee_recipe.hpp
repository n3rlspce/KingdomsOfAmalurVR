#pragma once
#include <cstdint>
#include "weapon_family.hpp"
namespace amalur {
struct PhysicalMeleeRecipe {uint32_t model{},attack{},flags{};unsigned hands{};};
// Exact native basic-attack observations. Model and attack resource namespaces
// are distinct. Hammer16 uses captured flags1; downstream audit035 traces this
// field through feedback forwarding, with no extra runtime ownership mode.
inline constexpr PhysicalMeleeRecipe physicalMeleeRecipe(uint32_t model){
 if(knownHammerModel(model))return {model,16,1,1};
 if(knownLongswordModel(model))return {model,50,0,1};
 switch(model){case 1520:return {1520,199,0,2};case 1250:return {1250,417,0,1};default:return {};}
}
inline constexpr bool currentPhysicalPublication(uint32_t owner,uint32_t model,unsigned publishedSelection,unsigned selected,uint64_t tick,uint64_t now){
 return owner&&physicalMeleeRecipe(model).attack&&selected<=1&&publishedSelection==selected&&tick&&tick<=now&&now-tick<100;
}
inline constexpr bool currentPhysicalDamageWeapon(uint32_t visual,uint32_t primary,uint32_t overrideWeapon){
 return visual&&visual==(overrideWeapon?overrideWeapon:primary);
}
inline constexpr bool supportedPhysicalAttack(uint32_t attack,uint32_t flags){return (flags==0&&(attack==199||attack==50||attack==417))||(attack==16&&flags==1);}
inline constexpr bool matchesPhysicalRecipe(uint32_t model,uint32_t attack,uint32_t flags){const auto r=physicalMeleeRecipe(model);return r.attack&&r.attack==attack&&r.flags==flags;}
}
