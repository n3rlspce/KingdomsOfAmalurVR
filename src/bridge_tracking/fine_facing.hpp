#pragma once
#include <cmath>
#include <cstdint>
namespace amalur {
// The native movement system accumulates unsigned binary-angle deltas:
// one turn is 2^32, and subtraction wraps across north without a long turn.
inline bool fineFacingDelta(double x,double y,uint32_t current,uint32_t& delta){
    delta=0;
    if(!std::isfinite(x)||!std::isfinite(y)||x*x+y*y<.01)return false;
    constexpr double turn=4294967296.0, tau=6.283185307179586476925286766559;
    double angle=std::atan2(y,x);if(angle<0)angle+=tau;
    const uint32_t target=static_cast<uint32_t>(static_cast<uint64_t>(std::llround(angle*(turn/tau))));
    delta=target-current;
    return delta!=0;
}
}
