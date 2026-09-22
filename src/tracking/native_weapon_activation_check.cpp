#include "native_weapon_activation.hpp"
#include <cassert>
int main(){
    using namespace amalur;
    NativeWeaponActivationState s{123,7,9,42,0,0,0,0,0,0,0,true,false};
    assert(needsNativeWeaponActivation(s));
    auto changed=s;changed.selection=1;assert(needsNativeWeaponActivation(changed));
    changed=s;changed.nativePrimary=9;assert(!needsNativeWeaponActivation(changed));
    changed=s;changed.overrideWeapon=9;assert(!needsNativeWeaponActivation(changed));
    changed=s;changed.pendingWeapon=9;assert(!needsNativeWeaponActivation(changed));
    WeaponSelectionRequest sent{s.player,s.owner,s.session,s.selection,s.weapon};
    changed.pendingOwned=ownsPendingNativeWeaponActivation(changed,sent);
    assert(changed.pendingOwned&&needsNativeWeaponActivation(changed));
    auto foreign=sent;++foreign.owner;assert(!ownsPendingNativeWeaponActivation(changed,foreign));
    foreign=sent;++foreign.session;assert(!ownsPendingNativeWeaponActivation(changed,foreign));
    foreign=sent;++foreign.player;assert(!ownsPendingNativeWeaponActivation(changed,foreign));
    foreign=sent;++foreign.slot;assert(!ownsPendingNativeWeaponActivation(changed,foreign));
    foreign=sent;++foreign.weapon;assert(!ownsPendingNativeWeaponActivation(changed,foreign));
    auto busy=changed;busy.overrideWeapon=9;assert(!needsNativeWeaponActivation(busy));
    busy=changed;busy.cachedWeaponCount=5;assert(needsNativeWeaponActivation(busy));
    busy=changed;busy.cachedKeyCount=5;assert(needsNativeWeaponActivation(busy));
    busy=changed;busy.windows=1;assert(!needsNativeWeaponActivation(busy));
    busy=changed;busy.inputBusy=true;assert(!needsNativeWeaponActivation(busy));
    changed.pendingWeapon=10;assert(!needsNativeWeaponActivation(changed));
    changed=s;changed.cachedKeyCount=5;assert(needsNativeWeaponActivation(changed));
    changed=s;changed.cachedWeaponCount=5;assert(needsNativeWeaponActivation(changed));
    changed=s;changed.windows=1;assert(!needsNativeWeaponActivation(changed));
    // Caller combines focus, spell, sheath, tracking and gameplay eligibility.
    changed=s;changed.contextValid=false;assert(!needsNativeWeaponActivation(changed));
    changed=s;changed.inputBusy=true;assert(!needsNativeWeaponActivation(changed));
    changed=s;changed.selection=2;assert(!needsNativeWeaponActivation(changed));
    changed=s;changed.session=0;assert(!needsNativeWeaponActivation(changed));
    changed=s;changed.player=0;assert(!needsNativeWeaponActivation(changed));
    changed=s;changed.owner=0;assert(!needsNativeWeaponActivation(changed));
    changed=s;changed.weapon=0;assert(!needsNativeWeaponActivation(changed));
    NativeWeaponActivationLimiter limiter;
    assert(limiter.claim(s,1000));assert(limiter.attempts()==1);
    assert(!limiter.claim(s,1000));assert(!limiter.claim(s,1499));
    assert(limiter.claim(s,1500));assert(limiter.claim(s,2000));
    assert(!limiter.claim(s,2500));assert(!limiter.claim(s,99999));
    changed=s;changed.nativePrimary=s.weapon;
    assert(!limiter.claim(changed,2100));assert(limiter.attempts()==0);
    assert(!limiter.claim(s,2200));assert(limiter.claim(s,2500));
    changed=s;changed.owner=8;
    assert(!limiter.claim(changed,2600));assert(limiter.attempts()==0);
    assert(limiter.claim(changed,3000));assert(!limiter.claim(changed,2999));
    changed.session++;assert(limiter.claim(changed,3500));assert(limiter.attempts()==1);
    changed.weapon++;changed.selection=1;assert(limiter.claim(changed,4000));assert(limiter.attempts()==1);
    changed.player++;assert(limiter.claim(changed,4500));assert(limiter.attempts()==1);
    changed.inputBusy=true;assert(!limiter.claim(changed,5000));assert(limiter.attempts()==1);
    NativeWeaponActivationLimiter zeroClock;
    assert(zeroClock.claim(s,0));assert(!zeroClock.claim(s,499));assert(zeroClock.claim(s,500));
    // Policy has no attack synthesis or native mutation; input remains intact.
    assert(s.nativePrimary==0&&s.cachedKeyCount==0&&s.windows==0);
    // Failed-session regression: sword -> daggers -> sword, with the native
    // idle-switch marker persisting. Each can activate without a trigger.
    NativeWeaponActivationLimiter switching;
    auto equip=s;equip.nativePrimary=0xbb0110;equip.weapon=0xf70133;equip.pendingWeapon=equip.weapon;
    equip.cachedKeyCount=equip.cachedWeaponCount=5; // Native cached-list counts persist while idle.
    sent={equip.player,equip.owner,equip.session,equip.selection,equip.weapon};
    equip.pendingOwned=ownsPendingNativeWeaponActivation(equip,sent);assert(switching.claim(equip,1000));
    equip.nativePrimary=equip.weapon;assert(!switching.claim(equip,1010));
    equip.weapon=0x910090;equip.pendingWeapon=equip.weapon;
    equip.pendingOwned=ownsPendingNativeWeaponActivation(equip,sent);assert(!switching.claim(equip,2000));
    sent.weapon=equip.weapon;equip.pendingOwned=ownsPendingNativeWeaponActivation(equip,sent);
    assert(switching.claim(equip,2000));equip.nativePrimary=equip.weapon;assert(!switching.claim(equip,2010));
}
