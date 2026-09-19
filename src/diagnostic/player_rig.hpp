#pragma once
#include "../tracking/dodge_facing.hpp"
// Re-Reckoning build 10619381 only. Resolve generation-checked engine handles;
// never retain part pointers across a load. Facing uses the native script service.
namespace player_rig {
inline std::atomic<void*> player{nullptr};
using SetCamera=void(__thiscall*)(void*,void*);
inline SetCamera originalSetCamera{};
using SetFacing=void(__thiscall*)(void*,uint32_t,int);
inline SetFacing nativeFacing{};
inline void __fastcall setCamera(void* self,void*,void* camera){
    originalSetCamera(self,camera);player.store(self);
    log("Player camera assigned: player=%p camera=%p\n",self,camera);
}
inline uintptr_t word(uintptr_t p){return *reinterpret_cast<uintptr_t*>(p);}
inline uintptr_t resolve(uint32_t handle){
    if(!(handle&0x0fff0000))return 0;
    auto manager=word(gameBase+0x015fec38);if(!manager)return 0;
    auto pool=manager+0x2238;auto index=handle&0xffff;
    if(index>=word(pool+0x20)||word(pool+0x20)>65536)return 0;
    auto generations=word(pool+0x1c),objects=word(pool+0xc);
    if(!generations||!objects||word(generations+index*4)!=handle)return 0;
    auto entity=word(objects+index*4);
    return entity&&word(entity+0x38)==handle?entity:0;
}
inline uintptr_t part(uintptr_t entity,unsigned index,uint32_t handle,uintptr_t vtableRva){
    if(!entity||!(word(entity+0x10c)&1))return 0;
    auto p=word(entity+0x3c+index*4);
    return p&&word(p)==gameBase+vtableRva&&word(p+0x18)==handle&&word(p+0x1c)==index?p:0;
}
inline bool location(void* camera,mgs5vr::Vec3& position){
    __try {
        auto p=reinterpret_cast<uintptr_t>(player.load());if(!p)return false;
        if(word(p)!=gameBase+0x1359f14&&word(p)!=gameBase+0x1359e94)return false;
        if(word(p+0x108)+8!=reinterpret_cast<uintptr_t>(camera))return false;
        auto handle=static_cast<uint32_t>(word(p+0x1ec));auto entity=resolve(handle);
        auto loc=part(entity,6,handle,0x1355cdc);if(!loc)return false;
        memcpy(&position,reinterpret_cast<void*>(loc+0x24),sizeof(position));
        return std::isfinite(position.x)&&std::isfinite(position.y)&&std::isfinite(position.z);
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline void face(void* camera,mgs5vr::Vec3 forward){
    const auto now=GetTickCount64();
    if(amalur::dodgeFacing.suppress(now)||amalur::locomotionFacing.suppress(now))return;
    if(!nativeFacing||!std::isfinite(forward.x)||!std::isfinite(forward.y)||forward.x*forward.x+forward.y*forward.y<.01f)return;
    __try {
        mgs5vr::Vec3 position;if(!location(camera,position))return;
        auto p=reinterpret_cast<uintptr_t>(player.load());
        auto owner=static_cast<uint32_t>(word(p+0x1ec)),entity=resolve(owner);
        auto loc=part(entity,6,owner,0x1355cdc),motion=part(entity,42,owner,0x13561e4);
        if(!loc||!motion||!(word(loc+0x20)&1)||!(word(motion+0x20)&1))return;
        // Native set_facing's runtime basis is clockwise from world +X,
        // opposite the atan2 convention used by our camera vectors.
        // PartLocation+b0 is an unsigned full-turn angle; PartMotion receives
        // the requested delta and performs the actual simulation rotation.
        int degrees=static_cast<int>(std::lround(-std::atan2(forward.y,forward.x)*57.2957795131f));
        if(degrees<0)degrees+=360;
        float current=static_cast<float>(static_cast<double>(static_cast<uint32_t>(word(loc+0xb0)))*(360.0/4294967296.0));
        if(std::abs(std::remainder(static_cast<float>(degrees)-current,360.f))<1.5f)return;
        static unsigned lastFrame=~0u;auto frame=presents.load();if(lastFrame==frame)return;lastFrame=frame;
        nativeFacing(nullptr,owner,degrees);
        static unsigned logged=0;if(logged++<8)log("Head facing: native=%.2f requested=%d\n",current,degrees);
    } __except(EXCEPTION_EXECUTE_HANDLER){nativeFacing=nullptr;log("Native head facing disabled after invalid state\n");}
}
inline void install(){
    auto target=reinterpret_cast<unsigned char*>(gameBase+0x9c6670);
    const unsigned char expected[]={0x83,0xec,0x28,0x53,0x57,0x8b,0x7c,0x24,0x34,0x8b,0xd9};
    if(memcmp(target,expected,sizeof(expected))){log("Player camera signature mismatch; first-person unavailable\n");return;}
    hook(target,reinterpret_cast<void*>(&setCamera),reinterpret_cast<void**>(&originalSetCamera),"Player camera ownership");
    auto facing=reinterpret_cast<unsigned char*>(gameBase+0xa3afc0);
    const unsigned char prefix[]={0x8b,0x44,0x24,0x04,0x8b,0x0d};
    const unsigned char tail[]={0x83,0xc4,0x1c,0xc2,0x08,0x00};
    if(!memcmp(facing,prefix,sizeof(prefix))&&word(reinterpret_cast<uintptr_t>(facing)+6)==gameBase+0x15fec38
       &&!memcmp(facing+0x11a,tail,sizeof(tail)))nativeFacing=reinterpret_cast<SetFacing>(facing);
    log("Native head facing %s\n",nativeFacing?"available":"signature mismatch");
}
}
