#pragma once
#include "arm_pose.hpp"
namespace amalur {
inline bool thickenArmBone(RigBone& bone,uint32_t id,float factor){
    if(factor==1.f||!std::isfinite(factor)||factor<.5f||factor>1.5f)return false;
    if(id!=0xf21468&&id!=0xd1f75e&&id!=0xf1076a&&id!=0xd0ea60)return false;
    float scale[3]{1,1,1};
    if(bone.opaque[12]&0x40)memcpy(scale,bone.opaque,12);
    for(float x:scale)if(!std::isfinite(x)||x<=0||x>10)return false;
    // Recorded native arm axes: X runs shoulder->elbow and elbow->wrist.
    // Model-space scales do not propagate into the independently stored hands.
    scale[1]*=factor;scale[2]*=factor;
    memcpy(bone.opaque,scale,12);bone.opaque[12]|=0x40;return true;
}
}
