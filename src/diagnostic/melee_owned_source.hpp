#pragma once
#include "melee_native.hpp"
#include "../tracking/physical_melee_recipe.hpp"
#include "../tracking/longsword_damage_recipe.hpp"

namespace melee_owned_source {
inline uintptr_t resident(uintptr_t manager,uint32_t index){
    if(!manager||index<2||index>=1000000)return 0;
    auto flags=player_rig::word(manager+0x28),table=player_rig::word(manager+0x18);
    if(!flags||!table)return 0;
    auto flag=*reinterpret_cast<const unsigned char*>(flags+index);
    return (flag&4)&&!(flag&16)?player_rig::word(table+index*4):0;
}
inline bool reference(uintptr_t manager,uint32_t asset,bool retain){
    __try{
        if(!manager||asset<2)return false;
        auto vtable=player_rig::word(manager);
        auto fn=player_rig::word(vtable+(retain?0xa4:0xa8));
        if(fn<gameBase||fn>=gameBase+0x1320000)return false;
        reinterpret_cast<void(__thiscall*)(void*,uint32_t)>(fn)(reinterpret_cast<void*>(manager),asset);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline const char* assetRejection(uintptr_t definition,bool verifyScript,uint32_t longswordAsset=0){
    if(!definition)return "resident-missing";
    if(player_rig::word(definition)!=gameBase+0x135807c)return "definition-type";
    if(longswordAsset&&longswordAsset!=50){
        amalur::DirectWeaponDefinition d{player_rig::word(definition+0xc),player_rig::word(definition+0x94),player_rig::word(definition+0x1a0),
            player_rig::word(definition+0x1bc),player_rig::word(definition+0x1f8),player_rig::word(definition+0x1fc),player_rig::word(definition+0x200),
            player_rig::word(definition+0x208),player_rig::word(definition+0x20c)};
        return amalur::matchesLongswordDirectDefinition(longswordAsset,d)?nullptr:"longsword-direct-definition";
    }
    if(player_rig::word(definition+0xc)!=1||player_rig::word(definition+0x1a0)!=1
        ||player_rig::word(definition+0x1bc)||player_rig::word(definition+0x1f8)
        ||player_rig::word(definition+0x1fc)||player_rig::word(definition+0x200)
        ||player_rig::word(definition+0x208)!=0x0100e002
        ||player_rig::word(definition+0x20c)!=0x01000104)return "definition-fields";
    auto listeners=player_rig::word(definition+8),listener=listeners?player_rig::word(listeners):0;
    if(!listener||player_rig::word(listener+4)!=definition
        ||player_rig::word(player_rig::word(listener)+4)!=gameBase+0xbb4eb0)return "listener-identity";
    auto scripts=player_rig::word(gameBase+0x15f4d34);
    auto script=resident(scripts,player_rig::word(definition+0x94));
    if(!script)return "script-resident-missing";
    if(player_rig::word(script)!=gameBase+0x1331a6c)return "script-type";
    auto name=player_rig::word(script+0x14);
    if(!name||player_rig::word(name+4)!=17
        ||memcmp(reinterpret_cast<void*>(player_rig::word(name)),"Atk_Parent_Weapon",17))return "script-name";
    if(!verifyScript)return nullptr;
    if(player_rig::word(script+0x24)!=1174)return "script-length";
    auto bytes=reinterpret_cast<const unsigned char*>(player_rig::word(script+0x20));if(!bytes)return "script-bytes-missing";
    uint32_t hash=2166136261u;for(unsigned i=0;i<1174;++i)hash=(hash^bytes[i])*16777619u;
    return hash==0x41950419u?nullptr:"script-hash";
}
inline bool supportedAsset(uintptr_t definition,bool verifyScript){return !assetRejection(definition,verifyScript);}
struct Lease {
    uintptr_t manager{},definition{};
    uint32_t owner{},weapon{},asset{},flags{};
    uint64_t epoch{};
    bool held{},longswordRoute{};
    bool current(uint32_t currentOwner,uint32_t currentWeapon)const{
        return held&&owner==currentOwner&&weapon==currentWeapon
            &&player_rig::word(gameBase+0x15f4dfc)==manager
            &&resident(manager,asset)==definition&&!assetRejection(definition,false,longswordRoute?asset:0);
    }
    bool clear(){
        auto oldManager=manager,oldDefinition=definition;auto oldAsset=asset;
        bool release=held;held=false;longswordRoute=false;manager=definition=0;owner=weapon=asset=flags=0;++epoch;
        if(!release)return true;
        // World replacement may invalidate an entire resource manager. Never
        // call through that stale manager; the caller quarantines the session.
        if(player_rig::word(gameBase+0x15f4dfc)!=oldManager
            ||resident(oldManager,oldAsset)!=oldDefinition)return false;
        return reference(oldManager,oldAsset,false);
    }
    bool acquireLongsword(uint32_t newOwner,uint32_t newWeapon,uint32_t newAsset,uintptr_t expected,uint32_t newFlags){
        if(!amalur::supportedLongswordDamage(newAsset,newFlags))return false;
        return acquireChecked(newOwner,newWeapon,newAsset,expected,newFlags,true);
    }
    bool acquire(uint32_t newOwner,uint32_t newWeapon,uint32_t newAsset,uintptr_t expected,uint32_t newFlags){
        if(!amalur::supportedPhysicalAttack(newAsset,newFlags))return false;
        return acquireChecked(newOwner,newWeapon,newAsset,expected,newFlags,false);
    }
private:
    bool acquireChecked(uint32_t newOwner,uint32_t newWeapon,uint32_t newAsset,uintptr_t expected,uint32_t newFlags,bool longsword){
        if(!newOwner||!newWeapon||assetRejection(expected,true,longsword?newAsset:0))return false;
        if(current(newOwner,newWeapon)&&asset==newAsset&&longswordRoute==longsword){flags=newFlags;return true;}
        if(!clear())return false;
        auto resources=player_rig::word(gameBase+0x15f4dfc);
        if(resident(resources,newAsset)!=expected||!reference(resources,newAsset,true))return false;
        manager=resources;definition=expected;asset=newAsset;owner=newOwner;weapon=newWeapon;
        flags=newFlags;held=true;longswordRoute=longsword;++epoch;
        if(current(newOwner,newWeapon))return true;
        clear();return false;
    }
};
}
