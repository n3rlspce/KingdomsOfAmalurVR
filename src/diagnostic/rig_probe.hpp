#pragma once
#include "arm_rig.hpp"
// Reversible discovery probe, not controller IK. Only the verified local-player
// armor child is eligible; NPCs and opaque bone bytes are untouched.
namespace rig_probe {
inline std::atomic<bool> enabled{false};
inline std::atomic<unsigned> samples{0};
using BoneEvaluate=void(__thiscall*)(void*,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
inline BoneEvaluate originalBones{};
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
inline void __fastcall evaluateBones(void* mapper,void*,uintptr_t slot,uintptr_t source,
    uintptr_t output,uintptr_t skeleton,uintptr_t extra,uintptr_t flags){
    auto object=output>=0x34?output-0x34:0;
    beforeEvaluation(object);
    arm_rig::Scratch scratch;
    const auto input=arm_rig::prepare(source,output,scratch)?reinterpret_cast<uintptr_t>(scratch.descriptor):source;
    originalBones(mapper,slot,input,output,skeleton,extra,flags);
    afterEvaluation(object);
}
inline void disable(){
    enabled.store(false);AcquireSRWLockExclusive(&lock);restore();ReleaseSRWLockExclusive(&lock);
}
inline void install(){
    auto target=reinterpret_cast<unsigned char*>(gameBase+0x8cd2c0);
    const unsigned char expected[]={0x81,0xec,0xf4,0,0,0,0xa1};
    if(memcmp(target,expected,sizeof(expected))||player_rig::word(reinterpret_cast<uintptr_t>(target)+7)!=gameBase+0x157713c)return;
    hook(target,reinterpret_cast<void*>(&evaluateBones),reinterpret_cast<void**>(&originalBones),"Player attachment pose remap probe");
}
}
