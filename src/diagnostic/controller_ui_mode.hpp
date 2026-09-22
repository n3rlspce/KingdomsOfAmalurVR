#pragma once
#include "../tracking/controller_ui_policy.hpp"
namespace controller_ui_mode {
static amalur::ControllerUiPolicy policy;
// Native is_gamepad_active's getter (RVA 76E200) tests InputDeviceMgr+1E0==0.
// PC keyboard/mouse events select 2/3. RTTI identifies the singleton's vtable.
// Called under motion_controls::lock, on the game's XInput polling thread.
inline void update(bool active){
    __try {
        if(!gameBase)return;
        const auto getter=reinterpret_cast<const unsigned char*>(gameBase+0x76e200);
        constexpr unsigned char tail[]{0x33,0xc0,0x39,0x81,0xe0,0x01,0,0,0x0f,0x94,0xc0,0xc3};
        if(getter[0]!=0x8b||getter[1]!=0x0d||*reinterpret_cast<const uintptr_t*>(getter+2)!=gameBase+0x15fdf80||memcmp(getter+6,tail,sizeof(tail))){policy.update(0,0,false);return;}
        const auto manager=player_rig::word(gameBase+0x15fdf80);
        if(!manager||player_rig::word(manager)!=gameBase+0x133f534){policy.update(0,0,false);return;}
        auto mode=reinterpret_cast<int*>(manager+0x1e0);
        const int native=*mode,selected=policy.update(manager,native,active);
        if(selected!=native){*mode=selected;static unsigned reports=0;if(reports++<12)log("VR UI input mode: %d -> %d active=%d\n",native,selected,active);}
    } __except(EXCEPTION_EXECUTE_HANDLER){policy.update(0,0,false);}
}
}
