#pragma once
#include <cstdint>
namespace amalur {
struct LongswordDamageRecipe {uint32_t asset{},flags{},field208{};bool scripted{},damage{};};
// Native reference20260921:6 and78 had no damage registration. Keep their
// existence visible without inventing resolver flags or a damaging charge phase.
inline constexpr LongswordDamageRecipe longswordDamageRecipe(uint32_t asset){
 switch(asset){case 50:return {50,0,0x0100e002,true,true};case 5:return {5,1,0x0100e002,false,true};
 case 6:return {6,0,0x0100e002,false,false};case 7:return {7,2,0x0100c002,false,true};
 case 78:return {78,0,0x01006002,false,false};case 81:return {81,1,0x01004002,false,true};default:return {};}
}
inline constexpr LongswordDamageRecipe longswordComboDamage(unsigned stage){return longswordDamageRecipe(stage==0?50:stage==1?5:stage==2?7:0);}
inline constexpr LongswordDamageRecipe longswordHeavyDamage(){return longswordDamageRecipe(81);}
inline constexpr bool supportedLongswordDamage(uint32_t asset,uint32_t flags){auto r=longswordDamageRecipe(asset);return r.damage&&r.flags==flags;}
struct DirectWeaponDefinition {uint32_t listenerCount{},script{},kind{},field1bc{},field1f8{},field1fc{},field200{},field208{},field20c{};};
inline constexpr bool matchesLongswordDirectDefinition(uint32_t asset,const DirectWeaponDefinition& d){
 auto r=longswordDamageRecipe(asset);return r.damage&&!r.scripted&&d.listenerCount==0&&d.script==1&&d.kind==1
 &&!d.field1bc&&!d.field1f8&&!d.field1fc&&!d.field200&&d.field208==r.field208&&d.field20c==0x01000104;
}
}
