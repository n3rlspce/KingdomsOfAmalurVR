#include "contact_damage_gate.hpp"
#include <cassert>
using amalur::ContactDamageGate;
int main(){
 for(unsigned bits=0;bits<64;++bits){
  bool physical=bits&1,ready=bits&2,same=bits&4,consumed=bits&8,budget=bits&16,speed=bits&32;
  unsigned calls=0;auto actual=amalur::contactDamageGate(physical,ready,same,consumed,budget?64:63,[&]{++calls;return speed;});
  const bool old=physical&&ready&&same&&!consumed&&!budget&&speed;
  assert((actual==ContactDamageGate::Allowed)==old);
  assert(calls==unsigned(physical&&ready&&same&&!consumed&&!budget));
  const auto reason=!physical?ContactDamageGate::NotPhysical:!ready?ContactDamageGate::NotReady:!same?ContactDamageGate::GestureMismatch:consumed?ContactDamageGate::Consumed:budget?ContactDamageGate::TargetBudget:!speed?ContactDamageGate::ContactSpeed:ContactDamageGate::Allowed;
  assert(actual==reason);
 }
}
