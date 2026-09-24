#pragma once
namespace amalur {
enum class ContactDamageGate { Allowed, NotPhysical, NotReady, GestureMismatch, Consumed, TargetBudget, ContactSpeed };
// Return the FIRST failing predicate, preserving native short-circuit ordering.
// In particular, do not evaluate contact speed on an ineligible/consumed stroke.
template<class Speed> ContactDamageGate contactDamageGate(bool physical,bool ready,bool sameGesture,bool consumed,unsigned targetCount,Speed speed){
 if(!physical)return ContactDamageGate::NotPhysical;
 if(!ready)return ContactDamageGate::NotReady;
 if(!sameGesture)return ContactDamageGate::GestureMismatch;
 if(consumed)return ContactDamageGate::Consumed;
 if(targetCount>=64)return ContactDamageGate::TargetBudget;
 if(!speed())return ContactDamageGate::ContactSpeed;
 return ContactDamageGate::Allowed;
}
}
