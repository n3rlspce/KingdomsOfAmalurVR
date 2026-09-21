#pragma once
#include "../tracking/shield_orientation.hpp"
namespace shield_control {
struct Identity {
    uintptr_t object{},root{},buffer{};uint32_t index{},owner{},rootOwner{},asset{};
};
struct Saved {Identity identity;amalur::RigBone before{},after{};};
inline Saved saved;
inline bool identify(uintptr_t object,Identity& result){
    auto root=weapon_control::currentWeaponRoot();if(!root||!object)return false;
    auto index=player_rig::word(object+0x194);
    if(weapon_control::fab(index)!=object||player_rig::word(object+0x38)!=2)return false;
    auto children=player_rig::word(root+0x24),count=player_rig::word(root+0x28);
    if(!children||count>32)return false;
    bool child=false;for(unsigned i=0;i<count;++i)
        if(weapon_control::fab(player_rig::word(children+i*4))==object){child=true;break;}
    if(!child)return false;
    auto owner=player_rig::word(object+0xf8);if(!player_rig::resolve(owner))return false;
    auto id=player_rig::word(object+0xf0),manager=player_rig::word(gameBase+0x15fdf54);
    if(!manager||id<2||id>=100000)return false;
    auto states=player_rig::word(manager+0x28),table=player_rig::word(manager+0x18);
    if(!states||!table)return false;
    auto state=*reinterpret_cast<const unsigned char*>(states+id);if(!(state&4)||(state&16))return false;
    auto asset=player_rig::word(table+id*4),blob=asset?player_rig::word(asset+0x1c):0;
    if(!blob||player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=2)return false;
    auto ids=player_rig::word(blob+0x20),parents=player_rig::word(blob+0x1c);
    if(!ids||ids>65536||!parents||parents>65536)return false;
    constexpr uint32_t expectedIds[]{6711025,17330092};constexpr int16_t expectedParents[]{-1,0};
    if(memcmp(reinterpret_cast<void*>(blob+0x20+ids),expectedIds,sizeof(expectedIds))
        ||memcmp(reinterpret_cast<void*>(blob+0x1c+parents),expectedParents,sizeof(expectedParents)))return false;
    auto buffer=player_rig::word(object+0x34);if(!buffer)return false;
    result={object,root,buffer,index,owner,player_rig::word(root+0xf8),id};return true;
}
inline bool same(const Identity& a,const Identity& b){
    return a.object&&a.object==b.object&&a.root==b.root&&a.buffer==b.buffer&&a.index==b.index
        &&a.owner==b.owner&&a.rootOwner==b.rootOwner&&a.asset==b.asset;
}
inline void restore(uintptr_t object){
    __try{
        if(!saved.identity.object)return;
        auto live=weapon_control::fab(saved.identity.index);Identity current;
        if(live!=saved.identity.object||!identify(live,current)||!same(saved.identity,current)){saved={};return;}
        if(object!=live)return;
        amalur::restoreShieldOrientation(reinterpret_cast<amalur::RigBone*>(current.buffer)[1],saved.before,saved.after);
        saved={};
    }__except(EXCEPTION_EXECUTE_HANDLER){saved={};}
}
inline void apply(uintptr_t object,uintptr_t source,bool solved){
    __try{
        if(saved.identity.object||!firstPerson.load()||!headTracking.load()||interfaceView.load()
            ||!motion_controls::gameFocused())return;
        Identity identity;if(!identify(object,identity)||source!=identity.root+0x34)return;
        if(!solved){static bool logged=false;if(!logged){logged=true;log("Shield upright correction waiting for tracked attachment solve\n");}return;}
        mgs5vr::Pose left;uint64_t tick;
        AcquireSRWLockShared(&weapon_control::poseLock);left=weapon_control::desiredLeft;tick=weapon_control::leftTick;ReleaseSRWLockShared(&weapon_control::poseLock);
        if(!amalur::freshWeaponPose(left,tick,GetTickCount64()))return;
        auto bones=reinterpret_cast<amalur::RigBone*>(identity.buffer);amalur::RigBone candidate[2];memcpy(candidate,bones,sizeof(candidate));
        const auto before=candidate[1];constexpr uint32_t ids[]{6711025,17330092};constexpr int16_t parents[]{-1,0};
        if(!amalur::uprightShield(candidate,2,ids,parents))return;
        saved={identity,before,candidate[1]};
        bones[1].orientation=candidate[1].orientation;
        bones[1].opaque[12]=candidate[1].opaque[12];
        static bool logged=false;if(!logged){logged=true;log("Shield upright correction applied asset=%u owner=%08x\n",identity.asset,identity.owner);}
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
}
