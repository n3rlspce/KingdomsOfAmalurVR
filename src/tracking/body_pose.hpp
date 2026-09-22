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
struct BodyReferenceKey {
    uint32_t root{},owner{},asset{},blob{},center{};
    bool operator==(const BodyReferenceKey& other)const{
        return root==other.root&&owner==other.owner&&asset==other.asset&&blob==other.blob&&center==other.center;
    }
};
struct BodyReference {
    mgs5vr::Pose relative[64]{};
    uint32_t ids[64]{};int16_t parents[64]{};
    bool upper[64]{};unsigned count{};bool ready{};
    mgs5vr::Vec3 anchor{},initialOffset{};
    BodyReferenceKey key{};uint64_t revision{};
};
// First-person torso must share the tracked arms' stable reference. Replaying
// jog sway in the chest while IK fixes the shoulders makes their seams shake.
// Retain the native lower-body animation, with a translation that preserves the current pelvis/spine seam, and
// replace the spine subtree with its calibrated pose before solving the hands.
inline bool stabilizeTrackedBody(const RigBone* native,RigBone* output,unsigned count,
    const int16_t* parents,const uint32_t* ids,mgs5vr::Vec3 anchor,BodyReference& reference,BodyReferenceKey key={}){
    using namespace mgs5vr;
    if(!native||!output||!parents||!ids||count<3||count>64||!valid(Pose{{},anchor}))return false;
    unsigned spine=count,head=count,right=count,left=count;
    for(unsigned i=0;i<count;++i){
        if(parents[i]<-1||parents[i]>=static_cast<int>(i))return false;
        if(ids[i]==0x688528){if(spine!=count)return false;spine=i;}
        if(ids[i]==0x5a2e4c){if(head!=count)return false;head=i;}
        if(ids[i]==0xf21468){if(right!=count)return false;right=i;}
        if(ids[i]==0xf1076a){if(left!=count)return false;left=i;}
    }
    if(spine==count||head==count)return false;
    BodyReference next=reference;
    // Rebuild in a local candidate. A bad transition must neither discard the
    // last valid reference nor partially write the caller's output.
    if(next.ready&&(!(next.key==key)||next.count!=count
        ||memcmp(next.ids,ids,count*sizeof(uint32_t))
        ||memcmp(next.parents,parents,count*sizeof(int16_t))))next={};
    if(!next.ready){
        RigBone calibrated[64],initial[64];memcpy(calibrated,native,count*sizeof(RigBone));
        bool upper[64]{};
        for(unsigned i=0;i<count;++i)upper[i]=i==spine||(parents[i]>=0&&upper[parents[i]]);
        // The first native frame may contain an idle/combat torso twist. Freezing
        // it also freezes that yaw forever. Native model forward is +X and the
        // right-minus-left shoulder axis is +Y; remove only its measured yaw.
        // Rotate around the spine attachment, then re-anchor the whole pose so
        // the lower-body seam and the tracked head remain in agreement.
        if(right<count&&left<count&&upper[right]&&upper[left]){
            if(!valid(bonePose(native[right]))||!valid(bonePose(native[left]))||!valid(bonePose(native[spine])))return false;
            const auto lateral=native[right].position-native[left].position;
            const float horizontal=lateral.x*lateral.x+lateral.y*lateral.y;
            if(!std::isfinite(horizontal)||horizontal<1.f)return false;
            const float half=.5f*std::atan2(lateral.x,lateral.y);
            const Pose turn{{0,0,std::sin(half),std::cos(half)}, {}};
            const auto pivot=native[spine].position;
            for(unsigned i=0;i<count;++i)if(upper[i]){
                auto pose=bonePose(native[i]);if(!valid(pose))return false;
                pose.position=pose.position-pivot;pose=compose(turn,pose);
                calibrated[i].position=pose.position+pivot;
                calibrated[i].orientation=nativeQuaternion(pose.orientation);
            }
        }
        if(!stabilizeBody(calibrated,initial,count,parents,ids,anchor))return false;
        next={};next.count=count;next.anchor=anchor;next.key=key;next.revision=reference.revision+1;
        next.initialOffset=initial[spine].position-native[spine].position;
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
    // The upper spine is frozen, while native animation still moves its
    // attachment relative to the pelvis. Recompute the shared lower-body
    // translation at that attachment instead of retaining first-frame sway.
    // Joint differences within the lower body and its native rotations remain.
    if(!valid(bonePose(native[spine])))return false;
    const auto offset=next.relative[spine].position+anchor-native[spine].position;
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
// Restore native arm animation relative to each stabilized shoulder parent.
// This keeps native hands moving even when the chest reference stays frozen.
inline bool restoreNativeArmAnimation(const RigBone* native,RigBone* stable,unsigned count,
    const int16_t* parents,const uint32_t* ids){
    for(auto side:{ArmSide::Right,ArmSide::Left}){
        unsigned shoulder,elbow,wrist;
        if(!armIndices(side,count,parents,ids,shoulder,elbow,wrist)||parents[shoulder]<0)return false;
        const auto parent=parents[shoulder];
        const auto correction=mgs5vr::compose(bonePose(stable[parent]),mgs5vr::inverse(bonePose(native[parent])));
        bool arm[64]{};
        for(unsigned i=0;i<count;++i){
            arm[i]=i==shoulder||(parents[i]>=0&&arm[parents[i]]);
            if(!arm[i])continue;
            const auto original=bonePose(native[i]);if(!mgs5vr::valid(original))return false;
            const auto pose=mgs5vr::compose(correction,original);
            stable[i].position=pose.position;stable[i].orientation=nativeQuaternion(pose.orientation);
        }
    }
    return true;
}

}
