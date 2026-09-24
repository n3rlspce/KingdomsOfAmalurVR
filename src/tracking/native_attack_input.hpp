#pragma once
#include <atomic>
#include <cstdint>
namespace amalur {
// Observe the final delivered pad, not raw bridge intent. Native spell modifier
// owns RT+face. Only a fresh, still-held weapon button blocks physical contact;
// native action lifetime after release remains the native-window guard's job.
class NativeAttackInput {
 std::atomic<std::uint64_t> heldTick_{0};
public:
 void observe(bool deliveredValid,bool gameplay,std::uint32_t buttons,
              unsigned rightTrigger,std::uint64_t now){
  const bool held=deliveredValid&&gameplay&&(buttons&0xc000u)&&rightTrigger<=30;
  heldTick_.store(held?now:0,std::memory_order_release);
 }
 bool held(std::uint64_t now,bool gameplay)const{
  const auto tick=heldTick_.load(std::memory_order_acquire);
  return gameplay&&tick&&tick<=now&&now-tick<100;
 }
};
}
