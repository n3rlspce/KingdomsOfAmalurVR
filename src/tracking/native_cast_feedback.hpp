#pragma once
#include "melee_lifetime.hpp"
namespace amalur {
struct NativeCastDefinition {uint32_t listeners{},script{},kind{},field1bc{},field1f8{},field1fc{},field200{},field208{},field20c{},scriptBytes{},scriptHash{};};
inline bool capturedNativeCast(uint32_t asset,const NativeCastDefinition& d){
 if(d.listeners!=1||d.field1bc||d.field1fc||d.field200||d.field20c!=0x01000104)return false;
 if(asset==892)return d.script==2434&&d.kind==4&&d.field1f8==0xdac&&d.field208==0xc803&&d.scriptBytes==783&&d.scriptHash==0xef010ecc;
 if(asset==160||asset==161)return d.script==1077&&d.kind==1&&!d.field1f8&&d.field208==0x0100c002&&d.scriptBytes==1174&&d.scriptHash==0x41950419;
 return false;
}
class NativeCastObservation {
 MeleeLifetimeRegistry<128> lifetimes_;
public:
 // Only called after native constructor returns successfully. The reset hook
 // retires old addresses; no timers or per-frame serials impersonate lifetime.
 uint64_t created(uintptr_t address,uint32_t owner,uint32_t localOwner,uint32_t asset,const NativeCastDefinition& d,int result,bool owned,bool identityValid){
  if(!address||!owner||owner!=localOwner||result!=0||owned||!identityValid||!capturedNativeCast(asset,d)||lifetimes_.current(address))return 0;
  return lifetimes_.replace(address);
 }
 void retired(uintptr_t address){const auto serial=lifetimes_.current(address);if(serial)lifetimes_.retire(address,0,serial);}
};
}
