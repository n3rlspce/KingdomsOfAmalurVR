#pragma once
#include "../tracking/body_pose.hpp"
#include "../tracking/visual_root.hpp"
#include "../tracking/grip_settings.hpp"
#include "arm_trace.hpp"
#include "support_grip.hpp"
#include <new>
namespace rig_probe {inline uintptr_t playerRoot();}
namespace arm_rig {
inline std::atomic<bool> enabled{true};
inline amalur::VisualRootRotation visualRootRotation;
inline std::atomic<unsigned> samples{0};
inline SRWLOCK bodyLock=SRWLOCK_INIT;
inline mgs5vr::Vec3 headAnchor{};inline uint64_t headTick{};
inline void sampleBody(mgs5vr::Vec3 position,uint64_t tick){
    AcquireSRWLockExclusive(&bodyLock);headAnchor=position;headTick=tick;ReleaseSRWLockExclusive(&bodyLock);
}
struct Scratch {amalur::LocomotionFrame locomotion;amalur::RigBone bones[64];uintptr_t descriptor[3];amalur::ArmTraceRecord trace{};
    alignas(amalur::skin_audit::CpuRecord) unsigned char auditBytes[sizeof(amalur::skin_audit::CpuRecord)];
    amalur::skin_audit::CpuRecord* audit{};};
inline SRWLOCK calibrationLock=SRWLOCK_INIT;
inline uintptr_t calibratedRoot{};inline uint32_t calibratedOwner{};inline unsigned calibratedCenter{};
inline amalur::GripSettingsChannel gripSettings;
inline float gripPitch{},gripYaw{},gripRoll{};
inline amalur::ArmReference neutralArm{};
inline amalur::ArmReference neutralLeftArm{};
inline bool calibratedBodyAnchor{};
inline amalur::BodyReference neutralBody{};
inline LONG lastAblationMask=-1;
inline bool solveUnsafe(uintptr_t root,Scratch& scratch,bool solveHand=true,uint32_t origin=amalur::skin_audit::Remap){
    if(interfaceView.load()||!headTracking.load()||!root||root!=rig_probe::playerRoot())return false;
    auto& trace=scratch.trace;trace.root=static_cast<uint32_t>(root);trace.frame=presents.load();trace.tick=GetTickCount64();trace.stage=1;
    skin_trace::ensure();
    const auto animationModes=amalur::bodyDebug.read();
    const auto ablations=animationModes&amalur::rigAblationMask;
    AcquireSRWLockExclusive(&calibrationLock);
    if(lastAblationMask!=ablations){
        neutralBody={};neutralArm={};neutralLeftArm={};calibratedRoot=0;visualRootRotation={};
        lastAblationMask=ablations;
        log("Rig ablations: skipSockets=%d skipRootSmoothing=%d nativeMeshInput=%d nativePositions=%d nativeRotations=%d\n",
            !!(ablations&amalur::skipRigSockets),!!(ablations&amalur::skipRootSmoothing),!!(ablations&amalur::nativeMeshInput),!!(ablations&amalur::nativeMeshPositions),!!(ablations&amalur::nativeMeshRotations));
    }
    ReleaseSRWLockExclusive(&calibrationLock);
    const bool nativeTorso=(animationModes&amalur::nativeTorso)!=0;
    const bool nativeArms=(animationModes&amalur::nativeArms)!=0;
    if(nativeTorso&&nativeArms)return false;
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
    if(skin_trace::reserveCpu(origin)){
        scratch.audit=new (scratch.auditBytes) amalur::skin_audit::CpuRecord{};
        auto& a=*scratch.audit;a.origin=origin;a.thread=GetCurrentThreadId();a.asset=assetId;
        trace.owner=player_rig::word(root+0xf8);
        a.sourceBuffer=buffer;a.count=count;a.modes=animationModes;a.solveId=++skin_trace::solveSerial;
        LARGE_INTEGER clock{};QueryPerformanceCounter(&clock);a.qpc=clock.QuadPart;
        memcpy(&a.rootWorldRaw,reinterpret_cast<void*>(root+0x124),sizeof(a.rootWorldRaw));
        memcpy(a.ids,ids,count*sizeof(uint32_t));memcpy(a.parents,parents,count*sizeof(int16_t));
        a.layoutHash=amalur::skin_audit::hash(a.parents,count*sizeof(int16_t),amalur::skin_audit::hash(a.ids,count*sizeof(uint32_t)));
        AcquireSRWLockShared(&calibrationLock);a.bodyBefore=neutralBody.revision;a.visualBefore=visualRootRotation.frame;ReleaseSRWLockShared(&calibrationLock);
    }
    mgs5vr::Pose grip,leftGrip;uint64_t timestamp,leftTimestamp;unsigned center;float scale;
    AcquireSRWLockShared(&weapon_control::poseLock);
    scratch.locomotion=weapon_control::locomotionFrame;
    grip=weapon_control::desired;timestamp=weapon_control::tick;center=weapon_control::generation;scale=weapon_control::worldScale;
    leftGrip=weapon_control::desiredLeft;leftTimestamp=weapon_control::leftTick;
    auto rawPose=[](const amalur::PosePacket& p){return mgs5vr::Pose{{p.orientation[0],p.orientation[1],p.orientation[2],p.orientation[3]},{p.position[0],p.position[1],p.position[2]}};};
    trace.raw[0]=rawPose(weapon_control::frameRight);trace.raw[1]=rawPose(weapon_control::frameLeft);
    trace.rawRightTick=weapon_control::frameRight.tick;trace.rawLeftTick=weapon_control::frameLeft.tick;
    ReleaseSRWLockShared(&weapon_control::poseLock);
    if(scratch.audit)scratch.audit->center=center;
    auto now=GetTickCount64();bool handValid=solveHand&&!nativeArms&&enabled.load()&&amalur::freshWeaponPose(grip,timestamp,now);
    bool leftValid=solveHand&&!nativeArms&&enabled.load()&&amalur::freshWeaponPose(leftGrip,leftTimestamp,now);
    trace.target[0]=grip;trace.target[1]=leftGrip;trace.rightTick=timestamp;trace.leftTick=leftTimestamp;
    trace.flags=(handValid?1u:0u)|(leftValid?2u:0u);trace.stage=2;
    mgs5vr::Pose worldRoot;
    memcpy(&worldRoot.position,reinterpret_cast<void*>(root+0x124),12);
    memcpy(&worldRoot.orientation,reinterpret_cast<void*>(root+0x134),16);
    worldRoot=amalur::nativePose(worldRoot);
    if(!mgs5vr::valid(worldRoot))return false;
    trace.rootWorld=worldRoot;
    const auto nativeWorldRoot=worldRoot;
    if(firstPerson.load()&&!nativeTorso&&!(ablations&amalur::skipRootSmoothing)){
        LARGE_INTEGER counter{},frequency{};QueryPerformanceCounter(&counter);QueryPerformanceFrequency(&frequency);
        const uint64_t identity=(uint64_t(player_rig::word(root+0xf8))<<32)|root;
        AcquireSRWLockExclusive(&calibrationLock);
        worldRoot=visualRootRotation.sample(worldRoot,identity,center,presents.load(),double(counter.QuadPart)/frequency.QuadPart);
        ReleaseSRWLockExclusive(&calibrationLock);
    }
    amalur::RigBone native[64];memcpy(native,reinterpret_cast<void*>(buffer),count*sizeof(amalur::RigBone));
    if(scratch.audit){memcpy(scratch.audit->original,native,count*sizeof(amalur::RigBone));
        scratch.audit->nativeHash=amalur::skin_audit::hash(native,count*sizeof(amalur::RigBone));}
    bool bodyApplied=false;mgs5vr::Vec3 anchor,localAnchor{};uint64_t bodyTick;
    AcquireSRWLockShared(&bodyLock);anchor=headAnchor;bodyTick=headTick;ReleaseSRWLockShared(&bodyLock);
    trace.head.position=anchor;trace.headTick=bodyTick;trace.stage=3;
    if(!nativeTorso&&firstPerson.load()&&bodyTick&&bodyTick<=now&&now-bodyTick<250){
        auto local=mgs5vr::compose(mgs5vr::inverse(worldRoot),mgs5vr::Pose{{},anchor});
        localAnchor=local.position;
        amalur::RigBone stable[64];
        const auto owner=player_rig::word(root+0xf8);
        AcquireSRWLockExclusive(&calibrationLock);
        const auto previousRevision=neutralBody.revision;
        const bool stabilized=amalur::stabilizeTrackedBody(native,stable,count,parents,ids,local.position,neutralBody,
            {static_cast<uint32_t>(root),owner,assetId,static_cast<uint32_t>(blob),center});
        if(stabilized&&neutralBody.revision!=previousRevision){neutralArm={};neutralLeftArm={};}
        ReleaseSRWLockExclusive(&calibrationLock);
        if(stabilized&&nativeArms&&!amalur::restoreNativeArmAnimation(native,stable,count,parents,ids))return false;
        if(stabilized){
            memcpy(native,stable,count*sizeof(amalur::RigBone));bodyApplied=true;
        }
    }
    if(!handValid&&!leftValid){
        if(!bodyApplied)return false;
        memcpy(scratch.bones,native,count*sizeof(amalur::RigBone));
        memcpy(scratch.descriptor,reinterpret_cast<void*>(source),sizeof(scratch.descriptor));
        scratch.descriptor[0]=reinterpret_cast<uintptr_t>(scratch.bones);
        amalur::rebaseVisualRig(scratch.bones,count,worldRoot,nativeWorldRoot);
        amalur::publishRigOverrides(reinterpret_cast<const amalur::RigBone*>(buffer),scratch.bones,count);
        if(origin==amalur::skin_audit::Remap){
            amalur::restoreNativeRigChannels(reinterpret_cast<const amalur::RigBone*>(buffer),scratch.bones,count,
                (ablations&amalur::nativeMeshPositions)!=0,(ablations&amalur::nativeMeshRotations)!=0);
            if(ablations&amalur::nativeMeshPositions)trace.flags|=512u;
            if(ablations&amalur::nativeMeshRotations)trace.flags|=1024u;
        }
trace.stage=8;trace.flags|=4u;++samples;return true;
    }
    unsigned wrist=count;for(unsigned i=0;i<count;++i)if(ids[i]==0x0088d0eb)wrist=i;
    if(wrist==count||!mgs5vr::valid(amalur::bonePose(native[wrist])))return false;
    auto rootOwner=player_rig::word(root+0xf8);mgs5vr::Pose alignment;amalur::ArmReference reference;
    amalur::ArmReference candidate,leftCandidate,leftReference;
    const bool lockArm=bodyApplied||weapon_control::desktopPose.load();
    trace.flags|=bodyApplied?4u:0u;trace.owner=rootOwner;trace.stage=4;
    if(lockArm)amalur::captureRightArmReference(native,count,parents,ids,localAnchor,candidate);
    if(lockArm)amalur::captureLeftArmReference(native,count,parents,ids,localAnchor,leftCandidate);
    AcquireSRWLockExclusive(&calibrationLock);
    if(calibratedRoot!=root||calibratedOwner!=rootOwner||calibratedBodyAnchor!=bodyApplied||calibratedCenter!=center){
        calibratedRoot=root;calibratedOwner=rootOwner;calibratedBodyAnchor=bodyApplied;
        neutralArm={};neutralLeftArm={};
    }
    calibratedCenter=center;
    if(lockArm&&!neutralArm.ready)neutralArm=candidate;
    if(lockArm&&!neutralLeftArm.ready)neutralLeftArm=leftCandidate;
    if(gripSettings.open(false))gripSettings.read(gripPitch,gripYaw,gripRoll);
    amalur::gripAngleTrim(gripPitch,gripYaw,gripRoll,alignment);
    reference=neutralArm;leftReference=neutralLeftArm;ReleaseSRWLockExclusive(&calibrationLock);
    auto target=mgs5vr::compose(mgs5vr::inverse(worldRoot),
        amalur::wristFromControllerGrip(amalur::ArmSide::Right,mgs5vr::compose(grip,alignment)));
    trace.target[0]=mgs5vr::compose(worldRoot,target);
    memcpy(scratch.bones,native,count*sizeof(amalur::RigBone));
    trace.stage=5;
    if(handValid&&!amalur::solveRightArm(native,scratch.bones,count,parents,ids,target,scale,
        lockArm&&reference.ready?&reference:nullptr,localAnchor))return false;
    if(leftValid){
        trace.stage=6;
        mgs5vr::Pose leftAlignment;amalur::gripAngleTrim(gripPitch,-gripYaw,-gripRoll,leftAlignment);
        auto leftTarget=mgs5vr::compose(mgs5vr::inverse(worldRoot),
            amalur::wristFromControllerGrip(amalur::ArmSide::Left,mgs5vr::compose(leftGrip,leftAlignment)));
        trace.target[1]=mgs5vr::compose(worldRoot,leftTarget);
        amalur::RigBone both[64];
        if(!amalur::solveLeftArm(scratch.bones,both,count,parents,ids,leftTarget,scale,
            lockArm&&leftReference.ready?&leftReference:nullptr,localAnchor))return false;
        memcpy(scratch.bones,both,count*sizeof(amalur::RigBone));
    }
    support_grip::apply(scratch.bones,count,parents,ids,worldRoot,leftGrip,rootOwner,center,scale,handValid&&leftValid);
    // Remapper reads this private copy synchronously. Never edit the authoritative
    // root animation or feed last frame's solved bones back into the solver.
    memcpy(scratch.descriptor,reinterpret_cast<void*>(source),sizeof(scratch.descriptor));
    scratch.descriptor[0]=reinterpret_cast<uintptr_t>(scratch.bones);
    for(unsigned side=0;side<2;++side){
        unsigned shoulder,elbow,traceWrist;
        if(amalur::armIndices(side?amalur::ArmSide::Left:amalur::ArmSide::Right,count,parents,ids,shoulder,elbow,traceWrist)){
            trace.shoulder[side]=mgs5vr::compose(worldRoot,amalur::bonePose(scratch.bones[shoulder]));
            trace.elbow[side]=mgs5vr::compose(worldRoot,amalur::bonePose(scratch.bones[elbow]));
            trace.solved[side]=mgs5vr::compose(worldRoot,amalur::bonePose(scratch.bones[traceWrist]));
        }
    }
    amalur::rebaseVisualRig(scratch.bones,count,worldRoot,nativeWorldRoot);
        amalur::publishRigOverrides(reinterpret_cast<const amalur::RigBone*>(buffer),scratch.bones,count);
        if(origin==amalur::skin_audit::Remap){
            amalur::restoreNativeRigChannels(reinterpret_cast<const amalur::RigBone*>(buffer),scratch.bones,count,
                (ablations&amalur::nativeMeshPositions)!=0,(ablations&amalur::nativeMeshRotations)!=0);
            if(ablations&amalur::nativeMeshPositions)trace.flags|=512u;
            if(ablations&amalur::nativeMeshRotations)trace.flags|=1024u;
        }

    trace.stage=8;
    ++samples;return true;
}
inline void finishAudit(Scratch& scratch,bool solved){
    if(!scratch.audit)return;auto& a=*scratch.audit;if(!a.count)return;
    a.trace=scratch.trace;
    AcquireSRWLockShared(&calibrationLock);a.bodyAfter=neutralBody.revision;a.visualAfter=visualRootRotation.frame;ReleaseSRWLockShared(&calibrationLock);
    if(solved){memcpy(a.solved,scratch.bones,a.count*sizeof(amalur::RigBone));
        a.solvedHash=amalur::skin_audit::hash(a.solved,a.count*sizeof(amalur::RigBone));}
    skin_trace::cpu(a);
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
inline void resetCalibration(){AcquireSRWLockExclusive(&calibrationLock);calibratedRoot=0;neutralBody={};visualRootRotation={};ReleaseSRWLockExclusive(&calibrationLock);}
inline uintptr_t trackedWeaponSlot(void* mapper,uintptr_t slot,uintptr_t output,bool solved){
    __try {
        if(amalur::bodyDebug.enabled(amalur::nativeArms)||!solved||(slot!=8&&slot!=9&&slot!=11)||!firstPerson.load()||!enabled.load()||output<0x34
            ||motion_controls::viewControls().selectedWeapon>1)return slot;
        AcquireSRWLockShared(&weapon_control::poseLock);
        auto tick=weapon_control::tick,leftTick=weapon_control::leftTick;
        auto grip=weapon_control::desired,leftGrip=weapon_control::desiredLeft;
        ReleaseSRWLockShared(&weapon_control::poseLock);
        auto now=GetTickCount64();
        auto object=output-0x34;if(!weapon_control::isPlayerDaggers(object)&&!weapon_control::isOnlyActivePlayerWeapon(object))return slot;
        if(!amalur::trackedWeaponSelection(motion_controls::viewControls().selectedWeapon,weapon_control::isOnlyActivePlayerWeapon(object)))return slot;
        auto count=player_rig::word(output+4);if(count!=4&&count!=5&&count!=7)return slot;
        auto manager=player_rig::word(gameBase+0x15fdf54),assetId=player_rig::word(object+0xf0);
        if(assetId<2||assetId>=100000)return slot;
        auto state=*reinterpret_cast<unsigned char*>(player_rig::word(manager+0x28)+assetId);
        if(!(state&4)||(state&0x10))return slot;
        auto asset=player_rig::word(player_rig::word(manager+0x18)+assetId*4),blob=player_rig::word(asset+0x1c);
        if(player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=count)return slot;
        auto offset=player_rig::word(blob+0x20);if(!offset||offset>65536)return slot;
        auto table=player_rig::word(reinterpret_cast<uintptr_t>(mapper));
        // Scalding Daggers: one Fab, two blade branches. Native slot 7 maps
        // left finger 36 to blade 1 and right finger 55 to blade 4.
        constexpr uint32_t daggerIds[]{11436941,11992818,11092278,14407505,10760771,15110326,13087492};
        if(count==7&&(slot==9||slot==11)&&!memcmp(reinterpret_cast<void*>(blob+0x20+offset),daggerIds,sizeof(daggerIds))){
            auto held=table+7*32;
            constexpr uint32_t heldMap[]{62,0,0,36,1,0,55,4,0};
            if(player_rig::word(held+4)==3&&!memcmp(reinterpret_cast<void*>(player_rig::word(held)),heldMap,sizeof(heldMap)))
                return amalur::trackedDaggerSlot(slot,amalur::freshWeaponPose(grip,tick,now),
                    amalur::freshWeaponPose(leftGrip,leftTick,now));
            return slot;
        }
        // Captured primary melee representatives: only replace their stowed
        // slot after confirming the exact native held attachment table.
        auto kind=weapon_control::capturedHeldKind(object);
        if(kind!=amalur::HeldWeaponKind::None){
            auto chosen=amalur::chooseHeldWeaponSlot(kind,slot,amalur::freshWeaponPose(grip,tick,now),
                amalur::freshWeaponPose(leftGrip,leftTick,now));
            if(chosen!=slot){
                auto stowed=table+8*32,held=table+chosen*32;
                constexpr uint32_t stowedMap[]{62,0,0},singleMap[]{62,0,0,55,1,0},dualMap[]{62,0,0,36,1,0,55,4,0};
                bool dual=kind==amalur::HeldWeaponKind::Faeblades;
                if(player_rig::word(stowed+4)==1&&player_rig::word(held+4)==(dual?3u:2u)
                    &&!memcmp(reinterpret_cast<void*>(player_rig::word(stowed)),stowedMap,sizeof(stowedMap))
                    &&!memcmp(reinterpret_cast<void*>(player_rig::word(held)),dual?dualMap:singleMap,dual?sizeof(dualMap):sizeof(singleMap)))
                    return chosen;
            }
            return slot;
        }
        // Preserve the existing verified staff skeleton fallback.
        if(count!=4||slot!=8||!amalur::freshWeaponPose(grip,tick,now))return slot;
        constexpr uint32_t staffIds[]{11436941,6711025,12316630,8749139};
        if(memcmp(reinterpret_cast<void*>(blob+0x20+offset),staffIds,sizeof(staffIds)))return slot;
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
