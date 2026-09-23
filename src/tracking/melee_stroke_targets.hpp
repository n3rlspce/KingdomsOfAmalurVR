#pragma once
#include <cstdint>
namespace amalur {
// A synchronous resolver attempt claims an actor for this hand/stroke. Claim
// before native callbacks, even when native damage rejects it: no repeated
// retries against invulnerable actors and no duplicate segment/AoE delivery.
struct MeleeStrokeTargets {
    uint32_t actors[64]{};unsigned count{};
    bool contains(uint32_t actor)const{for(unsigned i=0;i<count;++i)if(actors[i]==actor)return true;return false;}
    bool available(uint32_t actor)const{return actor&&!contains(actor)&&count<64;}
    bool claim(uint32_t actor){if(!available(actor))return false;actors[count++]=actor;return true;}
};
}
