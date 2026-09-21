#pragma once
#include <cstdint>
namespace amalur {
struct NativeWeaponSlot {uintptr_t object{},root{};uint32_t owner{},rootOwner{};uint64_t tick{};unsigned selection{};bool backSocket{};};
inline bool freshBackSocket(const NativeWeaponSlot& entry,uintptr_t object,uint32_t owner,uintptr_t root,uint32_t rootOwner,unsigned selection,uint64_t now){
 return object&&owner&&root&&rootOwner&&selection<=1&&entry.object==object&&entry.owner==owner&&entry.root==root&&entry.rootOwner==rootOwner
  &&entry.selection==selection&&entry.tick&&entry.tick<=now&&now-entry.tick<100&&entry.backSocket;
}
}
