#pragma once
#include "arm_pose.hpp"
namespace amalur {
struct LocomotionFrame {
    mgs5vr::Pose pose{};uint32_t owner{},center{};uint64_t tick{};bool valid{};
};
// Only the untracked locomotion basis is advanced. Physical head/controller
// motion remains in the original solved pose and is not counted a second time.
inline bool advanceLocomotion(RigBone& root,const LocomotionFrame& source,const LocomotionFrame& drawn){
    if(!source.valid||!drawn.valid||!source.owner||source.owner!=drawn.owner||source.center!=drawn.center
        ||drawn.tick<source.tick||drawn.tick-source.tick>=250
        ||!mgs5vr::valid(source.pose)||!mgs5vr::valid(drawn.pose)||!mgs5vr::valid(bonePose(root)))return false;
    const auto delta=mgs5vr::compose(drawn.pose,mgs5vr::inverse(source.pose));
    const auto moved=mgs5vr::compose(delta,bonePose(root));
    if(!mgs5vr::valid(moved))return false;
    root.position=moved.position;root.orientation=nativeQuaternion(moved.orientation);
    root.positionW=1.f;root.opaque[12]|=0x1e;
    return true;
}
}
