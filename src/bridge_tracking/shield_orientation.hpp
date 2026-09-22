#pragma once
#include "arm_pose.hpp"

namespace amalur {
// Exact attached shield skeleton captured for asset 2322. Runtime validates
// asset and owner identity. Local X remains provisional pending headset fit.
inline bool uprightShield(RigBone* bones,unsigned count,const uint32_t* ids,const int16_t* parents){
    constexpr uint32_t expectedIds[]{6711025,17330092};
    constexpr int16_t expectedParents[]{-1,0};
    if(!bones||!ids||!parents||count!=2||memcmp(ids,expectedIds,sizeof(expectedIds))
        ||memcmp(parents,expectedParents,sizeof(expectedParents)))return false;
    if(!mgs5vr::valid(bonePose(bones[0]))||!mgs5vr::valid(bonePose(bones[1])))return false;
    RigBone candidate=bones[1];
    const auto flipped=mgs5vr::compose(bonePose(candidate),mgs5vr::Pose{{1,0,0,0},{}});
    candidate.orientation=nativeQuaternion(flipped.orientation);
    candidate.opaque[12]|=0x1c;
    if(!mgs5vr::valid(bonePose(candidate)))return false;
    bones[1].orientation=candidate.orientation;bones[1].opaque[12]=candidate.opaque[12];return true;
}

// Caller must verify current engine identity before supplying this bone.
// New native quaternion or orientation bits win; unrelated flags survive.
inline bool restoreShieldOrientation(RigBone& currentBone,const RigBone& beforeBone,const RigBone& afterBone){
    constexpr unsigned char orientationMask=0x1c;
    if(memcmp(&currentBone.orientation,&afterBone.orientation,sizeof(currentBone.orientation))
        ||(currentBone.opaque[12]&orientationMask)!=(afterBone.opaque[12]&orientationMask))return false;
    currentBone.orientation=beforeBone.orientation;
    currentBone.opaque[12]=static_cast<unsigned char>((currentBone.opaque[12]&~orientationMask)
        |(beforeBone.opaque[12]&orientationMask));
    return true;
}
}
