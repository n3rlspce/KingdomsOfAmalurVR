#pragma once
#include "camera_pose.hpp"
#include <cstdint>

namespace amalur {
inline Vec3 dialogueEntryFacing(Vec3 player,Vec3 npc,Vec3 fallback){
    auto direction=npc-player;direction.z=0;
    return normalize(direction)?direction:fallback;
}
// Gameplay's visual body reference is 195 units high and 7 units ahead of
// the actor. Dialogue's eye is 185 high and 15 ahead. Transfer only the
// headset's movement between those references; do not move the actor or floor.
inline Vec3 dialogueBodyAnchor(Vec3 eye,Vec3 entryHeading){
    return eye+Vec3{0,0,10}-entryHeading*8.f;
}
// A dialogue camera cut must not move the VR viewer. Only a new conversation,
// player, or reference-space generation rebases the local headset origin.
struct DialogueView {
    uintptr_t conversation{};uint32_t owner{},participant{},localCenter{},bridgeCenter{};
    Pose origin{},lastHead{};Vec3 heading{};bool active{},continuityAdjusted{};
    uint64_t lastHeadTick{};
    void reset(){active=false;lastHeadTick=0;continuityAdjusted=false;}
    bool apply(uintptr_t dialog,uint32_t player,uint32_t npc,uint32_t local,uint32_t bridge,
               Vec3 position,Vec3 facing,Pose head,float scale,CameraPose& out,uint64_t sampleTick=0){
        if(!dialog||!player||!npc||!mgs5vr::valid(head)||!std::isfinite(scale)||scale<=0||
           !std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))return false;
        continuityAdjusted=false;
        // PlayerDialog records can change at a choice without ending the
        // conversation. Keep the headset origin while the actors stay the same.
        if(!active||owner!=player||participant!=npc){
            facing.z=0;if(!normalize(facing))return false;
            heading=facing;origin=levelOrigin(head);
            owner=player;participant=npc;localCenter=local;bridgeCenter=bridge;active=true;lastHeadTick=0;
        }else if(localCenter!=local||bridgeCenter!=bridge){
            origin=levelOrigin(head);localCenter=local;bridgeCenter=bridge;lastHeadTick=0;
        }else if(sampleTick&&lastHeadTick&&sampleTick>lastHeadTick){
            // OpenXR LOCAL may change origin without a bridge recenter event.
            // The observed dialogue trace jumped 16.5 cm in 15 ms and reset
            // yaw, while both actors stood still. Preserve the last relative
            // view when a physically implausible tracking-space jump occurs.
            const auto dt=sampleTick-lastHeadTick;
            const auto delta=head.position-lastHead.position;
            const float distance2=mgs5vr::dot(delta,delta);
            if(dt<=100&&distance2>.08f*.08f&&distance2*1000000.f>9.f*float(dt)*float(dt)){
                const auto relative=mgs5vr::compose(mgs5vr::inverse(origin),lastHead);
                const auto candidate=mgs5vr::compose(head,mgs5vr::inverse(relative));
                if(mgs5vr::valid(candidate)){origin=candidate;continuityAdjusted=true;}
            }
        }
        conversation=dialog;
        if(!lastHeadTick||sampleTick>=lastHeadTick){lastHead=head;lastHeadTick=sampleTick;}
        const auto eye=position+Vec3{0,0,185}+heading*15.f;
        return trackedCamera({eye,eye+heading*200.f,{0,0,1}},
            mgs5vr::compose(mgs5vr::inverse(origin),head),scale,out);
    }
};
}
