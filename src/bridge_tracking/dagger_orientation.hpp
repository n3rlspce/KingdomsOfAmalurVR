#pragma once
#include "weapon_pose.hpp"

namespace amalur {
// Known dagger model-space bone branches. Local X is a provisional grip-flip
// axis and must be checked in the headset for this authored weapon layout.
inline bool uprightLeftDagger(RigBone* bones,unsigned count,const int16_t* parents,const uint32_t* ids){
    constexpr uint32_t expectedIds[]{11436941,11992818,11092278,14407505,10760771,15110326,13087492};
    constexpr int16_t expectedParents[]{-1,0,1,1,0,4,4};
    if(!bones||!parents||!ids||count!=7||memcmp(ids,expectedIds,sizeof(expectedIds))
        ||memcmp(parents,expectedParents,sizeof(expectedParents)))return false;
    for(unsigned i=0;i<count;++i)if(!mgs5vr::valid(bonePose(bones[i])))return false;
    RigBone candidate[7];memcpy(candidate,bones,sizeof(candidate));
    const auto anchor=bonePose(bones[1]);
    const auto flipped=mgs5vr::compose(anchor,Pose{{1,0,0,0},{}});
    const auto rotation=mgs5vr::compose(flipped,mgs5vr::inverse(anchor)).orientation;
    for(unsigned i=1;i<4;++i){
        const auto pose=bonePose(bones[i]);
        candidate[i].orientation=nativeQuaternion(mgs5vr::compose(Pose{rotation,{}},Pose{pose.orientation,{}}).orientation);
        if(i!=1)candidate[i].position=anchor.position+mgs5vr::rotate(rotation,pose.position-anchor.position);
        if(!mgs5vr::valid(bonePose(candidate[i])))return false;
    }
    memcpy(bones,candidate,sizeof(candidate));return true;
}

// Position restoration belongs to the existing translation/native-remap path.
// Restore only our unchanged orientation output; newer native output wins.
inline bool restoreDaggerOrientation(RigBone* current,const RigBone* before,const RigBone* after,
    const HeldWeaponIdentity& identity,const HeldWeaponIdentity& currentIdentity){
    if(!current||!before||!after||!sameHeldWeapon(identity,currentIdentity))return false;
    for(unsigned i=1;i<4;++i)
        if(!memcmp(&current[i].orientation,&after[i].orientation,sizeof(current[i].orientation)))
            current[i].orientation=before[i].orientation;
    return true;
}
}
