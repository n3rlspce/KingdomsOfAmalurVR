#pragma once
#include "../tracking/weapon_pose.hpp"
#include "../tracking/grip_settings.hpp"
#include "../tracking/melee_swing.hpp"
#include "../tracking/grip_filter.hpp"
namespace arm_rig {extern std::atomic<bool> enabled;}
namespace weapon_control {
using Evaluate=void(__thiscall*)(void*,uintptr_t,uintptr_t);
inline Evaluate original{};
inline SRWLOCK poseLock=SRWLOCK_INIT;
inline mgs5vr::Pose desired{};
inline mgs5vr::Pose desiredLeft{};
inline uint64_t leftTick{};
inline mgs5vr::Pose bladeWorld[2]{};
inline uint64_t bladeTick{};inline uint32_t bladeOwner{};
inline unsigned swingSerial[2]{};inline float swingSpeed[2]{};
inline amalur::GripSettingsChannel positionSettings;
inline mgs5vr::Vec3 weaponCentimetres{};
inline uint64_t tick{};
inline unsigned generation{};
inline float worldScale{100.f};
inline std::atomic<bool> enabled{false};
inline std::atomic<bool> desktopPose{false};
inline amalur::PoseChannel hand{L"Local\\AmalurVRRightHandV3",L"Local\\AmalurVRRightHandMutexV3"};
inline amalur::PoseChannel leftHand{L"Local\\AmalurVRLeftHandV3",L"Local\\AmalurVRLeftHandMutexV3"};
inline amalur::MeleeSwing rightSwing,leftSwing;
inline amalur::PosePacket frameRight{},frameLeft{};
inline bool frameRightValid{},frameLeftValid{};
inline double frameSeconds{};
inline amalur::GripFilter rightFilter,leftFilter;
// Latch hands alongside the headset once per game Present. Camera callbacks
// can run repeatedly while armor and weapon remaps consume their targets.
// Re-reading XR packets in those callbacks mixes poses within one game frame.
inline void sampleHands(){
    amalur::PosePacket right{},left{};
    const bool rightValid=hand.open(false)&&hand.read(right);
    const bool leftValid=leftHand.open(false)&&leftHand.read(left);
    LARGE_INTEGER counter{},frequency{};QueryPerformanceCounter(&counter);QueryPerformanceFrequency(&frequency);
    const double seconds=double(counter.QuadPart)/double(frequency.QuadPart);
    AcquireSRWLockExclusive(&poseLock);
    frameRight=right;frameLeft=left;frameRightValid=rightValid;frameLeftValid=leftValid;
    frameSeconds=seconds;
    ReleaseSRWLockExclusive(&poseLock);
}
inline void sample(amalur::CameraPose rig,mgs5vr::Pose origin,float scale,unsigned recenter,mgs5vr::Vec3 headLocal){
    float pitch,yaw,roll;mgs5vr::Vec3 offset;
    bool offsetValid=positionSettings.open(false)&&positionSettings.read(pitch,yaw,roll,offset.x,offset.y,offset.z);
    amalur::PosePacket p,l;bool valid,leftValid;double visualTime;
    AcquireSRWLockShared(&poseLock);
    p=frameRight;l=frameLeft;valid=frameRightValid;leftValid=frameLeftValid;
    visualTime=frameSeconds;
    ReleaseSRWLockShared(&poseLock);
    const auto sampleNow=GetTickCount64();
    valid=valid&&p.tick<=sampleNow&&sampleNow-p.tick<250;
    leftValid=leftValid&&l.tick<=sampleNow&&sampleNow-l.tick<250;
    mgs5vr::Pose rightLocal{{p.orientation[0],p.orientation[1],p.orientation[2],p.orientation[3]},{p.position[0],p.position[1],p.position[2]}},
        leftLocal{{l.orientation[0],l.orientation[1],l.orientation[2],l.orientation[3]},{l.position[0],l.position[1],l.position[2]}};
    AcquireSRWLockExclusive(&poseLock);
    valid=rightFilter.sample(rightLocal,p.tick,recenter,valid,rightLocal,visualTime);
    leftValid=leftFilter.sample(leftLocal,l.tick,recenter,leftValid,leftLocal,visualTime);
    ReleaseSRWLockExclusive(&poseLock);
    mgs5vr::Pose result{};
    if(valid)valid=amalur::gripInGame(rig,mgs5vr::compose(mgs5vr::inverse(origin),rightLocal),scale,result);
    mgs5vr::Pose leftResult{};
    if(leftValid)leftValid=amalur::gripInGame(rig,mgs5vr::compose(mgs5vr::inverse(origin),leftLocal),scale,leftResult);
    auto now=GetTickCount64(),seen=motion_controls::daggerSeen.load();
    bool gestures=motion_controls::contactEnabled.load()&&firstPerson.load()&&!interfaceView.load()
        &&motion_controls::gameFocused()&&seen&&seen<=now&&now-seen<250&&motion_controls::viewControls().selectedWeapon==0;
    AcquireSRWLockExclusive(&poseLock);desired=result;tick=valid?p.tick:0;desiredLeft=leftResult;leftTick=leftValid?l.tick:0;generation=recenter;worldScale=scale;
    if(offsetValid)weaponCentimetres=offset;
    auto tip=[&](const amalur::PosePacket& v){return amalur::meleeTipRelative(mgs5vr::Pose{{v.orientation[0],v.orientation[1],v.orientation[2],v.orientation[3]},{v.position[0],v.position[1],v.position[2]}},headLocal);};
    bool r=rightSwing.sample(tip(p),p.tick,recenter,gestures&&valid);
    bool left=leftSwing.sample(tip(l),l.tick,recenter,gestures&&leftValid);
    if(r)++swingSerial[0];if(left)++swingSerial[1];
    swingSpeed[0]=rightSwing.speed();swingSpeed[1]=leftSwing.speed();
    if(r||left)motion_controls::swingUntil.store(now+90);
    ReleaseSRWLockExclusive(&poseLock);
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
inline bool isPlayerDaggers(uintptr_t self){
    auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());if(!player)return false;
    if(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94)return false;
    auto owner=player_rig::word(player+0x1ec),entity=player_rig::resolve(owner);
    auto render=player_rig::part(entity,7,owner,0x13560e4);if(!render)return false;
    auto root=fab(player_rig::word(render+0x9c));if(!root||player_rig::word(root+0xf8)!=owner)return false;
    auto n=player_rig::word(root+0x28);if(n>32)return false;bool child=false;
    for(unsigned i=0;i<n;++i)if(fab(player_rig::word(player_rig::word(root+0x24)+i*4))==self)child=true;
    if(!child||player_rig::word(self+0x38)!=7)return false;
    auto wo=player_rig::word(self+0xf8);if(!player_rig::part(player_rig::resolve(wo),11,wo,0x135745c))return false;
    auto manager=player_rig::word(gameBase+0x15fdf54),id=player_rig::word(self+0xf0);
    if(id<2||id>=100000)return false;
    auto flags=*reinterpret_cast<unsigned char*>(player_rig::word(manager+0x28)+id);
    if(!(flags&4)||(flags&16))return false;
    auto asset=player_rig::word(player_rig::word(manager+0x18)+id*4),blob=player_rig::word(asset+0x1c);
    if(player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=7)return false;
    auto off=player_rig::word(blob+0x20);if(!off||off>65536)return false;
    constexpr uint32_t ids[]{11436941,11992818,11092278,14407505,10760771,15110326,13087492};
    return !memcmp(reinterpret_cast<void*>(blob+0x20+off),ids,sizeof(ids));
}
struct Bone {mgs5vr::Vec3 position;float positionW;mgs5vr::Quat orientation;float scale[3];uint32_t flags;};
static_assert(sizeof(Bone)==48);
// Owned by the native remapper thread, never the Present/settings thread.
// Keep a single currently held dagger snapshot, not a cache of engine pointers.
struct HeldTranslation {
    amalur::HeldWeaponIdentity identity;
    amalur::RigBone before[7]{},after[7]{};
};
inline HeldTranslation heldTranslation;
inline uintptr_t currentWeaponRoot(){
    auto p=reinterpret_cast<uintptr_t>(player_rig::player.load());
    if(!p||(player_rig::word(p)!=gameBase+0x1359f14&&player_rig::word(p)!=gameBase+0x1359e94))return 0;
    auto owner=player_rig::word(p+0x1ec);
    auto render=player_rig::part(player_rig::resolve(owner),7,owner,0x13560e4);if(!render)return 0;
    auto root=fab(player_rig::word(render+0x9c));
    return root&&player_rig::word(root+0xf8)==owner?root:0;
}
using Visibility=void(__thiscall*)(void*);
inline Visibility originalHide{},nativeShow{};
inline bool keepDaggersVisible(uintptr_t object){
    __try{
        if(!firstPerson.load()||!headTracking.load()||!trackedCameraAvailable.load()
            ||!arm_rig::enabled.load()||interfaceView.load()
            ||motion_controls::viewControls().selectedWeapon!=0||!isPlayerDaggers(object))return false;
        auto root=currentWeaponRoot();
        // Native parent visibility (loading/cutscene/whole actor hiding) wins.
        if(!root||(player_rig::word(root+0x1d0)&0xf2004)
            ||(*reinterpret_cast<unsigned char*>(root+0x1d5)>0&&!*reinterpret_cast<unsigned char*>(root+0x1d6)))return false;
        mgs5vr::Pose right,left;uint64_t rightTimestamp,leftTimestamp,frame;uint32_t owner;
        AcquireSRWLockShared(&poseLock);
        right=desired;left=desiredLeft;rightTimestamp=tick;leftTimestamp=leftTick;frame=bladeTick;owner=bladeOwner;
        ReleaseSRWLockShared(&poseLock);
        return amalur::freshHeldDaggers(static_cast<uint32_t>(player_rig::word(object+0xf8)),owner,frame,GetTickCount64(),
            right,rightTimestamp,left,leftTimestamp)&&game_pause::sample(true)==0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void __fastcall hide(void* self,void*){
    if(keepDaggersVisible(reinterpret_cast<uintptr_t>(self))){
        static unsigned logged=0;if(logged++<4)log("Tracked daggers: native hide suppressed after held remap\n");
        return;
    }
    originalHide(self);
}
// Install after body_visibility has validated the original hide/show entrypoints.
inline void installHeldVisibility(){
    auto h=reinterpret_cast<unsigned char*>(gameBase+0x8ae970),s=reinterpret_cast<unsigned char*>(gameBase+0x8ae900);
    constexpr unsigned char prologue[]{0x56,0x8b,0xf1};
    constexpr unsigned char hideFlag[]{0x83,0x8e,0xd0,0x01,0,0,0x04};
    if(h[0]!=0x53||h[1]!=0x8b||h[2]!=0x1d||s[0]!=0x53||s[1]!=0x8b||s[2]!=0x1d
        ||player_rig::word(reinterpret_cast<uintptr_t>(h)+3)!=gameBase+0x15fdf54
        ||player_rig::word(reinterpret_cast<uintptr_t>(s)+3)!=gameBase+0x15fdf54
        ||memcmp(h+7,prologue,3)||memcmp(s+7,prologue,3)||memcmp(h+10,hideFlag,sizeof(hideFlag)))return;
    nativeShow=reinterpret_cast<Visibility>(s);
    hook(h,reinterpret_cast<void*>(&hide),reinterpret_cast<void**>(&originalHide),"Keep tracked daggers visible");
}
inline void restoreHeldTranslation(uintptr_t object){
    __try{
        auto& saved=heldTranslation;auto identity=saved.identity;if(!identity.object)return;
        // Resolve fresh identities before accessing any saved buffer. Removed,
        // replaced or reallocated attachments lose their snapshot without writes.
        auto root=currentWeaponRoot();auto live=fab(identity.index);
        if(!root||!live||!isPlayerDaggers(live)){saved={};return;}
        amalur::HeldWeaponIdentity current{live,root,player_rig::word(live+0x34),
            static_cast<uint32_t>(player_rig::word(live+0x194)),static_cast<uint32_t>(player_rig::word(live+0xf8)),
            static_cast<uint32_t>(player_rig::word(root+0xf8)),static_cast<uint32_t>(player_rig::word(live+0xf0)),
            static_cast<uint32_t>(player_rig::word(live+0x38))};
        if(!amalur::sameHeldWeapon(identity,current)){saved={};return;}
        if(object!=live)return;
        amalur::restoreDaggerTranslation(reinterpret_cast<amalur::RigBone*>(current.buffer),saved.before,saved.after,identity,current);
        saved={};
    }__except(EXCEPTION_EXECUTE_HANDLER){heldTranslation={};}
}
inline void translateHeld(uintptr_t object,uintptr_t slot,uintptr_t source,uintptr_t solvedSource){
    __try{
        if(heldTranslation.identity.object||slot!=7||!firstPerson.load()||!headTracking.load()||interfaceView.load()
            ||motion_controls::viewControls().selectedWeapon!=0||!isPlayerDaggers(object))return;
        auto root=currentWeaponRoot();if(!root||source!=root+0x34)return;
        mgs5vr::Vec3 offset;float scale;uint64_t rightTimestamp,leftTimestamp;mgs5vr::Pose right,left;
        AcquireSRWLockShared(&poseLock);
        offset=weaponCentimetres;scale=worldScale;rightTimestamp=tick;leftTimestamp=leftTick;right=desired;left=desiredLeft;
        ReleaseSRWLockShared(&poseLock);
        if(offset.x==0&&offset.y==0&&offset.z==0)return;
        auto now=GetTickCount64();
        bool rightTracked=amalur::freshWeaponPose(right,rightTimestamp,now),leftTracked=amalur::freshWeaponPose(left,leftTimestamp,now);
        if(!rightTracked&&!leftTracked)return;
        auto count=player_rig::word(solvedSource+4);
        if(count<3||count>64||count!=player_rig::word(source+4))return;
        auto manager=player_rig::word(gameBase+0x15fdf54),rootAsset=player_rig::word(root+0xf0);
        if(rootAsset<2||rootAsset>=100000)return;
        auto state=*reinterpret_cast<unsigned char*>(player_rig::word(manager+0x28)+rootAsset);
        if(!(state&4)||(state&16))return;
        auto blob=player_rig::word(player_rig::word(player_rig::word(manager+0x18)+rootAsset*4)+0x1c);
        if(player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=count)return;
        auto idsOffset=player_rig::word(blob+0x20),parentsOffset=player_rig::word(blob+0x1c);
        if(!idsOffset||idsOffset>65536||!parentsOffset||parentsOffset>65536)return;
        auto ids=reinterpret_cast<const uint32_t*>(blob+0x20+idsOffset);
        auto parents=reinterpret_cast<const int16_t*>(blob+0x1c+parentsOffset);
        unsigned shoulder,elbow,rightWrist,leftWrist;
        if(!amalur::rightArmIndices(count,parents,ids,shoulder,elbow,rightWrist)
            ||!amalur::leftArmIndices(count,parents,ids,shoulder,elbow,leftWrist))return;
        auto solved=reinterpret_cast<const amalur::RigBone*>(player_rig::word(solvedSource));if(!solved)return;
        auto world=[](uintptr_t p){mgs5vr::Pose pose;memcpy(&pose.position,reinterpret_cast<void*>(p+0x124),12);
            memcpy(&pose.orientation,reinterpret_cast<void*>(p+0x134),16);return amalur::nativePose(pose);};
        auto rootWorld=world(root),weaponWorld=world(object);if(!mgs5vr::valid(rootWorld))return;
        // Sliders remain in the controller grip frame, independent of the
        // authored bone-axis conversion used to orient the visible hand.
        auto rightWorld=amalur::controllerGripFromWrist(amalur::ArmSide::Right,
            mgs5vr::compose(rootWorld,amalur::bonePose(solved[rightWrist])));
        auto leftWorld=amalur::controllerGripFromWrist(amalur::ArmSide::Left,
            mgs5vr::compose(rootWorld,amalur::bonePose(solved[leftWrist])));
        auto asset=player_rig::word(object+0xf0);
        blob=player_rig::word(player_rig::word(player_rig::word(manager+0x18)+asset*4)+0x1c);
        idsOffset=player_rig::word(blob+0x20);parentsOffset=player_rig::word(blob+0x1c);
        if(!idsOffset||idsOffset>65536||!parentsOffset||parentsOffset>65536)return;
        auto buffer=player_rig::word(object+0x34);if(!buffer)return;
        HeldTranslation next{};next.identity={object,root,buffer,
            static_cast<uint32_t>(player_rig::word(object+0x194)),static_cast<uint32_t>(player_rig::word(object+0xf8)),
            static_cast<uint32_t>(player_rig::word(root+0xf8)),static_cast<uint32_t>(asset),7};
        memcpy(next.before,reinterpret_cast<void*>(buffer),sizeof(next.before));
        if(!amalur::translateDaggerGrip(next.before,next.after,7,
            reinterpret_cast<const int16_t*>(blob+0x1c+parentsOffset),reinterpret_cast<const uint32_t*>(blob+0x20+idsOffset),
            weaponWorld,rightWorld,leftWorld,offset,scale,rightTracked,leftTracked))return;
        heldTranslation=next;
        for(unsigned i=1;i<7;++i)memcpy(reinterpret_cast<void*>(buffer+i*48),&next.after[i].position,sizeof(mgs5vr::Vec3));
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline void recordBlades(uintptr_t object){
    __try{
        if(!isPlayerDaggers(object))return;
        auto native=[](mgs5vr::Pose p){p.orientation={-p.orientation.x,-p.orientation.y,-p.orientation.z,p.orientation.w};return p;};
        mgs5vr::Pose root;memcpy(&root.position,reinterpret_cast<void*>(object+0x124),12);memcpy(&root.orientation,reinterpret_cast<void*>(object+0x134),16);
        root=native(root);if(!mgs5vr::valid(root))return;
        auto bones=reinterpret_cast<const Bone*>(player_rig::word(object+0x34));mgs5vr::Pose p[2];
        p[0]=mgs5vr::compose(root,native({bones[4].orientation,bones[4].position}));
        p[1]=mgs5vr::compose(root,native({bones[1].orientation,bones[1].position}));
        if(!mgs5vr::valid(p[0])||!mgs5vr::valid(p[1]))return;
        auto owner=player_rig::word(object+0xf8);
        AcquireSRWLockExclusive(&poseLock);bladeWorld[0]=p[0];bladeWorld[1]=p[1];bladeTick=GetTickCount64();bladeOwner=owner;ReleaseSRWLockExclusive(&poseLock);
        if(nativeShow&&(player_rig::word(object+0x1d0)&4)&&keepDaggersVisible(object))
            nativeShow(reinterpret_cast<void*>(object));
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
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
    if(interfaceView.load()||!firstPerson.load()||!headTracking.load()||!enabled.load())return;
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
