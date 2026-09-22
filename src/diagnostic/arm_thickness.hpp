#pragma once
#include "../tracking/arm_thickness.hpp"
#include "../tracking/arm_thickness_settings.hpp"
namespace arm_thickness {
struct Lease {uintptr_t object{},owner{},buffer{};unsigned count{};bool changed[64]{};amalur::RigBone before[64]{},after[64]{};};
inline Lease leases[32];inline uintptr_t lastRoot{},lastOwner{};
inline void restore(uintptr_t object){
    __try {for(auto& l:leases)if(l.object==object){
        if(player_rig::word(object+0xf8)==l.owner&&player_rig::word(object+0x34)==l.buffer&&player_rig::word(object+0x38)==l.count){
            auto bones=reinterpret_cast<amalur::RigBone*>(l.buffer);
            for(unsigned i=0;i<l.count;++i)if(l.changed[i]&&!memcmp(&bones[i],&l.after[i],48))bones[i]=l.before[i];
        }l={};break;}
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline void apply(uintptr_t object,uintptr_t root,bool solved){
    __try {
        const float factor=amalur::armThickness.get()*.01f;
        if(!solved||factor==1.f||amalur::playMode.normal()||amalur::bodyDebug.enabled(amalur::nativeArms))return;
        // Exact player attachment ownership is checked by prepare; keep weapons out.
        auto owner=player_rig::word(object+0xf8),entity=player_rig::resolve(owner);
        if(player_rig::part(entity,11,owner,0x135745c))return;
        if(!arm_rig::bareBodyPalette(object,root)&&!player_rig::part(entity,12,owner,0x13563e4)&&!player_rig::part(entity,40,owner,0x1356bec))return;
        const uint32_t *ids{},*rootIds{};const int16_t *parents{},*rootParents{};unsigned count{},rootCount{};
        if(!body_visibility::skeleton(object,ids,parents,count)||count>64||!body_visibility::skeleton(root,rootIds,rootParents,rootCount))return;
        const auto rootOwner=player_rig::word(root+0xf8);
        if(lastRoot!=root||lastOwner!=rootOwner){for(auto& l:leases)l={};lastRoot=root;lastOwner=rootOwner;}
        Lease* slot=nullptr;
        for(auto& l:leases)if(!l.object){slot=&l;break;}if(!slot)return;
        auto buffer=player_rig::word(object+0x34);if(!buffer)return;
        Lease next{};next.object=object;next.owner=owner;next.buffer=buffer;next.count=count;
        memcpy(next.before,reinterpret_cast<void*>(buffer),count*48);memcpy(next.after,next.before,count*48);
        bool any=false;
        for(unsigned i=0;i<count;++i)if(amalur::thickenArmBone(next.after[i],ids[i],factor)){next.changed[i]=true;any=true;}
        if(!any)return;*slot=next;
        auto bones=reinterpret_cast<amalur::RigBone*>(buffer);
        for(unsigned i=0;i<count;++i)if(next.changed[i])bones[i]=next.after[i];
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
}
