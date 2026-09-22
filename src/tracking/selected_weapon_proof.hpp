#pragma once
#include <cstdint>
namespace amalur {
struct SelectedWeaponProof {uintptr_t player{};uint32_t owner{},weapon{},selection{},session{};uint64_t tick{};};
inline bool selectedWeaponProofCurrent(const SelectedWeaponProof& p,uintptr_t player,uint32_t owner,
    uint32_t selection,uint32_t session,uint64_t now){
    return player&&owner&&p.player==player&&p.owner==owner&&p.weapon&&p.selection<=1
        &&p.selection==selection&&p.session&&p.session==session&&p.tick&&p.tick<=now&&now-p.tick<100;
}
inline bool sameSelectedWeaponProof(const SelectedWeaponProof& a,const SelectedWeaponProof& b){
    return a.player==b.player&&a.owner==b.owner&&a.weapon==b.weapon&&a.selection==b.selection
        &&a.session==b.session&&a.tick==b.tick;
}
}
