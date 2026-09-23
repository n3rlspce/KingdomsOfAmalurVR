#pragma once
#include <cstdint>
namespace amalur {
// Match StereoSource's one-second transport lease. A brief game-thread stall
// must not drop only the menu while the same frame's world stays submitted.
inline bool menuImageVisible(bool menu,bool acquired,uint32_t mode,uint64_t tick,uint64_t now){
 return menu&&acquired&&mode==7&&tick&&tick<=now&&now-tick<=1000;
}
}
