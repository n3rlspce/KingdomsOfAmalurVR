#pragma once
#include "mgs5vr/core.hpp"
#include <cmath>

namespace amalur {
using mgs5vr::Vec3;
using mgs5vr::Pose;
struct CameraPose { Vec3 eye,target,up; };
inline bool cameraChanged(const CameraPose& a,const CameraPose& b){
    return a.eye.x!=b.eye.x||a.eye.y!=b.eye.y||a.eye.z!=b.eye.z
        ||a.target.x!=b.target.x||a.target.y!=b.target.y||a.target.z!=b.target.z
        ||a.up.x!=b.up.x||a.up.y!=b.up.y||a.up.z!=b.up.z;
}
inline Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool normalize(Vec3& v){float n=std::sqrt(mgs5vr::dot(v,v));if(!std::isfinite(n)||n<1e-5f)return false;v=v*(1.f/n);return true;}
inline Vec3 bodyAnchor(const CameraPose& camera,float clearance){
    Vec3 forward=camera.target-camera.eye;forward.z=0;
    if(!std::isfinite(clearance)||clearance<0||!normalize(forward))return camera.eye;
    return camera.eye-forward*clearance;
}
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
// Cumulative controller turns are relative to a bridge session. A new bridge
// preserves the current virtual turn; recenter explicitly rebases it to zero.
struct SnapHeading {
    unsigned session{};float baseline{},offset{},angle{};bool valid{};
    void reset(){valid=false;offset=angle=0;}
    Vec3 apply(Vec3 heading,unsigned currentSession,float cumulativeDegrees){
        if(!std::isfinite(cumulativeDegrees))return heading;
        if(!valid){session=currentSession;baseline=cumulativeDegrees;valid=true;}
        else if(session!=currentSession){offset=angle;baseline=cumulativeDegrees;session=currentSession;}
        angle=offset+cumulativeDegrees-baseline;
        const float radians=std::remainder(angle,360.f)*.017453292519943295f;
        const float c=std::cos(radians),s=std::sin(radians);
        return {heading.x*c-heading.y*s,heading.x*s+heading.y*c,heading.z};
    }
};
// Looking almost vertically makes projected head yaw ill-conditioned. Keep the
// body's last reliable heading there; the headset camera remains unfiltered.
struct BodyHeading {
    Vec3 heading{};bool valid{};
    void reset(){valid=false;}
    bool get(Vec3 view,Vec3& result){
        float total=mgs5vr::dot(view,view),horizontal=view.x*view.x+view.y*view.y;
        if(std::isfinite(total)&&total>1e-8f&&horizontal>total*.01f){
            view.z=0;if(normalize(view)){heading=view;valid=true;}
        }
        if(valid)result=heading;
        return valid;
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
