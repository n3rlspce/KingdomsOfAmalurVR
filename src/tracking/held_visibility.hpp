#pragma once
#include <cstdint>
namespace amalur {
struct HeldVisibilityIdentity {
    uintptr_t object{},root{},buffer{},mapper{},table{};
    uint32_t owner{},rootOwner{},asset{},selection{};
};
// A stale render timestamp must not deadlock recovery. Revalidate the native
// identity and mapping instead; only a subsequent real remap publishes poses.
inline bool sameHeldVisibilityIdentity(const HeldVisibilityIdentity& a,const HeldVisibilityIdentity& b){
    return a.object&&a.root&&a.buffer&&a.mapper&&a.table&&a.owner&&a.rootOwner&&a.asset&&a.selection<=1
        &&a.object==b.object&&a.root==b.root&&a.buffer==b.buffer&&a.mapper==b.mapper&&a.table==b.table
        &&a.owner==b.owner&&a.rootOwner==b.rootOwner&&a.asset==b.asset&&a.selection==b.selection;
}
}
