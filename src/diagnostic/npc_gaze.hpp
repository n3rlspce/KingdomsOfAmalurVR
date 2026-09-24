#pragma once
#include "../tracking/npc_head_gaze.hpp"

namespace npc_gaze {
struct Target {uint32_t actor{};mgs5vr::Vec3 eye{};uint64_t tick{};};
static SRWLOCK lock=SRWLOCK_INIT;
static Target target{};
static std::atomic<uint64_t> activeCalls{},rootMatches{},childMatches{},skeletonMatches{},solutions{};
static void publish(uint32_t actor,mgs5vr::Vec3 eye,uint64_t tick){
    AcquireSRWLockExclusive(&lock);target={actor,eye,tick};ReleaseSRWLockExclusive(&lock);
}
static void clear(){publish(0,{},0);}
static void apply(uintptr_t object,uintptr_t descriptor){
    Target t;AcquireSRWLockShared(&lock);t=target;ReleaseSRWLockShared(&lock);
    const auto now=GetTickCount64();
    if(!t.actor||now<t.tick||now-t.tick>250)return;
    ++activeCalls;
    __try {
        const auto word=player_rig::word;
        if(descriptor!=object+0x34)return;
        const auto entity=player_rig::resolve(t.actor);
        const auto rendering=player_rig::part(entity,7,t.actor,0x13560e4);
        const auto root=rendering?weapon_control::fab(word(rendering+0x9c)):0;
        if(!root||word(root+0xf8)!=t.actor)return;
        if(root==object)++rootMatches;
        else {
            const auto childCount=word(root+0x28),children=word(root+0x24);
            if(!children||childCount>32)return;
            bool directChild=false;
            for(unsigned i=0;i<childCount;++i)if(weapon_control::fab(word(children+i*4))==object){directChild=true;break;}
            if(!directChild)return;
            ++childMatches;
        }
        const uint32_t* ids{};const int16_t* parents{};unsigned count{};
        if(!body_visibility::skeleton(object,ids,parents,count)||word(descriptor+4)!=count)return;
        ++skeletonMatches;
        const auto buffer=word(descriptor);if(!buffer)return;
        amalur::RigBone native[128],world{};
        memcpy(native,reinterpret_cast<void*>(buffer),count*sizeof(amalur::RigBone));
        memcpy(&world,reinterpret_cast<void*>(object+0x124),sizeof(world));
        const auto rootPose=amalur::bonePose(world);
        if(!mgs5vr::valid(rootPose))return;
        if(amalur::npcHeadGaze(native,reinterpret_cast<amalur::RigBone*>(buffer),count,
            parents,ids,rootPose,t.eye)){
            ++solutions;
            static unsigned reports{};if(reports++<6)log("Dialogue NPC head gaze applied owner=%08x root=%08x\n",t.actor,unsigned(object));
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static void report(uint64_t now){
    static std::atomic<uint64_t> next{};
    auto due=next.load();
    if(now<due||!next.compare_exchange_strong(due,now+2000))return;
    Target t;AcquireSRWLockShared(&lock);t=target;ReleaseSRWLockShared(&lock);
    if(t.actor)log("Dialogue gaze audit npc=%08x activeCalls=%llu root=%llu child=%llu skeleton=%llu solved=%llu\n",
        t.actor,activeCalls.load(),rootMatches.load(),childMatches.load(),skeletonMatches.load(),solutions.load());
}
}
