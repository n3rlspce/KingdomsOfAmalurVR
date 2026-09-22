#pragma once
// Only invoke on the verified local player's post-update thread. This requests
// a native animation; true does not mean the animation finished or was audible.
namespace back_sheath_native {
inline bool disabled{};
inline bool bindings(){
    static int checked{};if(checked)return checked>0;
    const auto address=gameBase+0xa555a0;
    const unsigned char prefix[]{0x83,0xec,0x24,0x8b,0x44,0x24,0x28,0x6a,0x04,0x50,0x8d,0x4c,0x24,0x08,0xc7,0x44,0x24,0x08};
    const unsigned char tail[]{0x83,0xc4,0x24,0xc2,0x14,0x00};
    checked=!memcmp(reinterpret_cast<const void*>(address),prefix,sizeof(prefix))
        &&player_rig::word(address+0x12)==gameBase+0x1341aac
        &&!memcmp(reinterpret_cast<const void*>(address+0xc0),tail,sizeof(tail))?1:-1;
    log("VR back sheathe native animation bindings=%s\n",checked>0?"verified":"rejected");
    return checked>0;
}
inline bool start(uint32_t owner,bool draw){
    // No verified standalone draw animation appears in the extracted player
    // scripts. Do not invent Weapon_Unsheath or replay an attack to draw.
    if(disabled||draw)return false;
    __try{
        if(!bindings())return false;
        const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!player||player_rig::word(player)!=gameBase+0x1359f14
            ||player_rig::word(player+0x1ec)!=owner)return false;
        const auto entity=player_rig::resolve(owner);
        if(!entity||!(player_rig::word(entity+0x10c)&1)||(player_rig::word(entity+0x10c)&0x200))return false;
        const auto animation=player_rig::word(entity+0x3c+4*4);
        if(!animation||player_rig::word(animation+0x18)!=owner
            ||player_rig::word(animation+0x1c)!=4||!(player_rig::word(animation+0x20)&1))return false;
        // Extracted639271.lua_bxml, Secondary_StartSheath (live script990):
        // on_effect_begin(actor,...): ACTOR.start_anim(actor,"Weapon_Sheath",false,false,100).
        // Native wrapper A85FF0 -> A555A0 -> Part4 B49BF0 owns animation events,
        // attachment changes and native sound dispatch. No raw status/slot writes.
        // 6F14B0 obtains the insensitive hash via6F08B0(text,&hash,0). Reproduced
        // its table1321040 algorithm and verified WEAPON=001A734F before deriving
        // Weapon_Sheath=00960E8B. Service ignores incoming ECX and cleans20bytes.
        const uint32_t animationHash=0x00960e8b;
        using StartAnimation=void(__stdcall*)(uint32_t,const uint32_t*,uint32_t,uint32_t,uint32_t);
        reinterpret_cast<StartAnimation>(gameBase+0xa555a0)(owner,&animationHash,0,0,100);
        log("VR back sheathe native Weapon_Sheath requested owner=%08x hash=%08x\n",owner,animationHash);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){
        disabled=true;log("VR back sheathe native animation disabled after validation/call exception\n");return false;
    }
}
}
