#pragma once
#include <mgs5vr/arm_ik.hpp>
#include <cmath>
#include <cstring>
#include <initializer_list>
namespace amalur {
// Native model-space record. Preserve W and the opaque tail byte-for-byte.
struct RigBone {mgs5vr::Vec3 position;float positionW;mgs5vr::Quat orientation;unsigned char opaque[16];};
static_assert(sizeof(RigBone)==48);
// Native transform composition (RVA 6c5770) rotates vectors with q^-1.
// Conjugation converts between native storage and the math library convention.
inline mgs5vr::Quat nativeQuaternion(mgs5vr::Quat q){return {-q.x,-q.y,-q.z,q.w};}
inline mgs5vr::Pose nativePose(mgs5vr::Pose p){p.orientation=nativeQuaternion(p.orientation);return p;}
inline mgs5vr::Pose bonePose(const RigBone& b){return {nativeQuaternion(b.orientation),b.position};}
// Controller-local axes use +Y forward, +Z up. Apply roll about Y, then
// pitch about X, then yaw about Z. This offset never samples an animation pose.
inline bool gripAngleTrim(float pitch,float yaw,float roll,mgs5vr::Pose& out){
    for(float value:{pitch,yaw,roll})if(!std::isfinite(value)||std::abs(value)>180.f)return false;
    const float radians=.008726646259971648f;
    mgs5vr::Pose p{{std::sin(pitch*radians),0,0,std::cos(pitch*radians)},{}},
        y{{0,0,std::sin(yaw*radians),std::cos(yaw*radians)},{}},
        r{{0,std::sin(roll*radians),0,std::cos(roll*radians)}, {}};
    out=mgs5vr::compose(y,mgs5vr::compose(p,r));return mgs5vr::valid(out);
}
enum class ArmSide { Right, Left };
struct ArmReference {
    mgs5vr::ArmPose relative;
    mgs5vr::Pose handRelative[64]{};uint32_t handIds[64]{};bool handBone[64]{};
    unsigned count{};bool ready{};ArmSide side{ArmSide::Right};
};
inline bool armIndices(ArmSide side,unsigned count,const int16_t* parents,const uint32_t* ids,
    unsigned& shoulder,unsigned& elbow,unsigned& wrist){
    if(!parents||!ids||count<3||count>64)return false;
    shoulder=elbow=wrist=count;
    for(unsigned i=0;i<count;++i){
        if(parents[i]<-1||parents[i]>=static_cast<int>(i))return false;
        auto assign=[&](unsigned& index){if(index!=count)return false;index=i;return true;};
        if(ids[i]==(side==ArmSide::Right?0x00f21468u:0x00f1076au)&&!assign(shoulder))return false;
        if(ids[i]==(side==ArmSide::Right?0x00d1f75eu:0x00d0ea60u)&&!assign(elbow))return false;
        if(ids[i]==(side==ArmSide::Right?0x0088d0ebu:0x0087c3edu)&&!assign(wrist))return false;
    }
    if(shoulder==count||elbow==count||wrist==count||parents[elbow]!=static_cast<int>(shoulder)||parents[wrist]!=static_cast<int>(elbow))return false;
    return true;
}
inline bool rightArmIndices(unsigned count,const int16_t* parents,const uint32_t* ids,unsigned& shoulder,unsigned& elbow,unsigned& wrist){
    return armIndices(ArmSide::Right,count,parents,ids,shoulder,elbow,wrist);
}
inline bool leftArmIndices(unsigned count,const int16_t* parents,const uint32_t* ids,unsigned& shoulder,unsigned& elbow,unsigned& wrist){
    return armIndices(ArmSide::Left,count,parents,ids,shoulder,elbow,wrist);
}
// Capture a neutral model-space pose once per calibration, relative to the
// stable body anchor. Attack animations continue running in the native source.
inline bool captureArmReference(ArmSide side,const RigBone* native,unsigned count,const int16_t* parents,
    const uint32_t* ids,mgs5vr::Vec3 anchor,ArmReference& reference){
    unsigned shoulder,elbow,wrist;reference.ready=false;
    if(!native||!mgs5vr::valid(mgs5vr::Pose{{},anchor})||!armIndices(side,count,parents,ids,shoulder,elbow,wrist))return false;
    mgs5vr::ArmPose arm{bonePose(native[shoulder]),bonePose(native[elbow]),bonePose(native[wrist])};
    if(!mgs5vr::valid(arm.shoulder)||!mgs5vr::valid(arm.elbow)||!mgs5vr::valid(arm.wrist))return false;
    arm.shoulder.position=arm.shoulder.position-anchor;
    arm.elbow.position=arm.elbow.position-anchor;arm.wrist.position=arm.wrist.position-anchor;
    ArmReference result;result.relative=arm;result.count=count;result.side=side;
    const auto inverseWrist=mgs5vr::inverse(bonePose(native[wrist]));
    for(unsigned i=wrist+1;i<count;++i){
        if(parents[i]!=static_cast<int>(wrist)&&(parents[i]<0||!result.handBone[parents[i]]))continue;
        if(!mgs5vr::valid(bonePose(native[i])))return false;
        result.handBone[i]=true;result.handIds[i]=ids[i];
        result.handRelative[i]=mgs5vr::compose(inverseWrist,bonePose(native[i]));
    }
    result.ready=true;reference=result;return true;
}
inline bool solveArm(ArmSide side,const RigBone* native,RigBone* output,unsigned count,
    const int16_t* parents,const uint32_t* ids,mgs5vr::Pose target,float unitsPerMeter,
    const ArmReference* reference=nullptr,mgs5vr::Vec3 referenceAnchor={}){
    if(!native||!output||!mgs5vr::valid(target)
        ||!std::isfinite(unitsPerMeter)||unitsPerMeter<10||unitsPerMeter>1000)return false;
    unsigned shoulder,elbow,wrist;
    if(!armIndices(side,count,parents,ids,shoulder,elbow,wrist))return false;
    mgs5vr::ArmPose arm{bonePose(native[shoulder]),bonePose(native[elbow]),bonePose(native[wrist])};
    if(!mgs5vr::valid(arm.shoulder)||!mgs5vr::valid(arm.elbow)||!mgs5vr::valid(arm.wrist))return false;
    if(reference){
        if(!reference->ready||reference->count!=count||reference->side!=side||!mgs5vr::valid(mgs5vr::Pose{{},referenceAnchor}))return false;
        arm=reference->relative;
        arm.shoulder.position=arm.shoulder.position+referenceAnchor;
        arm.elbow.position=arm.elbow.position+referenceAnchor;arm.wrist.position=arm.wrist.position+referenceAnchor;
    }
    const float meters=1.f/unitsPerMeter;
    arm.shoulder.position=arm.shoulder.position*meters;arm.elbow.position=arm.elbow.position*meters;arm.wrist.position=arm.wrist.position*meters;
    target.position=target.position*meters;
    // Amalur model axes: +X forward, +Y right, +Z up. Elbow stays out and down.
    auto solved=mgs5vr::solveArm(arm,target,{-0.15f,side==ArmSide::Right?1.f:-1.f,-0.35f});if(!solved)return false;
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
            if(reference&&reference->handBone[i]){
                if(reference->handIds[i]!=ids[i])return false;
                // The staff's held socket is a finger descendant. Preserve its
                // calibrated wrist-relative pose instead of replaying cast sway.
                result=mgs5vr::compose(pose.wrist,reference->handRelative[i]);
            }else result=mgs5vr::compose(delta[i],bonePose(native[i]));
        }
        if(!mgs5vr::valid(result))return false;
        if(i==shoulder||i==elbow||i==wrist){delta[i]=mgs5vr::compose(result,mgs5vr::inverse(bonePose(native[i])));affected[i]=true;}
        output[i].position=result.position;output[i].orientation=nativeQuaternion(result.orientation);
    }
    return true;
}
inline bool captureRightArmReference(const RigBone* native,unsigned count,const int16_t* parents,
    const uint32_t* ids,mgs5vr::Vec3 anchor,ArmReference& reference){
    return captureArmReference(ArmSide::Right,native,count,parents,ids,anchor,reference);
}
inline bool captureLeftArmReference(const RigBone* native,unsigned count,const int16_t* parents,
    const uint32_t* ids,mgs5vr::Vec3 anchor,ArmReference& reference){
    return captureArmReference(ArmSide::Left,native,count,parents,ids,anchor,reference);
}
inline bool solveRightArm(const RigBone* native,RigBone* output,unsigned count,const int16_t* parents,
    const uint32_t* ids,mgs5vr::Pose target,float unitsPerMeter,const ArmReference* reference=nullptr,mgs5vr::Vec3 referenceAnchor={}){
    return solveArm(ArmSide::Right,native,output,count,parents,ids,target,unitsPerMeter,reference,referenceAnchor);
}
inline bool solveLeftArm(const RigBone* native,RigBone* output,unsigned count,const int16_t* parents,
    const uint32_t* ids,mgs5vr::Pose target,float unitsPerMeter,const ArmReference* reference=nullptr,mgs5vr::Vec3 referenceAnchor={}){
    return solveArm(ArmSide::Left,native,output,count,parents,ids,target,unitsPerMeter,reference,referenceAnchor);
}
}
