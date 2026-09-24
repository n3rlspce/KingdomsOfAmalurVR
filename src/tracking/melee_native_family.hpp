#pragma once
#include "longsword_damage_recipe.hpp"
#include "weapon_family.hpp"
#include <cstdint>
#include <initializer_list>
namespace amalur {
struct NativeFamilyRecipe {uint32_t model{},attack{},flags{},field208{};bool scripted{};unsigned step{};};
// Frozen native-reference-20260921, damage-report appendix: exact ordinary
// single-runtime chains. Multi-runtime heavies and delayed specials excluded.
inline constexpr NativeFamilyRecipe nativeFamilyRecipe(uint32_t model,unsigned step){
    if(model==1520){switch(step){
        case 1:return {model,199,0,0x0100e002,true,step};case 2:return {model,200,0,0x0100e002,true,step};
        case 3:return {model,201,2,0x0100e002,true,step};case 4:return {model,202,0,0x0100e002,true,step};}}
    if(model==1250){switch(step){
        case 1:return {model,417,0,0x0100e002,true,step};case 2:return {model,418,1,0x0100e002,false,step};
        case 3:return {model,419,1,0x0100e002,false,step};case 4:return {model,420,2,0x0100c002,false,step};}}
    if(knownHammerModel(model)){switch(step){
        case 1:return {model,16,1,0x0100e002,true,step};case 2:return {model,17,1,0x0100e002,true,step};
        case 3:return {model,18,2,0x0100c002,true,step};}}
    return {};
}
inline constexpr NativeFamilyRecipe nativeFamilyAttack(uint32_t attack){
    for(const auto model:{1520u,1250u,1323u})for(unsigned step=1;step<=4;++step){
        const auto recipe=nativeFamilyRecipe(model,step);if(recipe.attack==attack&&attack)return recipe;}
    return {};
}
inline constexpr bool supportedNativeFamilyAttack(uint32_t model,uint32_t attack,uint32_t flags){
    const auto r=nativeFamilyAttack(attack);return r.attack&&(r.model==model||(r.model==1323&&knownHammerModel(model)))&&r.flags==flags;
}
inline constexpr bool matchesFamilyDefinition(uint32_t attack,const DirectWeaponDefinition& d){
    const auto r=nativeFamilyAttack(attack);
    return r.attack&&d.listenerCount==(r.scripted?1u:0u)&&d.script==(r.scripted?1077u:1u)
        &&d.kind==1&&!d.field1bc&&!d.field1f8&&!d.field1fc&&!d.field200
        &&d.field208==r.field208&&d.field20c==0x01000104;
}
// A per-hand VR chain: each committed deliberate strike advances once. Timeout
// is an Amalur adaptation, not a recovered native animation/input timing rule.
struct NativeFamilyChain {
    uint32_t weapon{},model{};unsigned generation{},step{};uint64_t last{};
    void reset(){*this={};}
    NativeFamilyRecipe preview(uint32_t held,uint32_t asset,unsigned center,uint64_t now)const{
        if(!held||!now)return {};
        unsigned next=(held==weapon&&asset==model&&center==generation&&now>=last&&now-last<=1100)?step+1:1;
        if(!nativeFamilyRecipe(asset,next).attack)next=1;
        return nativeFamilyRecipe(asset,next);
    }
    NativeFamilyRecipe commit(uint32_t held,uint32_t asset,unsigned center,uint64_t now){
        const auto r=preview(held,asset,center,now);if(!r.attack){reset();return {};}
        weapon=held;model=asset;generation=center;last=now;step=r.step;return r;
    }
};
struct NativeSwingSounds {uint32_t motion{},voice{},supplemental{};};
inline constexpr NativeSwingSounds nativeSwingSounds(uint32_t model,uint32_t attack,uint32_t flags){
    if(knownLongswordModel(model)&&supportedLongswordDamage(attack,flags))
        // Native81 contains the ordinary pair plus this extra event at129ms:
        // frozen native-reference20260921 traces898/939, both start success1,
        // modeFFFFFFFF flags1/0. Audible role needs listening; not an invented
        // finisher voice or an assertion that this is a contact-impact sound.
        return {0x00657cab,attack==7?0x01edd57bu:0x0104d903u,attack==81?0x00b40f89u:0u};
    if(!supportedNativeFamilyAttack(model,attack,flags))return {};
    if(model==1520)return {0x0078678b,attack==200?0u:0x01c9d985u};
    if(model==1250)return {0x012e49bf,attack==420?0x01edd57bu:0x0104d903u};
    if(knownHammerModel(model))return {attack==16?0x00fca83bu:attack==17?0x00e72c9au:0x00e78ed9u,
        attack==18?0x01edd57bu:0x0104d903u};
    return {};
}
}
