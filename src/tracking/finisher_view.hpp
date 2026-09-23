#pragma once
#include "camera_pose.hpp"
#include "arm_pose.hpp"
#include <cstdint>
namespace amalur {
// RigBone's tail is opaque (including inactive/uninitialized channels), not
// three always-valid scale floats. Match the established rig pose conversion.
inline bool finisherHeadPosition(const RigBone& world,const RigBone& head,Vec3 body,Vec3& eye){
    const auto wp=bonePose(world),hp=bonePose(head);
    if(!mgs5vr::valid(wp)||!mgs5vr::valid(hp))return false;
    const auto result=mgs5vr::compose(wp,hp).position;
    const auto delta=result-body;
    const float distance2=mgs5vr::dot(delta,delta);
    // Reject stale/mismatched transforms. Ten native metres accommodates
    // authored finisher root motion; this is a safety bound, not a camera offset.
    if(!std::isfinite(distance2)||distance2>1000000.f)return false;
    eye=result;return true;
}
// A finisher follows the player's animated eye, never authored shot positions
// or rotations. The heading is latched once; only HMD motion changes view yaw.
struct FinisherView {
    uint32_t owner{},localCenter{},bridgeCenter{};bool active{};
    Pose origin{};Vec3 heading{};
    void reset(){active=false;owner=0;}
    bool apply(uint32_t actor,uint32_t local,uint32_t bridge,Vec3 eye,Vec3 entryHeading,
               Pose head,float scale,CameraPose& out){
        if(!actor||!mgs5vr::valid(head)||!std::isfinite(scale)||scale<=0||
           !std::isfinite(eye.x)||!std::isfinite(eye.y)||!std::isfinite(eye.z))return false;
        if(!active||actor!=owner){
            entryHeading.z=0;if(!normalize(entryHeading))return false;
            heading=entryHeading;origin=levelOrigin(head);owner=actor;
            localCenter=local;bridgeCenter=bridge;active=true;
        }else if(local!=localCenter||bridge!=bridgeCenter){
            // Recenter keeps virtual facing; it does not adopt a cinematic cut.
            origin=levelOrigin(head);localCenter=local;bridgeCenter=bridge;
        }
        return trackedCamera({eye,eye+heading*200.f,{0,0,1}},
            mgs5vr::compose(mgs5vr::inverse(origin),head),scale,out);
    }
};
}
