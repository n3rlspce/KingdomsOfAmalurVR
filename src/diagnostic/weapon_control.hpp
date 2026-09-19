#pragma once
#include "../tracking/weapon_pose.hpp"
namespace weapon_control {
using Evaluate=void(__thiscall*)(void*,uintptr_t,uintptr_t);
inline Evaluate original{};
inline SRWLOCK poseLock=SRWLOCK_INIT;
inline mgs5vr::Pose desired{};
inline uint64_t tick{};
inline unsigned generation{};
inline float worldScale{100.f};
inline std::atomic<bool> enabled{false};
inline amalur::PoseChannel hand{L"Local\\AmalurVRRightHandV3",L"Local\\AmalurVRRightHandMutexV3"};
inline void sample(amalur::CameraPose rig,mgs5vr::Pose origin,float scale,unsigned recenter){
    amalur::PosePacket p;mgs5vr::Pose result{};
    bool valid=hand.open(false)&&hand.read(p);
    if(valid){mgs5vr::Pose local{{p.orientation[0],p.orientation[1],p.orientation[2],p.orientation[3]},{p.position[0],p.position[1],p.position[2]}};
        valid=amalur::gripInGame(rig,mgs5vr::compose(mgs5vr::inverse(origin),local),scale,result);}
    AcquireSRWLockExclusive(&poseLock);desired=result;tick=valid?p.tick:0;generation=recenter;worldScale=scale;ReleaseSRWLockExclusive(&poseLock);
}
inline uintptr_t fab(uint32_t index){
    auto mgr=player_rig::word(gameBase+0x15fdf54);if(!mgr||index<2||index>=player_rig::word(mgr+0xc8))return 0;
    auto table=player_rig::word(mgr+0xc4);auto p=player_rig::word(table+index*4);
    return p&&player_rig::word(p)==gameBase+0x1340f3c&&player_rig::word(p+0x194)==index?p:0;
}
inline bool isSinglePlayerWeapon(uintptr_t self){
    auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());if(!player)return false;
    if(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94)return false;
    auto owner=player_rig::word(player+0x1ec);auto entity=player_rig::resolve(owner);
    auto render=player_rig::part(entity,7,owner,0x13560e4);if(!render)return false;
    auto parent=fab(player_rig::word(render+0x9c));if(!parent||player_rig::word(parent+0xf8)!=owner)return false;
    auto count=player_rig::word(parent+0x28);if(count>32)return false;
    auto children=player_rig::word(parent+0x24);uintptr_t selected=0;
    for(unsigned i=0;i<count;++i){auto child=fab(player_rig::word(children+i*4));if(!child)continue;
        auto childOwner=player_rig::word(child+0xf8);auto childEntity=player_rig::resolve(childOwner);
        if(player_rig::part(childEntity,11,childOwner,0x135745c)){
            // Native draw/sheath transitions can reference one Fab in two slots.
            if(selected&&selected!=child)return false;selected=child;
        }}
    // Dual weapons and ambiguous equipment deliberately await explicit slot mapping.
    return selected==self;
}
struct Bone {mgs5vr::Vec3 position;float positionW;mgs5vr::Quat orientation;float scale[3];uint32_t flags;};
static_assert(sizeof(Bone)==48);
inline SRWLOCK editLock=SRWLOCK_INIT;
inline uintptr_t editedSelf{},editedOwner{};inline Bone* editedBones{};inline unsigned editedCount{};
inline Bone before[128],after[128];
inline void restore(uintptr_t self){
    if(editedSelf!=self)return;
    __try {
        if(player_rig::word(self+0xf8)==editedOwner&&player_rig::word(self+0x34)==reinterpret_cast<uintptr_t>(editedBones)
            &&player_rig::word(self+0x38)==editedCount&&memcmp(editedBones,after,editedCount*sizeof(Bone))==0)
            memcpy(editedBones,before,editedCount*sizeof(Bone));
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    editedSelf=0;
}
inline bool apply(uintptr_t self,mgs5vr::Pose grip,unsigned center){
    __try {
        if(!isSinglePlayerWeapon(self))return false;
        auto count=player_rig::word(self+0x38);auto bones=reinterpret_cast<Bone*>(player_rig::word(self+0x34));
        if(!bones||count<1||count>128)return false;
        mgs5vr::Pose root;memcpy(&root.position,reinterpret_cast<void*>(self+0x124),12);memcpy(&root.orientation,reinterpret_cast<void*>(self+0x134),16);
        mgs5vr::Pose anchor{bones[0].orientation,bones[0].position};if(!mgs5vr::valid(root)||!mgs5vr::valid(anchor))return false;
        static uintptr_t calibratedSelf{};static uint32_t calibratedOwner{};static unsigned calibratedCenter{};static mgs5vr::Pose trim{};
        auto owner=player_rig::word(self+0xf8);
        if(calibratedSelf!=self||calibratedOwner!=owner||calibratedCenter!=center){
            auto world=mgs5vr::compose(root,anchor);
            trim=mgs5vr::compose(mgs5vr::inverse(grip),world);trim.position={};
            calibratedSelf=self;calibratedOwner=owner;calibratedCenter=center;
            log("Right weapon candidate calibrated: bones=%u owner=%08x (visual prototype)\n",count,owner);
        }
        auto target=mgs5vr::compose(mgs5vr::inverse(root),mgs5vr::compose(grip,trim));
        auto delta=mgs5vr::compose(target,mgs5vr::inverse(anchor));
        Bone adjusted[128];
        for(unsigned i=0;i<count;++i){adjusted[i]=bones[i];auto pose=mgs5vr::compose(delta,{bones[i].orientation,bones[i].position});
            if(!mgs5vr::valid(pose))return false;adjusted[i].position=pose.position;adjusted[i].orientation=pose.orientation;}
        // Restore our exact output before the next native evaluation, including
        // disable/tracking loss, so an idle animator cannot accumulate overrides.
        memcpy(before,bones,count*sizeof(Bone));memcpy(after,adjusted,count*sizeof(Bone));
        editedSelf=self;editedOwner=owner;editedBones=bones;editedCount=count;
        memcpy(bones,adjusted,count*sizeof(Bone));return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void __fastcall evaluate(void* self,void*,uintptr_t first,uintptr_t second){
    AcquireSRWLockExclusive(&editLock);restore(reinterpret_cast<uintptr_t>(self));ReleaseSRWLockExclusive(&editLock);
    original(self,first,second);
    if(!firstPerson.load()||!headTracking.load()||!enabled.load())return;
    mgs5vr::Pose grip;uint64_t timestamp;unsigned center;
    AcquireSRWLockShared(&poseLock);grip=desired;timestamp=tick;center=generation;ReleaseSRWLockShared(&poseLock);
    auto now=GetTickCount64();if(!timestamp||timestamp>now||now-timestamp>150)return;
    AcquireSRWLockExclusive(&editLock);apply(reinterpret_cast<uintptr_t>(self),grip,center);ReleaseSRWLockExclusive(&editLock);
}
inline void install(){
    // Full FabInstancePhysics evaluation includes the alternate animation path
    // that bypasses 0x91b4f0. Apply only after both native paths have completed.
    auto target=reinterpret_cast<unsigned char*>(gameBase+0x96f600);
    const unsigned char expected[]={0x83,0xec,0x34,0xa1};
    if(memcmp(target,expected,sizeof(expected))){log("Weapon evaluation signature mismatch; skipped\n");return;}
    if(*reinterpret_cast<uintptr_t*>(target+4)!=gameBase+0x157713c||target[8]!=0x33||target[9]!=0xc4){log("Weapon evaluation guard mismatch; skipped\n");return;}
    hook(target,reinterpret_cast<void*>(&evaluate),reinterpret_cast<void**>(&original),"Weapon pose after native evaluation");
}
}
