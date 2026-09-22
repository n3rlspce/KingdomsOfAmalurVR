#pragma once
#include "camera_pose.hpp"
#include <cstdint>

namespace amalur {
// XInput movement is camera-relative, but the rendered headset heading can
// diverge from the game's chase camera. Express desired movement in the native
// camera basis instead of applying a fixed quarter-turn to controller axes.
struct MovementBasis {
    Vec3 native{},head{};uint64_t sampled{};bool valid{};
    void sample(Vec3 nativeForward,Vec3 headForward,bool enabled,uint64_t now){
        nativeForward.z=headForward.z=0;
        valid=enabled&&normalize(nativeForward)&&normalize(headForward);
        if(valid){native=nativeForward;head=headForward;sampled=now;}
    }
    bool transform(float& x,float& y,uint64_t now)const{
        if(!valid||sampled>now||now-sampled>=250||!std::isfinite(x)||!std::isfinite(y))return false;
        const Vec3 nativeRight{-native.y,native.x,0},headRight{-head.y,head.x,0};
        const auto desired=headRight*x+head*y;
        x=mgs5vr::dot(desired,nativeRight);y=mgs5vr::dot(desired,native);
        // Keep diagonals inside XInput's representable component range.
        const float peak=std::fmax(1.f,std::fmax(std::abs(x),std::abs(y)));
        x/=peak;y/=peak;
        return true;
    }
};
}
