#pragma once
#include "../tracking/body_pose.hpp"
#include "../tracking/grip_settings.hpp"
namespace rig_probe {inline uintptr_t playerRoot();}
namespace arm_rig {
inline std::atomic<bool> enabled{true};
inline std::atomic<unsigned> samples{0};
inline SRWLOCK bodyLock=SRWLOCK_INIT;
inline mgs5vr::Vec3 headAnchor{};inline uint64_t headTick{};
inline void sampleBody(mgs5vr::Vec3 position,uint64_t tick){
    AcquireSRWLockExclusive(&bodyLock);headAnchor=position;headTick=tick;ReleaseSRWLockExclusive(&bodyLock);
}
struct Scratch {amalur::RigBone bones[64];uintptr_t descriptor[3];};
inline SRWLOCK calibrationLock=SRWLOCK_INIT;
inline uintptr_t calibratedRoot{};inline uint32_t calibratedOwner{};inline unsigned calibratedCenter{};
inline amalur::GripSettingsChannel gripSettings;
inline float gripPitch{},gripYaw{},gripRoll{};
inline amalur::ArmReference neutralArm{};
inline bool calibratedBodyAnchor{};
inline bool solveUnsafe(uintptr_t root,Scratch& scratch,bool solveHand=true){
    if(!headTracking.load()||!root||root!=rig_probe::playerRoot())return false;
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
    auto now=GetTickCount64();bool handValid=solveHand&&enabled.load()&&timestamp&&timestamp<=now&&now-timestamp<250&&mgs5vr::valid(grip);
    mgs5vr::Pose worldRoot;
    memcpy(&worldRoot.position,reinterpret_cast<void*>(root+0x124),12);
    memcpy(&worldRoot.orientation,reinterpret_cast<void*>(root+0x134),16);
    worldRoot=amalur::nativePose(worldRoot);
    if(!mgs5vr::valid(worldRoot))return false;
    amalur::RigBone native[64];memcpy(native,reinterpret_cast<void*>(buffer),count*sizeof(amalur::RigBone));
    bool bodyApplied=false;mgs5vr::Vec3 anchor,localAnchor{};uint64_t bodyTick;
    AcquireSRWLockShared(&bodyLock);anchor=headAnchor;bodyTick=headTick;ReleaseSRWLockShared(&bodyLock);
    if(firstPerson.load()&&bodyTick&&bodyTick<=now&&now-bodyTick<250){
        auto local=mgs5vr::compose(mgs5vr::inverse(worldRoot),mgs5vr::Pose{{},anchor});
        localAnchor=local.position;
        amalur::RigBone stable[64];
        if(amalur::stabilizeBody(native,stable,count,parents,ids,local.position)){
            memcpy(native,stable,count*sizeof(amalur::RigBone));bodyApplied=true;
        }
    }
    if(!handValid){
        if(!bodyApplied)return false;
        memcpy(scratch.bones,native,count*sizeof(amalur::RigBone));
        memcpy(scratch.descriptor,reinterpret_cast<void*>(source),sizeof(scratch.descriptor));
        scratch.descriptor[0]=reinterpret_cast<uintptr_t>(scratch.bones);++samples;return true;
    }
    unsigned wrist=count;for(unsigned i=0;i<count;++i)if(ids[i]==0x0088d0eb)wrist=i;
    if(wrist==count||!mgs5vr::valid(amalur::bonePose(native[wrist])))return false;
    auto rootOwner=player_rig::word(root+0xf8);mgs5vr::Pose alignment;amalur::ArmReference reference;
    amalur::ArmReference candidate;
    const bool lockArm=bodyApplied||weapon_control::desktopPose.load();
    if(lockArm)amalur::captureRightArmReference(native,count,parents,ids,localAnchor,candidate);
    AcquireSRWLockExclusive(&calibrationLock);
    if(calibratedRoot!=root||calibratedOwner!=rootOwner||calibratedBodyAnchor!=bodyApplied){
        calibratedRoot=root;calibratedOwner=rootOwner;calibratedBodyAnchor=bodyApplied;
        neutralArm={};
    }
    calibratedCenter=center;
    if(lockArm&&!neutralArm.ready)neutralArm=candidate;
    if(gripSettings.open(false))gripSettings.read(gripPitch,gripYaw,gripRoll);
    amalur::gripAngleTrim(gripPitch,gripYaw,gripRoll,alignment);
    reference=neutralArm;ReleaseSRWLockExclusive(&calibrationLock);
    auto target=mgs5vr::compose(mgs5vr::inverse(worldRoot),mgs5vr::compose(grip,alignment));
    if(!amalur::solveRightArm(native,scratch.bones,count,parents,ids,target,scale,
        lockArm&&reference.ready?&reference:nullptr,localAnchor))return false;
    // Remapper reads this private copy synchronously. Never edit the authoritative
    // root animation or feed last frame's solved bones back into the solver.
    memcpy(scratch.descriptor,reinterpret_cast<void*>(source),sizeof(scratch.descriptor));
    scratch.descriptor[0]=reinterpret_cast<uintptr_t>(scratch.bones);
    ++samples;return true;
}
inline bool prepareUnsafe(uintptr_t source,uintptr_t output,Scratch& scratch){
    if(!headTracking.load()||(!enabled.load()&&!firstPerson.load()))return false;
    auto root=rig_probe::playerRoot();if(!root||source!=root+0x34||output<0x34)return false;
    auto object=output-0x34;auto children=player_rig::word(root+0x24),childCount=player_rig::word(root+0x28);
    if(childCount>32)return false;bool owned=false;
    for(unsigned i=0;i<childCount;++i)if(weapon_control::fab(player_rig::word(children+i*4))==object){owned=true;break;}
    if(!owned)return false;
    // Held weapon maps include finger bones (staff slot 5: root55 -> weapon1).
    // Feed the same solved arm to armor and weapons so they cannot diverge.
    auto owner=player_rig::word(object+0xf8);
    auto entity=player_rig::resolve(owner);
    bool weapon=player_rig::part(entity,11,owner,0x135745c)!=0;
    if(!weapon&&!player_rig::part(entity,12,owner,0x13563e4)
       &&!player_rig::part(entity,40,owner,0x1356bec))return false;
    return solveUnsafe(root,scratch);
}
inline bool prepare(uintptr_t source,uintptr_t output,Scratch& scratch){
    __try{return prepareUnsafe(source,output,scratch);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void resetCalibration(){AcquireSRWLockExclusive(&calibrationLock);calibratedRoot=0;ReleaseSRWLockExclusive(&calibrationLock);}
inline uintptr_t trackedWeaponSlot(void* mapper,uintptr_t slot,uintptr_t output,bool solved){
    __try {
        if(!solved||slot!=8||!firstPerson.load()||!enabled.load()||output<0x34
            ||motion_controls::viewControls().selectedWeapon!=0)return slot;
        AcquireSRWLockShared(&weapon_control::poseLock);auto tick=weapon_control::tick;ReleaseSRWLockShared(&weapon_control::poseLock);
        auto now=GetTickCount64();if(!tick||tick>now||now-tick>=250)return slot;
        auto object=output-0x34;if(!weapon_control::isSinglePlayerWeapon(object)||player_rig::word(output+4)!=4)return slot;
        auto manager=player_rig::word(gameBase+0x15fdf54),assetId=player_rig::word(object+0xf0);
        if(assetId<2||assetId>=100000)return slot;
        auto state=*reinterpret_cast<unsigned char*>(player_rig::word(manager+0x28)+assetId);
        if(!(state&4)||(state&0x10))return slot;
        auto asset=player_rig::word(player_rig::word(manager+0x18)+assetId*4),blob=player_rig::word(asset+0x1c);
        if(player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=4)return slot;
        auto offset=player_rig::word(blob+0x20);if(!offset||offset>65536)return slot;
        // Verified four-bone staff layout. Other weapon types keep native slots.
        constexpr uint32_t staffIds[]{11436941,6711025,12316630,8749139};
        if(memcmp(reinterpret_cast<void*>(blob+0x20+offset),staffIds,sizeof(staffIds)))return slot;
        auto table=player_rig::word(reinterpret_cast<uintptr_t>(mapper));
        auto stowed=table+8*32,held=table+5*32;
        if(player_rig::word(stowed+4)!=1||player_rig::word(held+4)!=2)return slot;
        constexpr uint32_t stowedMap[]{62,0,0},heldMap[]{62,0,0,55,1,0};
        if(memcmp(reinterpret_cast<void*>(player_rig::word(stowed)),stowedMap,sizeof(stowedMap))
            ||memcmp(reinterpret_cast<void*>(player_rig::word(held)),heldMap,sizeof(heldMap)))return slot;
        // Visual remap only: native equipment, attack timers and events still run.
        return 5;
    } __except(EXCEPTION_EXECUTE_HANDLER){return slot;}
}
}
