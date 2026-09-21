#pragma once
#include "arm_pose.hpp"
#include "held_weapon_profile.hpp"
namespace amalur {
// Apply a rigid hand delta to wrist descendants only. Input/output are private
// model-space copies; the shoulder, elbow, other hand and opaque bytes stay exact.
inline bool attachLeftHand(RigBone* bones,unsigned count,const int16_t* parents,const uint32_t* ids,mgs5vr::Pose wristTarget){
    unsigned shoulder,elbow,wrist;
    if(!bones||!mgs5vr::valid(wristTarget)||!leftArmIndices(count,parents,ids,shoulder,elbow,wrist))return false;
    bool affected[64]{};affected[wrist]=true;RigBone result[64];memcpy(result,bones,count*sizeof(RigBone));
    const auto current=bonePose(bones[wrist]);if(!mgs5vr::valid(current))return false;
    const auto delta=mgs5vr::compose(wristTarget,mgs5vr::inverse(current));
    for(unsigned i=wrist;i<count;++i){
        if(i!=wrist)affected[i]=parents[i]>=0&&affected[parents[i]];
        if(!affected[i])continue;
        auto p=bonePose(bones[i]);if(!mgs5vr::valid(p))return false;
        p=mgs5vr::compose(delta,p);if(!mgs5vr::valid(p))return false;
        result[i].position=p.position;result[i].orientation=nativeQuaternion(p.orientation);
    }
    memcpy(bones,result,count*sizeof(RigBone));return true;
}
// Seat the hand's native weapon socket on the handle. The incoming target's
// orientation is the desired wrist orientation, but its position is the grip
// point. Measure socket-to-wrist offset from this frame's solved hand.
inline bool attachLeftHandAtSocket(RigBone* bones,unsigned count,const int16_t* parents,
    const uint32_t* ids,unsigned socket,mgs5vr::Pose target){
    unsigned shoulder,elbow,wrist;
    if(!bones||!mgs5vr::valid(target)||!leftArmIndices(count,parents,ids,shoulder,elbow,wrist)
        ||socket>=count||socket==wrist)return false;
    unsigned ancestor=socket;
    for(unsigned depth=0;depth<count&&ancestor!=wrist;++depth){
        if(parents[ancestor]<0||parents[ancestor]>=static_cast<int>(ancestor))return false;
        ancestor=static_cast<unsigned>(parents[ancestor]);
    }
    if(ancestor!=wrist)return false;
    const auto current=bonePose(bones[wrist]),grip=bonePose(bones[socket]);
    if(!mgs5vr::valid(current)||!mgs5vr::valid(grip))return false;
    const auto offset=mgs5vr::rotate(mgs5vr::inverse(current).orientation,grip.position-current.position);
    target.position=target.position-mgs5vr::rotate(target.orientation,offset);
    return attachLeftHand(bones,count,parents,ids,target);
}
// Metres along the captured handle's local Z; staff retains its tested point.
// Other hilt lengths require live fitting; no unverified finger curl is applied.
inline bool supportGripPoint(HeldWeaponKind kind,mgs5vr::Vec3& point){
    switch(kind){
        case HeldWeaponKind::Longsword:point={0,0,-.07f};return true;
        case HeldWeaponKind::Staff:case HeldWeaponKind::Greatsword:point={0,0,-.12f};return true;
        case HeldWeaponKind::Hammer:point={0,0,-.16f};return true;
        default:return false;
    }
}
inline bool supportGripEligible(HeldWeaponKind kind,unsigned selected,bool offhandFree){
    mgs5vr::Vec3 point;return selected<=1&&offhandFree&&supportGripPoint(kind,point);
}
struct SupportGrip {
    bool attached{};uint32_t owner{},generation{};mgs5vr::Pose relative{};
    HeldWeaponKind kind{HeldWeaponKind::None};unsigned selected{2};
    void release(){attached=false;owner=generation=0;relative={};kind=HeldWeaponKind::None;selected=2;}
    bool update(bool eligible,bool held,uint32_t weapon,unsigned center,float scale,
        mgs5vr::Pose handle,mgs5vr::Pose left,mgs5vr::Pose& target,
        HeldWeaponKind weaponKind=HeldWeaponKind::Staff,unsigned selectedSlot=0){
        if(!eligible||!supportGripEligible(weaponKind,selectedSlot,true)||!held||!weapon||!std::isfinite(scale)||scale<10||scale>1000||!mgs5vr::valid(handle)||!mgs5vr::valid(left)){release();return false;}
        if(attached&&(selected!=selectedSlot||kind!=weaponKind)){release();return false;}
        if(owner!=weapon||generation!=center)release();
        mgs5vr::Vec3 point;supportGripPoint(weaponKind,point);point=point*scale;
        if(!attached){
            auto local=mgs5vr::compose(mgs5vr::inverse(handle),left);auto d=local.position-point;
            if(mgs5vr::dot(d,d)>.25f*.25f*scale*scale)return false;
            relative=local;relative.position=point;owner=weapon;generation=center;kind=weaponKind;selected=selectedSlot;attached=true;
        }
        target=mgs5vr::compose(handle,relative);return true;
    }
};
}
