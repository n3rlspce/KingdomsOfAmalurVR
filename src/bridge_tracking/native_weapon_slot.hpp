#pragma once
#include <cstdint>
namespace amalur {
struct NativeWeaponSlot {uintptr_t object{},root{};uint32_t owner{},rootOwner{};uint64_t tick{};unsigned selection{};bool backSocket{};};
inline bool freshBackSocket(const NativeWeaponSlot& entry,uintptr_t object,uint32_t owner,uintptr_t root,uint32_t rootOwner,unsigned selection,uint64_t now){
 return object&&owner&&root&&rootOwner&&selection<=1&&entry.object==object&&entry.owner==owner&&entry.root==root&&entry.rootOwner==rootOwner
  &&entry.selection==selection&&entry.tick&&entry.tick<=now&&now-entry.tick<100&&entry.backSocket;
}
// An attached weapon is not necessarily the selected weapon. Duplicate child
// entries for one dual-weapon Fab are allowed; any unstowed sibling blocks it.
struct ActiveWeaponSet {
 bool found{},selfStowed{},ambiguous{};
 void observe(bool isSelf,bool provenBack){if(isSelf){found=true;selfStowed|=provenBack;}else if(!provenBack)ambiguous=true;}
 bool accepts()const{return found&&!selfStowed&&!ambiguous;}
};
}
