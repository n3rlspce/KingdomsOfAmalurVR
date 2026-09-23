#include "melee_stroke_targets.hpp"
#include <cassert>
int main(){using namespace amalur;
 MeleeStrokeTargets hands[2];assert(!hands[0].claim(0));
 assert(hands[0].claim(7));assert(!hands[0].available(7));
 assert(hands[0].claim(8));assert(!hands[0].claim(7)); // later sphere/frame
 assert(hands[1].claim(7)); // independent hand stroke
 const auto retained=hands[0];hands[0]=retained;
 assert(!hands[0].claim(7)&&!hands[0].claim(8)); // same-stroke lease recreation
 for(unsigned i=9;i<71;++i)assert(hands[0].claim(i));
 assert(hands[0].count==64&&!hands[0].claim(71));
 hands[0]={};assert(hands[0].claim(7));assert(!hands[1].claim(7));
 // Rejected native attempts remain claimed; do not retry against an immune
 // target or repeat effects simply because acceptedTargets was zero.
 assert(hands[0].claim(99));assert(!hands[0].claim(99));
}
