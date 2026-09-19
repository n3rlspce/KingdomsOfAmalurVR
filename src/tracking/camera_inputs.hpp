#pragma once
#include "camera_pose.hpp"
#include <cstring>
namespace amalur {
// Keep camera inputs consistent with derived view matrices until presentation.
// Restore only fields that still contain our values, preserving engine updates.
struct CameraInputs {
    unsigned char* core{};CameraPose original{},applied{};float fov{},appliedFov{};
    void restore(){
        if(!core)return;
        auto field=[&](unsigned offset,const void* before,const void* after,size_t size){
            if(std::memcmp(core+offset,after,size)==0)std::memcpy(core+offset,before,size);
        };
        field(4,&original.eye,&applied.eye,sizeof(Vec3));
        field(0x14,&original.target,&applied.target,sizeof(Vec3));
        field(0x1c0,&original.up,&applied.up,sizeof(Vec3));
        field(0x2c,&fov,&appliedFov,sizeof(float));core=nullptr;
    }
};
}
