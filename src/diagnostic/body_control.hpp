#pragma once
#include "native_finisher_state.hpp"
// Read-only build 10619381 native ownership. Never changes the user's play mode.
namespace body_control {
inline bool cinematic(){
    __try {
        if(!gameBase)return false;
        const auto g=player_rig::word(gameBase+0x15fe9c4);if(!g)return false;
        if(player_rig::word(g+0x1080)==gameBase+0x13485ec &&
           (*reinterpret_cast<const unsigned char*>(g+0x1598)&1))return true;
        // CAMERA.is_cinematic_camera_active (A419A0), including pre-player scenes.
        return player_rig::word(g+0x15a0)==gameBase+0x13483cc &&
            (static_cast<int32_t>(player_rig::word(g+0x165c))>=2 ||
             static_cast<int32_t>(player_rig::word(g+0x1660))>=2);
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool suspended(){
    __try {
        if(!gameBase||cinematic()||native_finisher_state::read().nativeSequence||game_pause::sample(true)!=0)return true;
        const auto word=player_rig::word;
        const auto g=word(gameBase+0x15fe9c4);if(!g)return true;
        const auto windows=g+0x397c;
        if(word(windows)!=gameBase+0x134e60c||word(windows+8)!=1)return true;
        const auto entries=word(windows+4);if(!entries)return true;
        const auto game=word(entries+4);if(!game)return true;
        if(word(game)!=gameBase+0x1328914&&word(game)!=gameBase+0x132ae7c)return true;
        const auto scene=word(game+0x40c);
        if(!scene||word(scene)!=gameBase+0x1326e9c)return true;
        const auto p=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!p||(word(p)!=gameBase+0x1359f14&&word(p)!=gameBase+0x1359e94))return true;
        const auto camera=word(scene+0x410);
        if(!camera||word(camera)!=gameBase+0x1335d08||camera!=word(p+0x108))return true;
        if(!player_rig::resolve(static_cast<uint32_t>(word(p+0x1ec))))return true;
        const auto entities=word(gameBase+0x15fec38);if(!entities)return true;
        const auto dialogs=entities+0x2df8;
        if(word(dialogs)==gameBase+0x134e074){
            const auto dialog=word(dialogs+0x148);
            if(dialog&&word(dialog)==gameBase+0x1343210 && !(word(dialog+0x6c)&0x11))return true;
        }
        // INPUT.get_block_input A26E50 selects the first available device type.
        // Native getter 75DB20: flags at +92, temporary override at +BC.
        const auto input=word(gameBase+0x15fd5e4);if(!input)return true;
        const auto list=input+0x164c,count=word(list+8),items=word(list+4);
        if(!items||!count||count>64)return true;
        for(unsigned type=0;type<5;++type)for(unsigned i=0;i<count;++i){
            const auto device=word(items+i*4);
            if(device&&word(device+0x100)==type)
                return *reinterpret_cast<const unsigned char*>(device+0x92)!=0 &&
                       *reinterpret_cast<const unsigned char*>(device+0xbc)==0;
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return true;}
}
}
