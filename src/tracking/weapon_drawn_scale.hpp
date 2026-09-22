#pragma once
#include "arm_pose.hpp"
#include <cstdint>
namespace amalur {
struct DrawnScaleIdentity {uintptr_t object{},root{},buffer{};uint32_t owner{},rootOwner{},asset{},count{},fabIndex{};};
inline bool sameDrawnScale(const DrawnScaleIdentity& a,const DrawnScaleIdentity& b){
    return a.object&&a.root&&a.buffer&&a.owner&&a.rootOwner&&a.fabIndex>=2&&a.asset==5457&&a.count==4
        &&a.object==b.object&&a.root==b.root&&a.buffer==b.buffer&&a.owner==b.owner
        &&a.rootOwner==b.rootOwner&&a.asset==b.asset&&a.count==b.count&&a.fabIndex==b.fabIndex;
}
struct DrawnScaleValue {float xyz[3];unsigned char flag;};
inline DrawnScaleValue drawnScaleValue(const RigBone& bone){
    DrawnScaleValue s;memcpy(s.xyz,bone.opaque,12);s.flag=bone.opaque[12]&0x40;return s;
}
inline bool sameDrawnScaleValue(const DrawnScaleValue& a,const DrawnScaleValue& b){
    return a.flag==b.flag&&!memcmp(a.xyz,b.xyz,12);
}
// PID39420, asset5457: all four bones animate uniformly .8 -> 1 -> .8;
// world and source socket scales stay 1. Only destination scale is corrected.
class DrawnScaleLease {
    DrawnScaleIdentity identity_{};DrawnScaleValue before_[4]{},after_[4]{};bool active_{};
public:
    bool active()const{return active_;}
    const DrawnScaleIdentity& identity()const{return identity_;}
    void clear(){active_=false;identity_={};}
    bool begin(RigBone* bones,const DrawnScaleIdentity& identity){
        if(active_||!bones||!sameDrawnScale(identity,identity))return false;
        DrawnScaleValue values[4];
        for(unsigned i=0;i<4;++i){
            values[i]=drawnScaleValue(bones[i]);
            if(values[i].flag!=0x40)return false;
            for(float v:values[i].xyz)if(!std::isfinite(v)||v<.79999f||v>1.00001f)return false;
            for(float v:values[i].xyz)if(std::abs(v-values[0].xyz[0])>.00001f)return false;
        }
        // A native full-sized frame needs no ownership or mutation.
        if(values[0].xyz[0]==1.f)return false;
        identity_=identity;active_=true;memcpy(before_,values,sizeof(values));
        const float full[]{1,1,1};
        // Remapper slot5 copies source62 -> destination0 and55 -> destination1,
        // retaining these destination scales. Its native child pass handles2/3.
        for(unsigned i=0;i<2;++i){memcpy(bones[i].opaque,full,12);bones[i].opaque[12]|=0x40;}
        for(unsigned i=0;i<4;++i)after_[i]=drawnScaleValue(bones[i]);
        return true;
    }
    void finish(const RigBone* bones,const DrawnScaleIdentity& live){
        if(!active_||!bones||!sameDrawnScale(identity_,live)){clear();return;}
        for(unsigned i=0;i<4;++i)after_[i]=drawnScaleValue(bones[i]);
    }
    void restore(RigBone* bones,const DrawnScaleIdentity& live){
        if(active_&&bones&&sameDrawnScale(identity_,live))for(unsigned i=0;i<4;++i){
            if(sameDrawnScaleValue(drawnScaleValue(bones[i]),after_[i])){
                memcpy(bones[i].opaque,before_[i].xyz,12);
                bones[i].opaque[12]=(bones[i].opaque[12]&~0x40)|before_[i].flag;
            }
        }
        clear();
    }
};
}
