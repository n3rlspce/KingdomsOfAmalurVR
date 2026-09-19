#pragma once
// Read-only game-clock pause state for Re-Reckoning build 10619381.
// The native is_game_paused binding (RVA A5EA30) follows GameWin+40C,
// SceneWin+418, then clock manager 15FB228; getter RVA 677C30 reads bit 0
// of clock+4A. The manager's CURRENT index is the menu clock while paused,
// so reading that index would incorrectly report gameplay as unpaused.
namespace game_pause {
inline int sample(bool gameplayActive) {
    if(!gameplayActive||!gameBase)return -1;
    __try {
        constexpr unsigned char getter[]={0x8b,0x54,0x24,0x04,0x32,0xc0,0x85,0xd2,
            0x78,0x10,0x3b,0x51,0x0c,0x7d,0x0b,0x8b,0x41,0x08,0x8b,0x0c,0x90,
            0x8a,0x41,0x4a,0x24,0x01,0xc2,0x04,0x00};
        if(memcmp(reinterpret_cast<const void*>(gameBase+0x677c30),getter,sizeof(getter)))return -1;
        const auto globals=player_rig::word(gameBase+0x15fe9c4);
        if(!globals)return -1;
        const auto windows=globals+0x397c;
        if(player_rig::word(windows)!=gameBase+0x134e60c||player_rig::word(windows+8)!=1)return -1;
        const auto entries=player_rig::word(windows+4);
        if(!entries)return -1;
        const auto game=player_rig::word(entries+4);
        if(!game)return -1;
        const auto gameVtable=player_rig::word(game);
        if(gameVtable!=gameBase+0x1328914&&gameVtable!=gameBase+0x132ae7c)return -1;
        const auto scene=player_rig::word(game+0x40c);
        if(!scene||player_rig::word(scene)!=gameBase+0x1326e9c)return -1;
        const auto index=player_rig::word(scene+0x418);
        const auto manager=player_rig::word(gameBase+0x15fb228);
        if(!manager)return -1;
        const auto count=player_rig::word(manager+0xc),clocks=player_rig::word(manager+8);
        if(!clocks||count>64||index>=count)return -1;
        const auto clock=player_rig::word(clocks+index*4);
        if(!clock||player_rig::word(clock)!=gameBase+0x132f100)return -1;
        return *reinterpret_cast<const unsigned char*>(clock+0x4a)&1;
    } __except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
}
