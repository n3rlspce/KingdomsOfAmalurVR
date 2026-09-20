#pragma once
// Verified build 10619381: CameraRebuild clamps near (core+24) using far
// (core+28), then calls BuildFrustum at RVA 85EE30 before rebuilding projection
// and view-projection. Override here so all native derived data agrees.
namespace near_clip {
using BuildFrustum=void(__thiscall*)(void*);
static BuildFrustum native{};
static thread_local void* requested{};
struct Lease {unsigned char* core{};float original{},applied{};};
static thread_local Lease lease;
static void restore(unsigned char* core){
    if(lease.core!=core)return;
    auto& value=*reinterpret_cast<float*>(core+0x24);
    if(value==lease.applied){value=lease.original;core[0x35e]|=1;}
    lease={};
}
static void __fastcall onFrustum(void* camera,void*){
    if(camera==requested){
        auto core=static_cast<unsigned char*>(camera);
        auto& nearDistance=*reinterpret_cast<float*>(core+0x24);
        const float farDistance=*reinterpret_cast<float*>(core+0x28);
        if(std::isfinite(nearDistance)&&nearDistance>2.f&&std::isfinite(farDistance)&&farDistance>nearDistance){
            lease={core,nearDistance,2.f};nearDistance=2.f;
            static unsigned reports=0;if(reports++<3)log("First-person near clip: %g -> 2 game units; far=%g\n",lease.original,farDistance);
        }
    }
    native(camera);
}
struct Scope {
    void* previous;
    Scope(void* camera,bool enabled):previous(requested){requested=enabled?camera:nullptr;}
    ~Scope(){requested=previous;}
};
static void install(){
    auto target=reinterpret_cast<unsigned char*>(gameBase+0x85ee30);
    const unsigned char signature[]{0x83,0xec,0x1c,0x0f,0xb7,0x81,0x22,0x03,0,0};
    if(memcmp(target,signature,sizeof(signature))){log("Near clip signature mismatch; skipped\n");return;}
    hook(target,reinterpret_cast<void*>(&onFrustum),reinterpret_cast<void**>(&native),"First-person near clip");
}
}
