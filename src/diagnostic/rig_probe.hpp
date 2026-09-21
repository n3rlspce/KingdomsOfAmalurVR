#pragma once
#include "arm_rig.hpp"
#include "rig_status.hpp"
#include "shield_control.hpp"
#include "skin_root_pair.hpp"
// Reversible discovery probe, not controller IK. Only the verified local-player
// armor child is eligible; NPCs and opaque bone bytes are untouched.
namespace rig_probe {
inline std::atomic<bool> enabled{false};
inline std::atomic<unsigned> samples{0};
using BoneEvaluate=void(__thiscall*)(void*,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
inline BoneEvaluate originalBones{};
using BoneWorld=uintptr_t(__thiscall*)(void*,unsigned,amalur::RigBone*,uintptr_t);
inline BoneWorld originalWorld{};
inline void adjustSocket(uintptr_t root,unsigned index,amalur::RigBone* output){
    __try {
        if(!output||root!=playerRoot()||index>=player_rig::word(root+0x38))return;
        arm_rig::Scratch scratch;const bool solved=arm_rig::solveUnsafe(root,scratch,true,amalur::skin_audit::Getter);
        scratch.trace.slot=index;arm_rig::finishAudit(scratch,solved);if(!solved)return;
        auto native=reinterpret_cast<const amalur::RigBone*>(player_rig::word(root+0x34));
        if(index>=64||!mgs5vr::valid(amalur::bonePose(native[index]))||!mgs5vr::valid(amalur::bonePose(*output)))return;
        if(!memcmp(&native[index],&scratch.bones[index],sizeof(amalur::RigBone)))return;
        mgs5vr::Pose world;memcpy(&world.position,reinterpret_cast<void*>(root+0x124),12);
        memcpy(&world.orientation,reinterpret_cast<void*>(root+0x134),16);world=amalur::nativePose(world);if(!mgs5vr::valid(world))return;
        auto before=mgs5vr::compose(world,amalur::bonePose(native[index]));
        auto after=mgs5vr::compose(world,amalur::bonePose(scratch.bones[index]));
        auto delta=mgs5vr::compose(after,mgs5vr::inverse(before));
        auto result=mgs5vr::compose(delta,amalur::bonePose(*output));if(!mgs5vr::valid(result))return;
        const auto originalPose=*output;
        output->position=result.position;output->orientation=amalur::nativeQuaternion(result.orientation);
        amalur::publishRigOverrides(&originalPose,output,1);
        static unsigned logs=0;if(logs++<12)log("Tracked native socket bone=%u\n",index);
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
inline uintptr_t __fastcall boneWorld(void* self,void*,unsigned index,amalur::RigBone* output,uintptr_t offset){
    auto result=originalWorld(self,index,output,offset);
    if(arm_rig::enabled.load()&&headTracking.load()&&!amalur::bodyDebug.enabled(amalur::skipRigSockets))adjustSocket(reinterpret_cast<uintptr_t>(self),index,output);
    return result;
}
inline SRWLOCK lock=SRWLOCK_INIT;
struct Position {float x,y,z;};
inline uintptr_t savedObject{},savedBuffer{};
inline uint32_t savedOwner{};
inline unsigned savedCount{};
inline Position before[64]{},after[64]{};
inline bool changed[64]{};
inline uintptr_t playerRoot(){
    auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
    if(!player||(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94))return 0;
    auto owner=player_rig::word(player+0x1ec),entity=player_rig::resolve(owner);
    auto part=player_rig::part(entity,7,owner,0x13560e4);if(!part)return 0;
    auto root=weapon_control::fab(player_rig::word(part+0x9c));
    return root&&player_rig::word(root+0xf8)==owner?root:0;
}
inline void restore(){
    __try {
        if(savedObject&&weapon_control::fab(player_rig::word(savedObject+0x194))==savedObject
            &&player_rig::word(savedObject+0xf8)==savedOwner
            &&player_rig::word(savedObject+0x34)==savedBuffer&&player_rig::word(savedObject+0x38)==savedCount){
            for(unsigned i=0;i<savedCount;++i)if(changed[i]){
                auto p=reinterpret_cast<void*>(savedBuffer+i*48);
                if(memcmp(p,&after[i],sizeof(Position))==0)memcpy(p,&before[i],sizeof(Position));
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    savedObject=0;
}
inline void apply(uintptr_t object){
    if(amalur::playMode.normal())return;
    __try {
        if(savedObject&&savedObject!=object)return; // one mesh lease for this probe
        auto root=playerRoot();if(!root||root==object)return;
        auto childCount=player_rig::word(root+0x28);if(childCount>32)return;
        bool owned=false;
        for(unsigned i=0;i<childCount;++i)if(weapon_control::fab(player_rig::word(player_rig::word(root+0x24)+i*4))==object){owned=true;break;}
        if(!owned)return;
        auto owner=player_rig::word(object+0xf8);
        if(!player_rig::part(player_rig::resolve(owner),12,owner,0x13563e4))return;
        unsigned count=player_rig::word(object+0x38);if(count<2||count>64)return;
        auto manager=player_rig::word(gameBase+0x15fdf54),assetId=player_rig::word(object+0xf0);
        if(assetId<2||assetId>=100000)return;
        auto flags=*reinterpret_cast<unsigned char*>(player_rig::word(manager+0x28)+assetId);
        if(!(flags&4)||(flags&0x10))return;
        auto asset=player_rig::word(player_rig::word(manager+0x18)+assetId*4);
        auto blob=player_rig::word(asset+0x1c);
        if(player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=count)return;
        auto parentOffset=*reinterpret_cast<int32_t*>(blob+0x1c);
        if(parentOffset<0||parentOffset>65536)return;
        auto parents=reinterpret_cast<const int16_t*>(blob+0x1c+parentOffset);
        for(unsigned i=0;i<count;++i)if(parents[i]<-1||parents[i]>=static_cast<int>(i))return;
        auto idOffset=*reinterpret_cast<int32_t*>(blob+0x20);if(idOffset<=0||idOffset>65536)return;
        auto ids=reinterpret_cast<const uint32_t*>(blob+0x20+idOffset);
        // Stable native bone ID found at root index 25 and glove index 8.
        // Resolve by ID, not by equipment slot or assumed mesh bone index.
        unsigned wrist=count;
        for(unsigned i=0;i<count;++i)if(ids[i]==0x0087c3ed){if(wrist!=count)return;wrist=i;}
        if(wrist==count)return;
        auto buffer=player_rig::word(object+0x34);if(!buffer)return;
        bool selected[64]{};
        for(unsigned i=0;i<count;++i){
            selected[i]=i==wrist||(parents[i]>=0&&selected[parents[i]]);
            if(!selected[i])continue;
            memcpy(&before[i],reinterpret_cast<void*>(buffer+i*48),sizeof(Position));
            auto p=before[i];
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)
                ||std::abs(p.x)>1000||std::abs(p.y)>1000||std::abs(p.z)>1000)return;
            after[i]={p.x,p.y,p.z+30.f};
        }
        savedObject=object;savedOwner=owner;savedBuffer=buffer;savedCount=count;
        memcpy(changed,selected,sizeof(changed));
        for(unsigned i=0;i<count;++i)if(changed[i])memcpy(reinterpret_cast<void*>(buffer+i*48),&after[i],sizeof(Position));
        ++samples;
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
inline void beforeEvaluation(uintptr_t object){
    AcquireSRWLockExclusive(&lock);
    if(savedObject==object)restore();
    ReleaseSRWLockExclusive(&lock);
}
inline void afterEvaluation(uintptr_t object){
    if(!enabled.load())return;
    AcquireSRWLockExclusive(&lock);
    if(savedObject==object)restore();
    if(enabled.load())apply(object);
    ReleaseSRWLockExclusive(&lock);
}
// Native attachment-pose remapper. Output is argument 3, a Fab + 0x34
// array descriptor. Confirmed with a hardware write breakpoint on glove wrist Z.
inline void traceRemap(uintptr_t object,uintptr_t output,uintptr_t slot,bool solved,arm_rig::Scratch& scratch){
    if(!scratch.trace.root)return;
    auto& trace=scratch.trace;trace.object=static_cast<uint32_t>(object);trace.slot=static_cast<uint32_t>(slot);
    if(solved)trace.flags|=8u;
    __try {
        const auto count=player_rig::word(output+4),buffer=player_rig::word(output);
        const auto manager=player_rig::word(gameBase+0x15fdf54),assetId=player_rig::word(object+0xf0);
        if(count&&count<=64&&buffer&&assetId>=2&&assetId<100000){
            const auto state=*reinterpret_cast<unsigned char*>(player_rig::word(manager+0x28)+assetId);
            if((state&4)&&!(state&16)){
                const auto asset=player_rig::word(player_rig::word(manager+0x18)+assetId*4),blob=player_rig::word(asset+0x1c);
                const auto offset=player_rig::word(blob+0x20);
                if(player_rig::word(blob)==0x45533033&&player_rig::word(blob+0x10)==count&&offset&&offset<=65536){
                    memcpy(&trace.objectWorld.position,reinterpret_cast<void*>(object+0x124),12);
                    memcpy(&trace.objectWorld.orientation,reinterpret_cast<void*>(object+0x134),16);
                    trace.objectWorld=amalur::nativePose(trace.objectWorld);
                    if(scratch.audit){
                        auto& a=*scratch.audit;a.childAsset=assetId;a.childBuffer=buffer;a.childCount=count;
                        memcpy(&a.childWorldBefore,reinterpret_cast<void*>(object+0x124),sizeof(a.childWorldBefore));
                        memcpy(a.child,reinterpret_cast<void*>(buffer),count*sizeof(amalur::RigBone));
                        memcpy(a.childIds,reinterpret_cast<void*>(blob+0x20+offset),count*sizeof(uint32_t));
                        const auto po=player_rig::word(blob+0x1c);
                        if(po&&po<=65536)memcpy(a.childParents,reinterpret_cast<void*>(blob+0x1c+po),count*sizeof(int16_t));
                        a.childHash=amalur::skin_audit::hash(a.child,count*sizeof(amalur::RigBone));
                    }
                    for(unsigned i=0;i<count;++i){
                        const auto id=player_rig::word(blob+0x20+offset+i*4);
                        if(id!=0x88d0eb&&id!=0x87c3ed)continue;
                        const unsigned side=id==0x88d0eb?0:1;
                        const auto pose=amalur::bonePose(reinterpret_cast<const amalur::RigBone*>(buffer)[i]);
                        if(!mgs5vr::valid(pose))continue;
                        // The caller copies the parent root transform to this
                        // child AFTER this hook returns. Use that final basis;
                        // retain the old child basis separately for diagnosis.
                        trace.remapped[side]=mgs5vr::compose(trace.rootWorld,pose);trace.flags|=16u<<side;
                    }
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){trace.flags|=64u;}
    arm_trace::publish(trace);
    arm_rig::finishAudit(scratch,solved);
}
inline void __fastcall evaluateBones(void* mapper,void*,uintptr_t slot,uintptr_t source,
    uintptr_t output,uintptr_t skeleton,uintptr_t extra,uintptr_t flags){
    auto object=output>=0x34?output-0x34:0;
    beforeEvaluation(object);
    weapon_control::observeNativeWeaponSlot(mapper,slot,source,output);
    arm_rig::Scratch scratch;
    const bool solved=arm_rig::prepare(source,output,scratch);
    // Ablation: retain the solve and its state changes, but publish native mesh
    // input. Socket behavior is controlled independently by skipRigSockets.
    const bool nativeMesh=amalur::bodyDebug.enabled(amalur::nativeMeshInput);
    if(nativeMesh)scratch.trace.flags|=256u;
    const auto input=solved&&!nativeMesh?reinterpret_cast<uintptr_t>(scratch.descriptor):source;
    const auto renderSlot=arm_rig::trackedWeaponSlot(mapper,slot,output,solved);
    weapon_control::restoreHeldTranslation(object);
    shield_control::restore(object);
    originalBones(mapper,renderSlot,input,output,skeleton,extra,flags);
    if(!nativeMesh&&!amalur::bodyDebug.enabled(amalur::nativeMeshRotations)&&arm_rig::enabled.load()&&!amalur::bodyDebug.enabled(amalur::nativeArms))shield_control::apply(object,source,solved);
    if(solved&&!nativeMesh&&!amalur::bodyDebug.enabled(amalur::nativeMeshPositions)&&arm_rig::enabled.load()&&!amalur::bodyDebug.enabled(amalur::nativeArms))weapon_control::translateHeld(object,renderSlot,source,input);
    if(solved&&arm_rig::enabled.load()&&!amalur::bodyDebug.enabled(amalur::nativeArms))support_grip::record(object,renderSlot,scratch.trace.rootWorld);
    traceRemap(object,output,renderSlot,solved,scratch);
    if(solved&&renderSlot==7&&!amalur::bodyDebug.enabled(amalur::nativeArms))weapon_control::recordBlades(object);
    if(solved&&arm_rig::enabled.load()&&!amalur::bodyDebug.enabled(amalur::nativeArms))weapon_control::recordHeld(object,renderSlot,scratch.trace.rootWorld);
    rig_status::attachment(mapper,renderSlot,source,output,slot);
    afterEvaluation(object);
    skin_root_pair::remember(object,scratch.trace.root,solved&&!nativeMesh&&!amalur::bodyDebug.enabled(amalur::nativeMeshPositions),scratch.locomotion);
}
inline void disable(){
    enabled.store(false);AcquireSRWLockExclusive(&lock);restore();ReleaseSRWLockExclusive(&lock);
}
inline void install(){
    auto target=reinterpret_cast<unsigned char*>(gameBase+0x8cd2c0);
    const unsigned char expected[]={0x81,0xec,0xf4,0,0,0,0xa1};
    if(memcmp(target,expected,sizeof(expected))||player_rig::word(reinterpret_cast<uintptr_t>(target)+7)!=gameBase+0x157713c)return;
    hook(target,reinterpret_cast<void*>(&evaluateBones),reinterpret_cast<void**>(&originalBones),"Player attachment pose remap probe");
    auto world=reinterpret_cast<unsigned char*>(gameBase+0x8aefe0);
    const unsigned char worldPrefix[]={0x83,0xec,0x64,0xa1};
    const unsigned char worldTail[]={0x83,0xc4,0x64,0xc2,0x0c,0x00};
    if(!memcmp(world,worldPrefix,sizeof(worldPrefix))&&player_rig::word(reinterpret_cast<uintptr_t>(world)+4)==gameBase+0x157713c
        &&!memcmp(world+0x85,worldTail,sizeof(worldTail)))
        hook(world,reinterpret_cast<void*>(&boneWorld),reinterpret_cast<void**>(&originalWorld),"Tracked player bone world sockets");
    weapon_control::installHeldVisibility();
    skin_root_pair::install();
}
}
