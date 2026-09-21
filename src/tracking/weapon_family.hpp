#pragma once
#include <cstdint>
namespace amalur {
// Captured model identities only. This does not bypass per-model skeleton proof.
inline constexpr bool knownLongswordModel(uint32_t model){return model==2478||model==5457;}
}
