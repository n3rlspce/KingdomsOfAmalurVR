#pragma once
#include "../tracking/weapon_selection.hpp"
#include "../tracking/native_weapon_activation.hpp"
#include "../tracking/physical_melee_recipe.hpp"
// Invoke only from the verified local game-update hook. No synthetic attack or
// renderer-side Fab substitution: the game's idle-switch service owns the swap.
namespace weapon_selection {
inline amalur::WeaponSelectionLatch requests;
inline bool disabled{};
inline void reportSelection(const char* reason,uint32_t selected=~0u,uint32_t weapon=0){
    static const char* previous{};static uint32_t previousSlot=~0u,previousWeapon{},records{};static uint64_t last{};
    const auto now=GetTickCount64();
    if(records>=96||(previous==reason&&previousSlot==selected&&previousWeapon==weapon&&now-last<5000))return;
    previous=reason;previousSlot=selected;previousWeapon=weapon;last=now;++records;
    log("VR inventory selection tick=%llu reason=%s slot=%u weapon=%08x\n",now,reason,selected,weapon);
}
inline bool bindings(){
    static int checked{};if(checked)return checked>0;
    const auto lookup=reinterpret_cast<const unsigned char*>(gameBase+0x9c49a0);
    const auto idle=reinterpret_cast<const unsigned char*>(gameBase+0x9d1770);
    const unsigned char lookupPrefix[]{0x83,0xec,0x10,0x8b,0x15};
    const unsigned char idlePrefix[]{0x83,0xec,0x14,0x8b,0x44,0x24,0x18,0x56,0x57,0x6a,0x0a,0x8b,0xf1,0xbf};
    checked=!memcmp(lookup,lookupPrefix,sizeof(lookupPrefix))&&player_rig::word(gameBase+0x9c49a5)==gameBase+0x15fd08c
        &&!memcmp(idle,idlePrefix,sizeof(idlePrefix))&&player_rig::word(gameBase+0x9d177e)==gameBase+0x1341aac?1:-1;
    log("VR weapon selection native idle-switch bindings=%s\n",checked>0?"verified":"rejected");return checked>0;
}
inline uint32_t equipped(uintptr_t player,unsigned selected,const char*& reason){
    reason="inventory-validation-failed";
    const auto owner=player_rig::word(player+0x1ec),entity=player_rig::resolve(owner);
    // PID13464 read-only validation: part9 vtable1356DA4, Weapon type7/base8,
    // slot0=00910090 (sword5457), slot1=01190148 (bow1423). Type is NOT hardcoded.
    const auto inventory=player_rig::part(entity,9,owner,0x1356da4);if(!inventory||!(player_rig::word(inventory+0x20)&1))return 0;
    const auto globals=player_rig::word(gameBase+0x15fe9c4);if(!globals)return 0;
    const auto registry=globals+0x2ee4,typeCount=player_rig::word(registry+8);
    if(!typeCount||typeCount>64)return 0;
    // C2654E constructs "WEAPON" with hashes17F4E7/1A734F.6F4D70 stores
    // argument3=1A734F at string+4; C2657A passes that exact hash to9C49A0.
    const uint32_t weaponHash=0x1a734f;
    using Lookup=int(__thiscall*)(void*,const uint32_t*);
    const auto type=reinterpret_cast<Lookup>(gameBase+0x9c49a0)(reinterpret_cast<void*>(registry),&weaponHash);
    if(type<0||unsigned(type)>=typeCount)return 0;
    const auto bases=player_rig::word(registry+4),sizes=player_rig::word(registry+0x14);
    const auto handles=player_rig::word(inventory+0x34),inventoryCount=player_rig::word(inventory+0x38);
    if(!bases||!sizes||!handles)return 0;unsigned index{};
    if(!amalur::equippedWeaponIndex(unsigned(type),typeCount,player_rig::word(bases+type*4),player_rig::word(sizes+type*4),selected,inventoryCount,index))return 0;
    const auto weapon=player_rig::word(handles+index*4);
    if(!weapon){reason="inventory-slot-empty";return 0;}
    const auto itemEntity=player_rig::resolve(weapon);
    if(!player_rig::part(itemEntity,11,weapon,0x135745c))return 0;
    // Idle-switch repeats inventory ownership validation; check it before entry.
    const auto item=player_rig::word(itemEntity+0x3c+10*4);
    if(!item||player_rig::word(item+0x18)!=weapon||player_rig::word(item+0x1c)!=10
        ||!(player_rig::word(item+0x20)&1)||player_rig::word(item+0x8c)!=owner)return 0;
    reason="inventory-item-verified";return weapon;
}
inline uint32_t equipped(uintptr_t player,unsigned selected){
    const char* reason{};return equipped(player,selected,reason);
}
inline bool activationBindings(){
    static int checked{};if(checked)return checked>0;
    const auto fn=reinterpret_cast<const unsigned char*>(gameBase+0xc11bd0);
    const unsigned char entry[]{0x83,0xec,0x08,0x8b,0x44,0x24,0x0c,0x56,0x8b,0xf1,0x89,0x86,0x50,0x03,0x00,0x00};
    const unsigned char idleBranch[]{0x83,0xbe,0xdc,0x02,0x00,0x00,0x00,0x74,0x14,0xc7,0x86,0xcc,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0xc7,0x86,0xdc,0x02,0x00,0x00,0x00,0x00,0x00,0x00};
    const unsigned char end[]{0x5e,0x83,0xc4,0x08,0xc2,0x04,0x00};
    checked=!memcmp(fn,entry,sizeof(entry))&&!memcmp(fn+0x4c,idleBranch,sizeof(idleBranch))
        &&!memcmp(fn+0x7a,end,sizeof(end))&&fn[0x47]==0xe8
        &&gameBase+0xc11c1c+*reinterpret_cast<const int32_t*>(fn+0x48)==gameBase+0xb8ab90
        &&player_rig::word(reinterpret_cast<uintptr_t>(fn)+0x21)==gameBase+0x1341aac
        &&player_rig::word(reinterpret_cast<uintptr_t>(fn)+0x32)==gameBase+0x1341b64?1:-1;
    log("VR native weapon activation bindings=%s\n",checked>0?"verified":"rejected");return checked>0;
}
inline void activateSelected(uintptr_t player,uintptr_t physics,uint32_t owner,uint32_t weapon,const amalur::MotionInputPacket& input){
    static amalur::NativeWeaponActivationLimiter limiter;
    if(!weapon||!activationBindings())return;
    const auto now=GetTickCount64();
    // Locate the selected captured melee model without requiring a prior held
    // pose or an attack animation. Inventory membership was verified above.
    const auto root=weapon_control::currentWeaponRoot();if(!root)return;
    const auto count=player_rig::word(root+0x28),children=player_rig::word(root+0x24);
    if(!children||count>32)return;
    bool supported=false;
    for(unsigned i=0;i<count;++i){const auto object=weapon_control::fab(player_rig::word(children+i*4));
        if(object&&player_rig::word(object+0xf8)==weapon
            &&amalur::physicalMeleeRecipe(player_rig::word(object+0xf0)).attack
            &&weapon_control::authoritativeSelectedWeapon(object)){supported=true;break;}}
    if(!supported)return;
    amalur::NativeWeaponActivationState state{};
    state.player=player;state.owner=owner;state.weapon=weapon;state.session=input.session;state.selection=input.selectedWeapon;
    state.nativePrimary=player_rig::word(player+0x350);state.overrideWeapon=player_rig::word(player+0x354);
    state.pendingWeapon=player_rig::word(player+0x200);state.cachedKeyCount=player_rig::word(player+0x2cc);
    state.cachedWeaponCount=player_rig::word(player+0x2dc);state.windows=player_rig::word(physics+0x2e4);
    // Our idle-switch request can remain pending after the visual switch and
    // even after a native attack. It is not an active transition by itself.
    state.pendingOwned=amalur::ownsPendingNativeWeaponActivation(state,requests.sent);
    state.contextValid=input.active&&arm_rig::enabled.load()&&!amalur::bodyDebug.enabled(amalur::nativeArms)
        &&!weapon_control::weaponSheathed.load()&&!weapon_control::backGripClaimed.load()
        &&!motion_controls::explicitSpellActive(now);
    state.inputBusy=motion_controls::nativeAttackHeld(now)||(input.buttons&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B|XINPUT_GAMEPAD_X|XINPUT_GAMEPAD_Y))!=0;
    if(state.nativePrimary!=weapon){
        static uint64_t reported{};if(now>=reported+2000){reported=now;
            log("VR native activation state tick=%llu weapon=%08x native=%08x pending=%08x owned=%u override=%08x cachedKeys=%u cachedWeapons=%u windows=%u context=%u inputBusy=%u eligible=%u attempts=%u\n",
                now,weapon,state.nativePrimary,state.pendingWeapon,unsigned(state.pendingOwned),state.overrideWeapon,
                state.cachedKeyCount,state.cachedWeaponCount,state.windows,unsigned(state.contextValid),unsigned(state.inputBusy),
                unsigned(amalur::needsNativeWeaponActivation(state)),limiter.attempts());}
    }
    if(!limiter.claim(state,now))return;
    // C11BD0 is the game's verified-handle weapon switch, also reached from
    // PLAYER.switch_to_weapon. It invalidates its own cached weapon lookup lists.
    // No synthetic attack, global field patch, or animation request is involved.
    if(equipped(player,input.selectedWeapon)!=weapon)return;
    using Activate=void(__thiscall*)(void*,uint32_t);
    reinterpret_cast<Activate>(gameBase+0xc11bd0)(reinterpret_cast<void*>(player),weapon);
    if(player_rig::word(player+0x350)!=weapon||player_rig::word(player+0x354)
        ||player_rig::word(physics+0x2e4)){
        disabled=true;log("VR native weapon activation postcondition failed; selection service disabled\n");return;
    }
    log("VR native weapon activated without attack owner=%08x slot=%u weapon=%08x before=%08x after=%08x windows=%u pending=%08x pendingOwned=%u cachedKeysAfter=%u cachedWeaponsAfter=%u\n",
        owner,input.selectedWeapon,weapon,state.nativePrimary,player_rig::word(player+0x350),
        player_rig::word(physics+0x2e4),state.pendingWeapon,unsigned(state.pendingOwned),
        player_rig::word(player+0x2cc),player_rig::word(player+0x2dc));
}
inline void sample(uintptr_t physics){
    if(disabled)return;
    __try{
        if(!melee_probe::local(physics))return;
        if(!headTracking.load()||!firstPerson.load()||!trackedCameraAvailable.load()
            ||interfaceView.load()||!motion_controls::gameFocused()||motion_controls::dialogueActive.load()||game_pause::sample(true)!=0){reportSelection("gameplay-inactive");return;}
        amalur::MotionInputPacket input;bool read=false;
        AcquireSRWLockExclusive(&motion_controls::lock);
        read=motion_controls::channel.open(false)&&motion_controls::channel.read(input);
        ReleaseSRWLockExclusive(&motion_controls::lock);
        if(!read||!input.tick||!amalur::validMotionInput(input,GetTickCount64())){reportSelection("input-stale");return;}
        if(!bindings()){reportSelection("native-bindings-rejected");return;}
        const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!player||player_rig::word(player)!=gameBase+0x1359f14){reportSelection("player-type-rejected",input.selectedWeapon);return;}
        const auto owner=player_rig::word(player+0x1ec);
        if(owner!=player_rig::word(physics+0x18)){reportSelection("player-owner-mismatch",input.selectedWeapon);return;}
        const char* selectionReason{};
        const auto weapon=equipped(player,input.selectedWeapon,selectionReason);
        reportSelection(selectionReason,input.selectedWeapon,weapon);
        // Refresh inventory identity independently of native render/remap activity.
        // Hidden attachments may stop evaluating and cannot identify themselves
        // through a fresh visual publication until visibility has recovered.
        weapon_control::publishSelectedWeapon(player,owner,weapon,input.selectedWeapon,input.session,GetTickCount64());
        activateSelected(player,physics,owner,weapon,input);
        if(disabled||motion_controls::nativeAttackHeld(GetTickCount64()))return;
        const amalur::WeaponSelectionRequest request{player,owner,input.session,input.selectedWeapon,weapon};
        if(!requests.pending(request))return;
        // Commit before invoking: reentrancy cannot duplicate a request. Native
        // pending+200 owns deferral until idle; never retry it every render frame.
        requests.commit(request);
        using SwitchIdle=void(__thiscall*)(void*,uint32_t);
        reinterpret_cast<SwitchIdle>(gameBase+0x9d1770)(reinterpret_cast<void*>(player),weapon);
        log("VR weapon selection idle-switch requested owner=%08x slot=%u weapon=%08x session=%u\n",owner,input.selectedWeapon,weapon,input.session);
    }__except(EXCEPTION_EXECUTE_HANDLER){disabled=true;log("VR weapon selection disabled after native validation/call exception\n");}
}
}
