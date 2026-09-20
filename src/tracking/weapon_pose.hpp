#pragma once
#include "camera_pose.hpp"
#include "arm_pose.hpp"
#include <cstdint>
namespace amalur {
inline bool freshWeaponPose(const Pose& pose,uint64_t tick,uint64_t now){
    return tick&&tick<=now&&now-tick<250&&mgs5vr::valid(pose);
}
// A dagger Fab contains both blades. Keep the held map while either arm has a
// usable target; solveArm independently leaves an untracked arm native. Losing
// one controller must not sheath the other, still-tracked hand's dagger.
// Caller must first validate dagger ownership, skeleton and held-slot mapping.
inline uintptr_t trackedDaggerSlot(uintptr_t nativeSlot,bool rightValid,bool leftValid){
    return (rightValid||leftValid)&&(nativeSlot==9||nativeSlot==11)?7:nativeSlot;
}
// A recent successful held remap is evidence that suppressing an idle hide
// request cannot reveal an uncalibrated or different weapon.
inline bool freshHeldDaggers(uint32_t owner,uint32_t heldOwner,uint64_t frame,uint64_t now,
    const Pose& right,uint64_t rightTick,const Pose& left,uint64_t leftTick){
    return owner&&owner==heldOwner&&frame&&frame<=now&&now-frame<250
        &&(freshWeaponPose(right,rightTick,now)||freshWeaponPose(left,leftTick,now));
}
// Centimetres in the solved wrist frame: +X outward (mirrored on the left),
// +Y forward, +Z up. Translate weapon branches only; never alter wrist targets.
inline bool translateDaggerGrip(const RigBone* native,RigBone* output,unsigned count,
    const int16_t* parents,const uint32_t* ids,Pose weaponWorld,Pose rightWristWorld,
    Pose leftWristWorld,Vec3 centimetres,float unitsPerMeter,bool rightTracked=true,bool leftTracked=true){
    constexpr uint32_t daggerIds[]{11436941,11992818,11092278,14407505,10760771,15110326,13087492};
    constexpr int16_t daggerParents[]{-1,0,1,1,0,4,4};
    if(!native||!output||!parents||!ids||count!=7
        ||memcmp(ids,daggerIds,sizeof(daggerIds))||memcmp(parents,daggerParents,sizeof(daggerParents))
        ||!mgs5vr::valid(weaponWorld)||!mgs5vr::valid(rightWristWorld)||!mgs5vr::valid(leftWristWorld)
        ||!std::isfinite(unitsPerMeter)||unitsPerMeter<10||unitsPerMeter>1000)return false;
    for(float value:{centimetres.x,centimetres.y,centimetres.z})
        if(!std::isfinite(value)||std::abs(value)>20.f)return false;
    for(unsigned i=0;i<count;++i)if(!mgs5vr::valid(bonePose(native[i])))return false;
    RigBone result[7];memcpy(result,native,sizeof(result));
    if(centimetres.x!=0||centimetres.y!=0||centimetres.z!=0){
        auto right=centimetres*(unitsPerMeter*.01f),left=right;left.x=-left.x;
        auto inverseRoot=mgs5vr::inverse(weaponWorld).orientation;
        auto rightDelta=mgs5vr::rotate(inverseRoot,mgs5vr::rotate(rightWristWorld.orientation,right));
        auto leftDelta=mgs5vr::rotate(inverseRoot,mgs5vr::rotate(leftWristWorld.orientation,left));
        for(unsigned i=1;i<count;++i){
            if(i<4?!leftTracked:!rightTracked)continue;
            result[i].position=result[i].position+(i<4?leftDelta:rightDelta);
            if(!mgs5vr::valid(bonePose(result[i])))return false;
        }
    }
    memcpy(output,result,sizeof(result));return true;
}
struct HeldWeaponIdentity {
    uintptr_t object{},root{},buffer{};uint32_t index{},owner{},rootOwner{},asset{},count{};
};
inline bool sameHeldWeapon(const HeldWeaponIdentity& a,const HeldWeaponIdentity& b){
    return a.object&&a.root&&a.buffer&&a.count==7&&b.count==7
        &&a.object==b.object&&a.root==b.root&&a.buffer==b.buffer&&a.index==b.index
        &&a.owner==b.owner&&a.rootOwner==b.rootOwner&&a.asset==b.asset;
}
// Runtime caller freshly resolves engine handles and validates membership first.
// Native changes win: restore only position fields still equal to our output.
inline bool restoreDaggerTranslation(RigBone* current,const RigBone* before,const RigBone* after,
    const HeldWeaponIdentity& saved,const HeldWeaponIdentity& live){
    if(!sameHeldWeapon(saved,live)||!current||!before||!after)return false;
    for(unsigned i=1;i<7;++i)
        if(!memcmp(&current[i].position,&after[i].position,sizeof(Vec3)))current[i].position=before[i].position;
    return true;
}
// The game's camera basis reflects XR's handedness. Quaternion vector parts
// are axial vectors, so reflection needs a minus sign as well as the axis map.
inline bool gripInGame(CameraPose rig,Pose relative,float scale,Pose& out){
    Vec3 forward=rig.target-rig.eye;forward.z=0;if(!normalize(forward)||!mgs5vr::valid(relative)||!std::isfinite(scale)||scale<=0)return false;
    Vec3 up{0,0,1},right=cross(up,forward);
    auto map=[&](Vec3 v){return right*v.x+up*v.y-forward*v.z;};
    auto v=map({relative.orientation.x,relative.orientation.y,relative.orientation.z})*-1.f;
    mgs5vr::Quat reflected{v.x,v.y,v.z,relative.orientation.w};
    // Zero XR rotation corresponds to a game frame facing +Y, yawed with rig.
    float yaw=std::atan2(-forward.x,forward.y);
    out.orientation=mgs5vr::compose({reflected,{}},{{0,0,std::sin(yaw*.5f),std::cos(yaw*.5f)}, {}}).orientation;
    out.position=rig.eye+map(relative.position)*scale;
    return mgs5vr::valid(out);
}
}
