#pragma once
#include <cstdint>
namespace amalur {
// b9d643..b9d783: native hit owner at record+14 carries a type nibble.
// Only types 1 and 2 decode to actor handles; terrain/other hits resolve to 0.
constexpr uint32_t meleeHitActor(uint32_t encoded){
    auto type=encoded>>28;
    return type==1||type==2?encoded&0x0fffffffu:0;
}
}
