#include "melee_hit_identity.hpp"
#include <cstdio>
static_assert(amalur::meleeHitActor(0x126d0005)==0x026d0005);
static_assert(amalur::meleeHitActor(0x226d0005)==0x026d0005);
static_assert(amalur::meleeHitActor(0x026d0005)==0);
static_assert(amalur::meleeHitActor(0x326d0005)==0);
static_assert(amalur::meleeHitActor(0xffffffff)==0);
int main(){std::puts("PASS: native hit kind decoding preserves actor generation and rejects non-actor types");}
