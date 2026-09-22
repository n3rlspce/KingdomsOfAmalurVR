#pragma once
#include <cstdint>
#include "weapon_selection.hpp"
namespace amalur {
struct NativeWeaponActivationState {
    uintptr_t player{};
    uint32_t owner{},weapon{},session{},selection{};
    uint32_t nativePrimary{},overrideWeapon{},pendingWeapon{},cachedKeyCount{},cachedWeaponCount{},windows{};
    bool contextValid{},inputBusy{},pendingOwned{};
    bool validIdentity()const{return player&&owner&&weapon&&session&&selection<=1;}
};
inline bool ownsPendingNativeWeaponActivation(const NativeWeaponActivationState& s,const WeaponSelectionRequest& sent){
    return s.validIdentity()&&s.pendingWeapon==s.weapon&&sent.valid()
        &&sent==WeaponSelectionRequest{s.player,s.owner,s.session,s.selection,s.weapon};
}
inline bool needsNativeWeaponActivation(const NativeWeaponActivationState& s){
    return s.validIdentity()&&s.contextValid&&!s.inputBusy&&s.weapon!=s.nativePrimary
        &&!s.overrideWeapon&&!s.windows
        &&(!s.pendingWeapon||(s.pendingOwned&&s.pendingWeapon==s.weapon));
}
class NativeWeaponActivationLimiter {
    NativeWeaponActivationState identity_{};
    uint64_t lastCall_{};
    unsigned attempts_{};
    bool called_{};
    bool sameIdentity(const NativeWeaponActivationState& s)const{
        return identity_.player==s.player&&identity_.owner==s.owner&&identity_.weapon==s.weapon
            &&identity_.session==s.session&&identity_.selection==s.selection;
    }
public:
    // Call for every observed state, including already matching states. Claim
    // before invoking native code so reentrant sampling cannot duplicate it.
    bool claim(const NativeWeaponActivationState& s,uint64_t now){
        if(!s.validIdentity())return false;
        if(!sameIdentity(s)){identity_=s;attempts_=0;}
        if(s.nativePrimary==s.weapon){attempts_=0;return false;}
        if(!needsNativeWeaponActivation(s)||attempts_>=3)return false;
        // Preserve cooldown across identity changes and successful readback.
        // Clock rollback fails closed rather than wrapping into an immediate retry.
        if(called_&&(now<lastCall_||now-lastCall_<500))return false;
        called_=true;lastCall_=now;++attempts_;return true;
    }
    unsigned attempts()const{return attempts_;}
};
}
