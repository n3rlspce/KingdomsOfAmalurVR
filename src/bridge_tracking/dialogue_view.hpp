#pragma once
#include "camera_pose.hpp"
#include <cstdint>

namespace amalur {
// A dialogue camera cut must not move the VR viewer. Only a new conversation,
// player, or reference-space generation rebases the local headset origin.
struct DialogueView {
    uintptr_t conversation{};uint32_t owner{},localCenter{},bridgeCenter{};
    Pose origin{};Vec3 heading{};bool active{};
    void reset(){active=false;}
    bool apply(uintptr_t dialog,uint32_t player,uint32_t local,uint32_t bridge,
               Vec3 position,Vec3 facing,Pose head,float scale,CameraPose& out){
        if(!dialog||!player||!mgs5vr::valid(head)||!std::isfinite(scale)||scale<=0||
           !std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z))return false;
        if(!active||conversation!=dialog||owner!=player){
            facing.z=0;if(!normalize(facing))return false;
            heading=facing;origin=levelOrigin(head);
            conversation=dialog;owner=player;localCenter=local;bridgeCenter=bridge;active=true;
        }else if(localCenter!=local||bridgeCenter!=bridge){
            origin=levelOrigin(head);localCenter=local;bridgeCenter=bridge;
        }
        const auto eye=position+Vec3{0,0,185}+heading*15.f;
        return trackedCamera({eye,eye+heading*200.f,{0,0,1}},
            mgs5vr::compose(mgs5vr::inverse(origin),head),scale,out);
    }
};
}
