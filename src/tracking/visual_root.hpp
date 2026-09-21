#pragma once
#include "arm_pose.hpp"
namespace amalur {
// Smooth only the visual rig's native root rotation. Controller world targets
// remain exact; gameplay facing/collision and source animation are unchanged.
struct VisualRootRotation {
    mgs5vr::Quat rotation{};
    uint64_t owner{},frame{};
    unsigned center{};
    double time{};
    bool ready{};
    mgs5vr::Pose sample(mgs5vr::Pose root,uint64_t identity,unsigned recenter,uint64_t currentFrame,double now){
        if(!mgs5vr::valid(root)||!std::isfinite(now))return root;
        if(!ready||owner!=identity||center!=recenter||now<time||now-time>.25){
            rotation=root.orientation;owner=identity;center=recenter;frame=currentFrame;time=now;ready=true;return root;
        }
        if(frame!=currentFrame){
            auto target=root.orientation;
            float d=rotation.x*target.x+rotation.y*target.y+rotation.z*target.z+rotation.w*target.w;
            if(d<0){target={-target.x,-target.y,-target.z,-target.w};d=-d;}
            const float angle=std::acos(std::clamp(d,0.f,1.f));
            // 300 degrees/s maximum; follow smaller steps with a 60 ms response.
            const double dt=now-time;
            const float fraction=angle>1e-5f?std::min(float(1-std::exp(-dt/.06)),float(dt*2.617993878/angle)):1.f;
            const float denominator=std::sin(angle);
            const float a=angle>1e-5f?std::sin((1-fraction)*angle)/denominator:0.f;
            const float b=angle>1e-5f?std::sin(fraction*angle)/denominator:1.f;
            rotation={rotation.x*a+target.x*b,rotation.y*a+target.y*b,rotation.z*a+target.z*b,rotation.w*a+target.w*b};
            const float norm=std::sqrt(rotation.x*rotation.x+rotation.y*rotation.y+rotation.z*rotation.z+rotation.w*rotation.w);
            rotation={rotation.x/norm,rotation.y/norm,rotation.z/norm,rotation.w/norm};
            frame=currentFrame;time=now;
        }
        root.orientation=rotation;return root;
    }
};
// Convert the completed scratch rig back to the native root's coordinates,
// compensating the native turn exactly once before the engine remaps it.
inline void rebaseVisualRig(RigBone* bones,unsigned count,mgs5vr::Pose visual,mgs5vr::Pose native){
    const auto correction=mgs5vr::compose(mgs5vr::inverse(native),visual);
    for(unsigned i=0;i<count;++i){
        const auto pose=bonePose(bones[i]);if(!mgs5vr::valid(pose))continue;
        const auto corrected=mgs5vr::compose(correction,pose);
        bones[i].position=corrected.position;bones[i].orientation=nativeQuaternion(corrected.orientation);
    }
}
}
