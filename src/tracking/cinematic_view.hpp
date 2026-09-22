#pragma once
#include "camera_pose.hpp"
#include <cstdint>

namespace amalur {
// Follow the authored scene position, but never add its animated yaw/pitch/roll
// to the viewer's head. Scene changes and abrupt native shot cuts rebase yaw.
struct CinematicView {
    uintptr_t scene{},camera{};uint32_t localCenter{},bridgeCenter{};
    Pose origin{};Vec3 heading{};bool active{};
    Vec3 lastNativeEye{},lastNativeForward{};uint64_t lastNativeTick{};
    void reset(){active=false;lastNativeTick=0;}
    bool apply(uintptr_t sceneId,uintptr_t cameraId,uint32_t local,uint32_t bridge,
               CameraPose native,Pose head,float scale,CameraPose& out,uint64_t now=0){
        if(!sceneId||!cameraId||!mgs5vr::valid(head)||!std::isfinite(scale)||scale<=0||
           !std::isfinite(native.eye.x)||!std::isfinite(native.eye.y)||!std::isfinite(native.eye.z))return false;
        auto forward=native.target-native.eye;forward.z=0;
        if(!std::isfinite(forward.x)||!std::isfinite(forward.y)||!normalize(forward))return false;
        bool cut=false;
        if(active&&scene==sceneId&&camera==cameraId&&lastNativeTick&&now>lastNativeTick&&now-lastNativeTick<=100){
            // Compare only authored inputs, never headset motion or accumulated
            // pan angle. Smooth pans/travel keep free head-look. Same-camera
            // hard cuts typically jump direction or position between updates.
            const auto delta=native.eye-lastNativeEye;
            const float metresSquared=mgs5vr::dot(delta,delta)/(scale*scale);
            const float cosine=mgs5vr::dot(forward,lastNativeForward);
            cut=cosine<.819152f || metresSquared>2.25f ||
                (metresSquared>.5625f&&cosine<.984808f);
        }
        if(!active||scene!=sceneId||camera!=cameraId||cut){
            heading=forward;origin=levelOrigin(head);scene=sceneId;camera=cameraId;
            localCenter=local;bridgeCenter=bridge;active=true;
        }else if(localCenter!=local||bridgeCenter!=bridge){
            origin=levelOrigin(head);localCenter=local;bridgeCenter=bridge;
        }
        lastNativeEye=native.eye;lastNativeForward=forward;lastNativeTick=now;
        return trackedCamera({native.eye,native.eye+heading*200.f,{0,0,1}},
            mgs5vr::compose(mgs5vr::inverse(origin),head),scale,out);
    }
};
}
