#pragma once
#include <cstdint>
namespace amalur {
inline constexpr uint32_t nativeArrowFiredHash=0x005f7bf4;
struct NativeArrowIdentity {uintptr_t entity{},player{};uint32_t owner{};bool valid{};};
inline bool sameNativeArrowOwner(const NativeArrowIdentity& a,const NativeArrowIdentity& b){
 return a.valid&&b.valid&&a.entity&&a.player&&a.owner&&a.entity==b.entity&&a.player==b.player&&a.owner==b.owner;
}
class NativeArrowObservation {
 uint64_t sequence_{};
public:
 uint64_t completed(uint32_t callbackHash,const NativeArrowIdentity& before,const NativeArrowIdentity& after){
  if(callbackHash!=nativeArrowFiredHash||!sameNativeArrowOwner(before,after)||sequence_>=0x7fffffffffffffffULL)return 0;
  // Each call is one confirmed native animation callback, not a polled frame.
  // A later shot reuses its animation resource/hash; it must get a new token.
  return 0x8000000000000000ULL|++sequence_;
 }
};
// Keep forwarding semantics testable: the original runs once even if observation
// rejects its identity; its raw return value is passed through unchanged.
template<class Original,class After,class Notify>
inline uintptr_t observeNativeArrowCall(Original original,After after,Notify notify){
 const auto result=original();notify(after());return result;
}
}
