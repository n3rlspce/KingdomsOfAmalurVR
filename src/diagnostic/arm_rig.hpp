#pragma once
#include "../tracking/arm_pose.hpp"
namespace rig_probe {inline uintptr_t playerRoot();}
namespace arm_rig {
inline std::atomic<bool> enabled{true};
inline std::atomic<unsigned> samples{0};
struct Scratch {amalur::RigBone bones[64];uintptr_t descriptor[3];};
inline SRWLOCK calibrationLock=SRWLOCK_INIT;
inline uintptr_t calibratedRoot{};inline uint32_t calibratedOwner{};inline unsigned calibratedCenter{};
inline mgs5vr::Pose trim{};
inline bool solveUnsafe(uintptr_t root,Scratch& scratch){
    if(!enabled.load()||!headTracking.load()||!root||root!=rig_probe::playerRoot())return false;
    auto source=root+0x34;
    auto count=player_rig::word(source+4);if(count<3||count>64)return false;
    auto buffer=player_rig::word(source);if(!buffer)return false;
    auto manager=player_rig::word(gameBase+0x15fdf54),assetId=player_rig::word(root+0xf0);
    if(assetId<2||assetId>=100000)return false;
    auto flags=*reinterpret_cast<unsigned char*>(player_rig::word(manager+0x28)+assetId);
    if(!(flags&4)||(flags&0x10))return false;
    auto asset=player_rig::word(player_rig::word(manager+0x18)+assetId*4),blob=player_rig::word(asset+0x1c);
    if(player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=count)return false;
    auto parentOffset=player_rig::word(blob+0x1c),idOffset=player_rig::word(blob+0x20);
    if(!parentOffset||parentOffset>65536||!idOffset||idOffset>65536)return false;
    auto parents=reinterpret_cast<const int16_t*>(blob+0x1c+parentOffset);
    auto ids=reinterpret_cast<const uint32_t*>(blob+0x20+idOffset);
    mgs5vr::Pose grip;uint64_t timestamp;unsigned center;float scale;
    AcquireSRWLockShared(&weapon_control::poseLock);
    grip=weapon_control::desired;timestamp=weapon_control::tick;center=weapon_control::generation;scale=weapon_control::worldScale;
    ReleaseSRWLockShared(&weapon_control::poseLock);
    auto now=GetTickCount64();if(!timestamp||timestamp>now||now-timestamp>150)return false;
    mgs5vr::Pose worldRoot;
    memcpy(&worldRoot.position,reinterpret_cast<void*>(root+0x124),12);
    memcpy(&worldRoot.orientation,reinterpret_cast<void*>(root+0x134),16);
    if(!mgs5vr::valid(worldRoot)||!mgs5vr::valid(grip))return false;
    amalur::RigBone native[64];memcpy(native,reinterpret_cast<void*>(buffer),count*sizeof(amalur::RigBone));
    unsigned wrist=count;for(unsigned i=0;i<count;++i)if(ids[i]==0x0088d0eb)wrist=i;
    if(wrist==count||!mgs5vr::valid(amalur::bonePose(native[wrist])))return false;
    auto rootOwner=player_rig::word(root+0xf8);mgs5vr::Pose alignment;
    AcquireSRWLockExclusive(&calibrationLock);
    if(calibratedRoot!=root||calibratedOwner!=rootOwner||calibratedCenter!=center){
        trim=mgs5vr::compose(mgs5vr::inverse(grip),mgs5vr::compose(worldRoot,amalur::bonePose(native[wrist])));
        trim.position={};calibratedRoot=root;calibratedOwner=rootOwner;calibratedCenter=center;
    }
    alignment=trim;ReleaseSRWLockExclusive(&calibrationLock);
    auto target=mgs5vr::compose(mgs5vr::inverse(worldRoot),mgs5vr::compose(grip,alignment));
    if(!amalur::solveRightArm(native,scratch.bones,count,parents,ids,target,scale))return false;
    // Remapper reads this private copy synchronously. Never edit the authoritative
    // root animation or feed last frame's solved bones back into the solver.
    memcpy(scratch.descriptor,reinterpret_cast<void*>(source),sizeof(scratch.descriptor));
    scratch.descriptor[0]=reinterpret_cast<uintptr_t>(scratch.bones);
    ++samples;return true;
}
inline bool prepareUnsafe(uintptr_t source,uintptr_t output,Scratch& scratch){
    if(!enabled.load()||!headTracking.load())return false;
    auto root=rig_probe::playerRoot();if(!root||source!=root+0x34||output<0x34)return false;
    auto object=output-0x34;auto children=player_rig::word(root+0x24),childCount=player_rig::word(root+0x28);
    if(childCount>32)return false;bool owned=false;
    for(unsigned i=0;i<childCount;++i)if(weapon_control::fab(player_rig::word(children+i*4))==object){owned=true;break;}
    if(!owned)return false;
    // Only player armor meshes. Weapon attachment state remains native.
    auto owner=player_rig::word(object+0xf8);
    if(!player_rig::part(player_rig::resolve(owner),12,owner,0x13563e4))return false;
    return solveUnsafe(root,scratch);
}
inline bool prepare(uintptr_t source,uintptr_t output,Scratch& scratch){
    __try{return prepareUnsafe(source,output,scratch);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void resetCalibration(){AcquireSRWLockExclusive(&calibrationLock);calibratedRoot=0;ReleaseSRWLockExclusive(&calibrationLock);}
}
