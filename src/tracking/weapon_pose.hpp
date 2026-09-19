#pragma once
#include "camera_pose.hpp"
namespace amalur {
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
