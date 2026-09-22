#include "selected_weapon_proof.hpp"
#include <cassert>
int main(){
    using namespace amalur;
    SelectedWeaponProof p{123,7,9,1,42,1000};
    auto current=[&](const SelectedWeaponProof& q,uint64_t now=1099){return selectedWeaponProofCurrent(q,123,7,1,42,now);};
    assert(current(p));assert(!current(p,1100));assert(!current(p,999));assert(!current({}));
    auto changed=p;changed.owner=8;assert(!current(changed));
    changed=p;changed.player=124;assert(!current(changed));
    changed=p;changed.session=43;assert(!current(changed));
    changed=p;changed.selection=0;assert(!current(changed));
    changed=p;changed.weapon=0;assert(!current(changed));
    // A selection refresh during pose computation must prevent the old pose
    // from being labeled as the new selected weapon (even within freshness).
    changed=p;changed.weapon=10;assert(current(changed));assert(!sameSelectedWeaponProof(p,changed));
    changed=p;changed.tick++;assert(!sameSelectedWeaponProof(p,changed));
    assert(sameSelectedWeaponProof(p,p));
    // Character changes with the same bridge session and player allocation
    // still invalidate proof through the actor owner identity.
    assert(!selectedWeaponProofCurrent(p,123,8,1,42,1099));
}
