#pragma once
#include <cstdint>
namespace amalur {
struct PhysicalMeleeRecipe {uint32_t model{},attack{},flags{};unsigned hands{};};
// Exact native basic-attack observations. Model and attack resource namespaces
// are distinct. Flag1 hammer attacks remain unverified for custom resolution.
inline constexpr PhysicalMeleeRecipe physicalMeleeRecipe(uint32_t model){
 switch(model){case 1520:return {1520,199,0,2};case 2478:return {2478,50,0,1};case 1250:return {1250,417,0,1};default:return {};}
}
inline constexpr bool currentPhysicalPublication(uint32_t owner,uint32_t model,unsigned publishedSelection,unsigned selected,uint64_t tick,uint64_t now){
 return owner&&physicalMeleeRecipe(model).attack&&selected==0&&publishedSelection==selected&&tick&&tick<=now&&now-tick<100;
}
inline constexpr bool supportedPhysicalAttack(uint32_t attack,uint32_t flags){return flags==0&&(attack==199||attack==50||attack==417);}
inline constexpr bool matchesPhysicalRecipe(uint32_t model,uint32_t attack,uint32_t flags){const auto r=physicalMeleeRecipe(model);return r.attack&&r.attack==attack&&r.flags==flags;}
}
