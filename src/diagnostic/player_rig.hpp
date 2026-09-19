#pragma once
// Re-Reckoning build 10619381 only. Resolve generation-checked engine handles;
// never retain part pointers across a load or write player simulation transforms.
namespace player_rig {
inline std::atomic<void*> player{nullptr};
using SetCamera=void(__thiscall*)(void*,void*);
inline SetCamera originalSetCamera{};
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
inline void install(){
    auto target=reinterpret_cast<unsigned char*>(gameBase+0x9c6670);
    const unsigned char expected[]={0x83,0xec,0x28,0x53,0x57,0x8b,0x7c,0x24,0x34,0x8b,0xd9};
    if(memcmp(target,expected,sizeof(expected))){log("Player camera signature mismatch; first-person unavailable\n");return;}
    hook(target,reinterpret_cast<void*>(&setCamera),reinterpret_cast<void**>(&originalSetCamera),"Player camera ownership");
}
}
