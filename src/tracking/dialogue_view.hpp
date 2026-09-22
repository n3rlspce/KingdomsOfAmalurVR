#pragma once
#include "camera_pose.hpp"
#include <cstdint>
#include <algorithm>

namespace amalur {
// Rate-limit native body yaw toward the viewer's position. Head orientation is
// deliberately not an input: looking away must not make the NPC swivel away.
struct DialogueNpcFollow {
    uintptr_t dialog{};uint32_t npc{};uint64_t tick{};bool active{};
    void reset(){active=false;}
    bool step(uintptr_t conversation,uint32_t actor,uint64_t now,double current,
              Vec3 position,Vec3 eye,uint32_t& delta){
        delta=0;
        const auto d=eye-position;
        if(!conversation||!actor||!std::isfinite(current)||!std::isfinite(d.x)||
           !std::isfinite(d.y)||d.x*d.x+d.y*d.y<1.f){reset();return false;}
        if(!active||dialog!=conversation||npc!=actor||now<tick){
            dialog=conversation;npc=actor;tick=now;active=true;return false;
        }
        if(now==tick)return false;
        const double dt=std::min(double(now-tick)/1000.0,.05);tick=now;
        const double desired=std::atan2(double(d.y),double(d.x))*180.0/3.141592653589793;
        const double error=std::remainder(desired-current,360.0);
        if(std::abs(error)<.3)return false;
        const double step=std::clamp(error,-60.0*dt,60.0*dt);
        delta=static_cast<uint32_t>(static_cast<int64_t>(std::llround(step*(4294967296.0/360.0))));
        return delta!=0;
    }
};
inline bool dialogueNpcYaw(Vec3 npc,Vec3 eye,int& degrees){
    const auto d=eye-npc;
    if(!std::isfinite(d.x)||!std::isfinite(d.y)||d.x*d.x+d.y*d.y<1.f)return false;
    const double yaw=std::atan2(double(d.y),double(d.x))*180.0/3.141592653589793;
    degrees=(int(std::lround(yaw))+360)%360;
    return true;
}
inline Vec3 dialogueEntryFacing(Vec3 player,Vec3 npc,Vec3 fallback){
    auto direction=npc-player;direction.z=0;
    return normalize(direction)?direction:fallback;
}
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
