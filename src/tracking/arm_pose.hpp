#pragma once
#include <mgs5vr/arm_ik.hpp>
#include <cmath>
#include <cstring>
namespace amalur {
// Native model-space record. Preserve W and the opaque tail byte-for-byte.
struct RigBone {mgs5vr::Vec3 position;float positionW;mgs5vr::Quat orientation;unsigned char opaque[16];};
static_assert(sizeof(RigBone)==48);
inline mgs5vr::Pose bonePose(const RigBone& b){return {b.orientation,b.position};}
inline bool solveRightArm(const RigBone* native,RigBone* output,unsigned count,
    const int16_t* parents,const uint32_t* ids,mgs5vr::Pose target,float unitsPerMeter){
    if(!native||!output||!parents||!ids||count<3||count>64||!mgs5vr::valid(target)
        ||!std::isfinite(unitsPerMeter)||unitsPerMeter<10||unitsPerMeter>1000)return false;
    unsigned shoulder=count,elbow=count,wrist=count;
    for(unsigned i=0;i<count;++i){
        if(parents[i]<-1||parents[i]>=static_cast<int>(i))return false;
        auto assign=[&](unsigned& index){if(index!=count)return false;index=i;return true;};
        if(ids[i]==0x00f21468&&!assign(shoulder))return false;
        if(ids[i]==0x00d1f75e&&!assign(elbow))return false;
        if(ids[i]==0x0088d0eb&&!assign(wrist))return false;
    }
    if(shoulder==count||elbow==count||wrist==count||parents[elbow]!=static_cast<int>(shoulder)||parents[wrist]!=static_cast<int>(elbow))return false;
    mgs5vr::ArmPose arm{bonePose(native[shoulder]),bonePose(native[elbow]),bonePose(native[wrist])};
    if(!mgs5vr::valid(arm.shoulder)||!mgs5vr::valid(arm.elbow)||!mgs5vr::valid(arm.wrist))return false;
    const float meters=1.f/unitsPerMeter;
    arm.shoulder.position=arm.shoulder.position*meters;arm.elbow.position=arm.elbow.position*meters;arm.wrist.position=arm.wrist.position*meters;
    target.position=target.position*meters;
    // Amalur model axes: +X forward, +Y right, +Z up. Elbow stays out and down.
    auto solved=mgs5vr::solveArm(arm,target,{-0.15f,1.f,-0.35f});if(!solved)return false;
    auto pose=solved->pose;
    pose.shoulder.position=pose.shoulder.position*unitsPerMeter;
    pose.elbow.position=pose.elbow.position*unitsPerMeter;pose.wrist.position=pose.wrist.position*unitsPerMeter;
    mgs5vr::Pose delta[64]{};bool affected[64]{};
    memcpy(output,native,count*sizeof(RigBone));
    for(unsigned i=0;i<count;++i){
        mgs5vr::Pose result;
        if(i==shoulder)result=pose.shoulder;
        else if(i==elbow)result=pose.elbow;
        else if(i==wrist)result=pose.wrist;
        else {
            if(parents[i]<0||!affected[parents[i]])continue;
            if(!mgs5vr::valid(bonePose(native[i])))return false;
            delta[i]=delta[parents[i]];affected[i]=true;
            result=mgs5vr::compose(delta[i],bonePose(native[i]));
        }
        if(!mgs5vr::valid(result))return false;
        if(i==shoulder||i==elbow||i==wrist){delta[i]=mgs5vr::compose(result,mgs5vr::inverse(bonePose(native[i])));affected[i]=true;}
        output[i].position=result.position;output[i].orientation=result.orientation;
    }
    return true;
}
}
