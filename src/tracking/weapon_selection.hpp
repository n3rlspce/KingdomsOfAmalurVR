#pragma once
#include <cstdint>
namespace amalur {
inline bool equippedWeaponIndex(unsigned type,unsigned typeCount,unsigned base,unsigned slots,unsigned selected,unsigned inventoryCount,unsigned& index){
    if(!typeCount||typeCount>64||type>=typeCount||selected>1||slots<2||slots>16||selected>=slots
        ||!inventoryCount||inventoryCount>128||base>=inventoryCount||slots>inventoryCount-base)return false;
    index=base+selected;return true;
}
struct WeaponSelectionRequest {
    uintptr_t player{};uint32_t owner{},session{},slot{},weapon{};
    bool valid()const{return player&&owner&&session&&slot<=1&&weapon;}
    bool operator==(const WeaponSelectionRequest& b)const{return player==b.player&&owner==b.owner&&session==b.session&&slot==b.slot&&weapon==b.weapon;}
};
struct WeaponSelectionLatch {
    WeaponSelectionRequest sent{};
    bool pending(const WeaponSelectionRequest& request)const{return request.valid()&&!(sent==request);}
    void commit(const WeaponSelectionRequest& request){if(request.valid())sent=request;}
};
}
