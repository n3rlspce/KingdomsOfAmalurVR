#pragma once
#include "mgs5vr/core.hpp"
#include <cmath>

namespace amalur {
using mgs5vr::Vec3;
using mgs5vr::Pose;
struct CameraPose { Vec3 eye,target,up; };
inline Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool normalize(Vec3& v){float n=std::sqrt(mgs5vr::dot(v,v));if(!std::isfinite(n)||n<1e-5f)return false;v=v*(1.f/n);return true;}
// The native chase camera follows body turns. Capture a world heading once,
// rather than adding physical head yaw to that moving basis every frame.
struct HeadingAnchor {
    Vec3 heading{};bool valid{};
    void reset(){valid=false;}
    bool get(Vec3 native,Vec3& result){
        if(!valid){native.z=0;if(!normalize(native))return false;heading=native;valid=true;}
        result=heading;return true;
    }
};
// Recenter position and heading only. Capturing initial pitch/roll would leave
// a tilted horizon when the headset is subsequently held upright.
inline Pose levelOrigin(Pose head) {
    Vec3 forward=mgs5vr::rotate(head.orientation,{0,0,-1});
    float yaw=std::atan2(-forward.x,-forward.z);
    return {{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)},head.position};
}
// OpenXR: X right, Y up, -Z forward. Amalur: derive basis from native eye/target/up.
// Keep orbit/locomotion in the original rig and apply recentered physical motion locally.
inline bool trackedCamera(CameraPose base,Pose relative,float unitsPerMeter,CameraPose& out) {
    if(!mgs5vr::valid(relative)||!std::isfinite(unitsPerMeter)||unitsPerMeter<=0)return false;
    Vec3 forward=base.target-base.eye;float distance=std::sqrt(mgs5vr::dot(forward,forward));
    if(!normalize(forward))return false;
    // Native basis builder RVA 0x836a00 uses up x forward for screen-right.
    // Reversing this cross product mirrors yaw, roll and lateral translation.
    Vec3 right=cross(base.up,forward);if(!normalize(right))return false;
    Vec3 up=cross(forward,right);if(!normalize(up))return false;
    auto toGame=[&](Vec3 v){return right*v.x+up*v.y-forward*v.z;};
    out.eye=base.eye+toGame(relative.position)*unitsPerMeter;
    out.target=out.eye+toGame(mgs5vr::rotate(relative.orientation,{0,0,-1}))*distance;
    out.up=toGame(mgs5vr::rotate(relative.orientation,{0,1,0}));
    return true;
}
}
