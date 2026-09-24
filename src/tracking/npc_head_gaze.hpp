#pragma once
#include "arm_pose.hpp"
#include "camera_pose.hpp"
#include <algorithm>

namespace amalur {
// Rotate only the head subtree in the freshly evaluated visual pose. The
// native actor transform and animation source remain untouched.
inline bool npcHeadGaze(const RigBone* native,RigBone* output,unsigned count,
    const int16_t* parents,const uint32_t* ids,Pose root,Vec3 viewer){
    if(!native||!output||!parents||!ids||count<2||count>128||!mgs5vr::valid(root))return false;
    unsigned head=count;bool subtree[128]{};
    for(unsigned i=0;i<count;++i){
        if(parents[i]<-1||parents[i]>=static_cast<int>(i))return false;
        if(ids[i]==0x005a2e4c){if(head!=count)return false;head=i;}
    }
    if(head==count||!mgs5vr::valid(bonePose(native[head])))return false;
    const Pose worldHead=mgs5vr::compose(root,bonePose(native[head]));
    // The verified Amalur head bind pose points local +X up the neck. Its
    // face looks along local -Y; aiming +X pitched every NPC toward the floor.
    Vec3 from=mgs5vr::rotate(worldHead.orientation,{0,-1,0}),to=viewer-worldHead.position;
    const float distance2=mgs5vr::dot(to,to);
    if(!std::isfinite(distance2)||distance2<900.f||distance2>360000.f||
       !normalize(from)||!normalize(to))return false;
    const float cosine=std::clamp(mgs5vr::dot(from,to),-1.f,1.f);
    const float angle=std::acos(cosine);
    if(angle<.008f)return false;
    Vec3 axis=cross(from,to);if(!normalize(axis))return false;
    const float half=std::min(angle,1.31f)*.5f;
    const mgs5vr::Quat turn{axis.x*std::sin(half),axis.y*std::sin(half),axis.z*std::sin(half),std::cos(half)};
    const Pose delta{turn,worldHead.position-mgs5vr::rotate(turn,worldHead.position)};
    const Pose inverseRoot=mgs5vr::inverse(root);
    RigBone result[128];memcpy(result,native,count*sizeof(RigBone));
    for(unsigned i=0;i<count;++i){
        subtree[i]=i==head||(parents[i]>=0&&subtree[parents[i]]);
        if(!subtree[i])continue;
        if(!mgs5vr::valid(bonePose(native[i])))return false;
        const Pose world=mgs5vr::compose(root,bonePose(native[i]));
        const Pose local=mgs5vr::compose(inverseRoot,mgs5vr::compose(delta,world));
        if(!mgs5vr::valid(local))return false;
        result[i].position=local.position;
        result[i].orientation=nativeQuaternion(local.orientation);
    }
    memcpy(output,result,count*sizeof(RigBone));
    publishRigOverrides(native,output,count);
    return true;
}
}
