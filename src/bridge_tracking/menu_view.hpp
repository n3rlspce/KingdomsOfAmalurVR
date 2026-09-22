#pragma once
#include "pose_channel.hpp"
#include <algorithm>
#include <cmath>
namespace amalur {
inline bool fullscreenMenu(int nativePaused,bool dialogue){return nativePaused==1&&!dialogue;}
inline void presentAsMenu(PosePacket& pose,bool menu){
    // Pausing does not invalidate an actually rendered tracked camera. Preserve
    // its image/pose pairing; screens without one naturally retain valid=0.
    if(menu)pose.gameMode=4;
}
inline float menuScale(float setting){return std::isfinite(setting)?std::clamp(setting,.5f,1.5f):1.f;}
}
