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
}
