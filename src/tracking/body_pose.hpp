#pragma once
#include "arm_pose.hpp"
namespace amalur {
// Translate the complete visual pose as one unit. Native animation rotations,
// relative joint positions and vertical gait remain intact; gameplay transforms
// are never written. Foot planting is a separate, not-yet-implemented stage.
inline bool stabilizeBody(const RigBone* native,RigBone* output,unsigned count,
    const int16_t* parents,const uint32_t* ids,mgs5vr::Vec3 headAnchor){
    using namespace mgs5vr;
    if(!native||!output||!parents||!ids||count>64||count<3
       ||!std::isfinite(headAnchor.x)||!std::isfinite(headAnchor.y))return false;
    unsigned head=count;
    for(unsigned i=0;i<count;++i){
        if(parents[i]<-1||parents[i]>=static_cast<int>(i))return false;
        if(ids[i]==0x005a2e4c){if(head!=count)return false;head=i;}
    }
    if(head==count||!valid(bonePose(native[head])))return false;
    Vec3 offset{headAnchor.x-native[head].position.x,headAnchor.y-native[head].position.y,0};
    if(dot(offset,offset)>10000)return false;
    RigBone result[64];memcpy(result,native,count*sizeof(RigBone));
    for(unsigned i=0;i<count;++i){
        // The game's trailing sentinel is not a transform. Preserve it exactly.
        if(!valid(bonePose(native[i])))continue;
        result[i].position=result[i].position+offset;
    }
    memcpy(output,result,count*sizeof(RigBone));return true;
}
struct BodyReference {
    mgs5vr::Pose relative[64]{};
    uint32_t ids[64]{};int16_t parents[64]{};
    bool upper[64]{};unsigned count{};bool ready{};
    mgs5vr::Vec3 anchor{},initialOffset{};
};
// First-person torso must share the tracked arms' stable reference. Replaying
// jog sway in the chest while IK fixes the shoulders makes their seams shake.
// Retain the native lower-body animation, with one stable translation, and
// replace the spine subtree with its calibrated pose before solving the hands.
inline bool stabilizeTrackedBody(const RigBone* native,RigBone* output,unsigned count,
    const int16_t* parents,const uint32_t* ids,mgs5vr::Vec3 anchor,BodyReference& reference){
    using namespace mgs5vr;
    if(!native||!output||!parents||!ids||count<3||count>64||!valid(Pose{{},anchor}))return false;
    unsigned spine=count,head=count;
    for(unsigned i=0;i<count;++i){
        if(parents[i]<-1||parents[i]>=static_cast<int>(i))return false;
        if(ids[i]==0x688528){if(spine!=count)return false;spine=i;}
        if(ids[i]==0x5a2e4c){if(head!=count)return false;head=i;}
    }
    if(spine==count||head==count)return false;
    BodyReference next=reference;
    if(!next.ready){
        RigBone initial[64];
        if(!stabilizeBody(native,initial,count,parents,ids,anchor))return false;
        next={};next.count=count;next.anchor=anchor;
        next.initialOffset=initial[head].position-native[head].position;
        for(unsigned i=0;i<count;++i){
            next.ids[i]=ids[i];next.parents[i]=parents[i];
            next.upper[i]=i==spine||(parents[i]>=0&&next.upper[parents[i]]);
            if(next.upper[i]){
                if(!valid(bonePose(initial[i])))return false;
                next.relative[i]=bonePose(initial[i]);next.relative[i].position=next.relative[i].position-anchor;
            }
        }
        if(!next.upper[head])return false;
        next.ready=true;
    }
    if(next.count!=count||memcmp(next.ids,ids,count*sizeof(uint32_t))
        ||memcmp(next.parents,parents,count*sizeof(int16_t)))return false;
    const auto offset=next.initialOffset+(anchor-next.anchor);
    if(dot(offset,offset)>10000)return false;
    RigBone result[64];memcpy(result,native,count*sizeof(RigBone));
    for(unsigned i=0;i<count;++i){
        if(next.upper[i]){
            result[i].position=next.relative[i].position+anchor;
            result[i].orientation=nativeQuaternion(next.relative[i].orientation);
        }else if(valid(bonePose(native[i])))result[i].position=result[i].position+offset;
    }
    memcpy(output,result,count*sizeof(RigBone));reference=next;return true;
}
}
