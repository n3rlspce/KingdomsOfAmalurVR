#pragma once
#include "../tracking/dodge_facing.hpp"
#include "../tracking/fine_facing.hpp"
#include "staff_aim.hpp"
// Re-Reckoning build 10619381 only. Resolve generation-checked engine handles;
// never retain part pointers across a load. Facing uses the native script service.
namespace player_rig {
inline std::atomic<void*> player{nullptr};
using SetCamera=void(__thiscall*)(void*,void*);
inline SetCamera originalSetCamera{};
using SetFacing=void(__thiscall*)(void*,uint32_t,int);
inline SetFacing nativeFacing{};
// RVA b4fac0 takes pointers to yaw/pitch binary-angle deltas (ret8).
// Same native accumulator and activation path used by script set_facing.
using FineFacing=void(__thiscall*)(void*,const uint32_t*,const uint32_t*);
inline FineFacing nativeFineFacing{};
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
    if(amalur::playMode.nativeBody())return;
    const auto now=GetTickCount64();
    if(amalur::dodgeFacing.suppress(now))return;
    if(!nativeFineFacing||!std::isfinite(forward.x)||!std::isfinite(forward.y)||forward.x*forward.x+forward.y*forward.y<.01f)return;
    __try {
        mgs5vr::Vec3 position;if(!location(camera,position))return;
        auto p=reinterpret_cast<uintptr_t>(player.load());
        auto owner=static_cast<uint32_t>(word(p+0x1ec)),entity=resolve(owner);
        const bool staffAiming=staff_aim::allowed(owner,now);
        if(amalur::locomotionFacing.suppress(now)&&!staffAiming)return;
        auto loc=part(entity,6,owner,0x1355cdc),motion=part(entity,42,owner,0x13561e4);
        if(!loc||!motion||!(word(loc+0x20)&1)||!(word(motion+0x20)&1))return;
        const auto current=static_cast<uint32_t>(word(loc+0xb0));
        uint32_t delta{},zero{};
        if(!amalur::fineFacingDelta(forward.x,forward.y,current,delta))return;
        static unsigned lastFrame=~0u;auto frame=presents.load();if(lastFrame==frame)return;lastFrame=frame;
        nativeFineFacing(reinterpret_cast<void*>(motion),&delta,&zero);
        static uint64_t lastStaffLog{};
        if(staffAiming&&now-lastStaffLog>=1000){lastStaffLog=now;
            log("VR staff attack head facing tick=%llu current=%.3f requested=%.3f moving=%u\n",now,
                double(current)*(360.0/4294967296.0),double(uint32_t(current+delta))*(360.0/4294967296.0),
                unsigned(amalur::locomotionFacing.suppress(now)));}
        static unsigned logged=0;if(logged++<8)log("Fine head facing: current=%.6f requested=%.6f deltaBits=%08x\n",
            double(current)*(360.0/4294967296.0),double(uint32_t(current+delta))*(360.0/4294967296.0),delta);
    } __except(EXCEPTION_EXECUTE_HANDLER){nativeFineFacing=nullptr;log("Native head facing disabled after invalid state\n");}
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
    auto fine=reinterpret_cast<unsigned char*>(gameBase+0xb4fac0);
    const unsigned char finePrefix[]={0x83,0xec,0x08,0x8b,0x44,0x24,0x0c,0x56,0x8b,0xf1,0x50,0x8d,0x4e,0x30,0xe8};
    const unsigned char fineTail[]={0x83,0xc4,0x08,0xc2,0x08,0x00};
    const auto yawCall=gameBase+0xb4fad3+*reinterpret_cast<const int32_t*>(fine+0xf);
    const auto pitchCall=gameBase+0xb4fae0+*reinterpret_cast<const int32_t*>(fine+0x1c);
    if(nativeFacing&&!memcmp(fine,finePrefix,sizeof(finePrefix))&&!memcmp(fine+0x56,fineTail,sizeof(fineTail))
        &&yawCall==gameBase+0x6b8e50&&pitchCall==gameBase+0x6b8e50)
        nativeFineFacing=reinterpret_cast<FineFacing>(fine);
    log("Fine native head facing %s (binary-angle delta, no degree rounding/deadband)\n",nativeFineFacing?"available":"signature mismatch");
}
}
