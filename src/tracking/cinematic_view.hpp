#pragma once
#include "camera_pose.hpp"
#include <cstdint>

namespace amalur {
// Follow the authored scene position, but never add its animated yaw/pitch/roll
// to the viewer's head. A new scene establishes a new horizontal reference.
struct CinematicView {
    uintptr_t scene{},camera{};uint32_t localCenter{},bridgeCenter{};
    Pose origin{};Vec3 heading{};bool active{};
    void reset(){active=false;}
    bool apply(uintptr_t sceneId,uintptr_t cameraId,uint32_t local,uint32_t bridge,
               CameraPose native,Pose head,float scale,CameraPose& out){
        if(!sceneId||!cameraId||!mgs5vr::valid(head)||!std::isfinite(scale)||scale<=0||
           !std::isfinite(native.eye.x)||!std::isfinite(native.eye.y)||!std::isfinite(native.eye.z))return false;
        if(!active||scene!=sceneId||camera!=cameraId){
            auto forward=native.target-native.eye;forward.z=0;
            if(!normalize(forward))return false;
            heading=forward;origin=levelOrigin(head);scene=sceneId;camera=cameraId;
            localCenter=local;bridgeCenter=bridge;active=true;
        }else if(localCenter!=local||bridgeCenter!=bridge){
            origin=levelOrigin(head);localCenter=local;bridgeCenter=bridge;
        }
        return trackedCamera({native.eye,native.eye+heading*200.f,{0,0,1}},
            mgs5vr::compose(mgs5vr::inverse(origin),head),scale,out);
    }
};
}
