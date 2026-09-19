#pragma once
#include "core.hpp"
namespace mgs5vr {
struct ArmPose { Pose shoulder,elbow,wrist; };
struct ArmSolution { ArmPose pose; bool reachClamped{}; };
struct ArmSurface { Vec3 point,normal; float clearance{}; };
struct ArmBasis { Vec3 upperAxis,forearmAxis; Vec3 elbowBend{0,0,1},wristUp{0,1,0}; };
std::optional<Vec3> outsideArmSurface(Vec3 point,const ArmSurface& surface);
std::optional<Pose> nativeAffinePose(const std::array<float,16>& rowMatrix);
std::optional<ArmSolution> solveArm(const ArmPose& animated,Pose wristTarget,Vec3 bendHint,
                                  const ArmBasis* basis=nullptr,const ArmSurface* elbowSurface=nullptr);
}
