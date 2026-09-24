#pragma once
#include <cstdint>
namespace amalur {
// Captured model identities only. This does not bypass per-model skeleton proof.
inline constexpr bool knownLongswordModel(uint32_t model){return model==2478||model==5457;}
// Reckoning hammer5215: same inventory weapon, separately captured skeleton.
inline constexpr bool knownHammerModel(uint32_t model){return model==1323||model==5215;}
}
