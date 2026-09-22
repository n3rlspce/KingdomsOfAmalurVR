#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include "../tracking/melee_retirement.hpp"

// Build 10619381: native sphere sweep and combat resolution. No event pointers,
// animation windows, hooks, or scheduling are owned by this adapter.
namespace melee_native {
// Owned contact is an explicit test-build option; release builds default off.
#if defined(AMALUR_OWNED_MELEE) && AMALUR_OWNED_MELEE
inline constexpr bool customContactEnabled=true;
#else
inline constexpr bool customContactEnabled=false;
#endif
using Reset = void(__thiscall*)(void*);
using Radius = void(__thiscall*)(void*, float, uintptr_t);
using Query = uintptr_t(__thiscall*)(void*, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
using Resolve = void(__thiscall*)(void*, uint32_t, void*, const float*, const float*, int32_t, uint32_t);
using Lookup = uintptr_t(__thiscall*)(void*, uint32_t);
using Index = int32_t(__thiscall*)(void*, uint32_t, int32_t);
using Free = void(__cdecl*)(int, void*);
inline Query queryFunction{};
inline bool ready{};

struct Context {
    uint32_t owner{}, key{}, flags{};
    int32_t talentIndex{-1}, subIndex{-1};
    uintptr_t baseTalent{}, selectedTalent{};
    uint32_t baseAsset{}, selectedAsset{};
};

inline bool signature(uintptr_t rva, const unsigned char* bytes, size_t count) {
    return std::memcmp(reinterpret_cast<void*>(gameBase+rva), bytes, count)==0;
}
inline bool initialize(Query unhookedQuery = nullptr) {
    const unsigned char reset[]{0x53,0x56,0x8b,0xf1,0x57,0x8b,0x7e,0x20};
    const unsigned char radius[]{0x56,0x57,0x8b,0xf1,0xe8,0xa7,0x5a,0xff,0xff};
    const unsigned char query[]{0x55,0x8b,0xec,0x83,0xe4,0xf0,0x83,0xec,0x20};
    const unsigned char resolve[]{0x55,0x8b,0xec,0x83,0xe4,0xf8,0xb8,0x64,0x12,0,0};
    const unsigned char lookup[]{0x56,0x8b,0x71,0x28,0x33,0xc0,0x57};
    const unsigned char index[]{0x53,0x56,0x8b,0x71,0x28,0x83,0xc8,0xff};
    const unsigned char release[]{0x56,0x8b,0x74,0x24,0x0c,0x85,0xf6};
    ready=signature(0x958920,reset,sizeof(reset))&&signature(0x962e70,radius,sizeof(radius))
        &&signature(0xb9d520,resolve,sizeof(resolve))&&signature(0xb414f0,lookup,sizeof(lookup))
        &&signature(0xb41590,index,sizeof(index))&&signature(0x6fe180,release,sizeof(release))
        &&(unhookedQuery||signature(0x958e30,query,sizeof(query)));
    // Caller may supply the verified trampoline when 958e30 is already detoured.
    queryFunction=ready?(unhookedQuery?unhookedQuery:reinterpret_cast<Query>(gameBase+0x958e30)):nullptr;
    return ready;
}

inline uintptr_t component(uint32_t owner,unsigned slot) {
    auto entity=player_rig::resolve(owner);
    if(!entity||!(player_rig::word(entity+0x10c)&1))return 0;
    auto part=player_rig::word(entity+0x3c+slot*4);
    return part&&player_rig::word(part+0x18)==owner&&player_rig::word(part+0x1c)==slot
        &&(player_rig::word(part+0x20)&1)?part:0;
}
// be1f20 reserves slot zero and reuses inactive runtime objects in-place.
// be65b0 sets active/index/owner at +1c/+20/+24; bbfbf0 clears them.
// Identity checks reject inactive or differently-owned reuse, but cannot prove
// a lifetime generation when every field is recycled to the same value.
inline bool runtimeCurrent(uintptr_t table,uint32_t count,int32_t index,
    uintptr_t expected,uint32_t asset,uint32_t owner) {
    if(!table||!expected||!owner||index<=0||count>1048576||uint32_t(index)>=count)return false;
    auto runtime=player_rig::word(table+static_cast<uintptr_t>(index)*4);
    return runtime==expected&&(player_rig::word(runtime+0x1c)&1)
        &&player_rig::word(runtime+0x20)==uint32_t(index)
        &&player_rig::word(runtime+0x24)==owner&&player_rig::word(runtime+4)==asset;
}

enum class RetirementResult {Disabled,Expired,Rejected,Released};
// Only for a runtime independently created and tracked by the owned backend.
// Never use with a borrowed native attack. Requires the native update thread,
// and the owned key's runtime references must already have been detached.
// customContactEnabled gates every mutation.
template<class Registry>
inline RetirementResult retireOwnedRuntime(Registry& lifetimes,uintptr_t runtime,
    uint64_t generation,int32_t index,uint32_t asset,uint32_t owner){
    if(!customContactEnabled)return RetirementResult::Disabled;
    if(!lifetimes.matches(runtime,0,generation))return RetirementResult::Expired;
    const unsigned char release[]{0x8b,0x41,0x04,0x8b,0x4c,0x24,0x04};
    const unsigned char cancel[]{0x8b,0x51,0x18,0x56,0x33,0xc0,0x57};
    if(!ready||!signature(0xbe1eb0,release,sizeof(release))
        ||!signature(0xbbfcb0,cancel,sizeof(cancel)))return RetirementResult::Rejected;
    auto manager=player_rig::word(gameBase+0x15fec38);
    if(!manager||!runtimeCurrent(player_rig::word(manager+0xd0),player_rig::word(manager+0xd4),
        index,runtime,asset,owner))return RetirementResult::Rejected;
    auto pool=manager+0xcc;
    auto plan=amalur::planMeleeRetirement(uint32_t(index),player_rig::word(runtime+0x44),
        player_rig::word(runtime+0x48),
        reinterpret_cast<const amalur::MeleeNotification*>(player_rig::word(pool+0x14)),
        player_rig::word(pool+0x18));
    if(!plan.valid)return RetirementResult::Rejected;
    // Claim once before native effect-complete callbacks can reenter cleanup.
    if(!lifetimes.retire(runtime,0,generation))return RetirementResult::Expired;
    using Cancel=bool(__thiscall*)(void*,uint32_t);
    using ReleaseRuntime=bool(__thiscall*)(void*,int32_t);
    for(unsigned i=0;i<plan.count;++i)
        reinterpret_cast<Cancel>(gameBase+0xbbfcb0)(reinterpret_cast<void*>(pool),plan.ids[i]);
    *reinterpret_cast<uint32_t*>(runtime+0x44)=0xffffffffu;
    *reinterpret_cast<uint32_t*>(runtime+0x48)=0xffffffffu;
    return reinterpret_cast<ReleaseRuntime>(gameBase+0xbe1eb0)(reinterpret_cast<void*>(pool),index)
        ?RetirementResult::Released:RetirementResult::Rejected;
}
// Re-resolve the talent mapping for every swing/contact. Equipment changes,
// loading, or recycled talent entries must invalidate cached metadata.
inline bool current(const Context& c) {
    if(!ready||c.talentIndex<0||c.talentIndex>1048576||c.subIndex<0||c.subIndex>256
        ||!c.baseTalent||!c.selectedTalent)return false;
    auto part=component(c.owner,18);if(!part)return false;
    auto count=player_rig::word(part+0x28);
    if(!count||count>4096||!player_rig::word(part+0x24)||!player_rig::word(part+0x34))return false;
    auto manager=player_rig::word(gameBase+0x15fec38);
    auto table=manager?player_rig::word(manager+0xd0):0;
    auto runtimeCount=manager?player_rig::word(manager+0xd4):0;
    auto baseIndex=reinterpret_cast<Index>(gameBase+0xb41590)(reinterpret_cast<void*>(part),c.key,0);
    if(!runtimeCurrent(table,runtimeCount,baseIndex,c.baseTalent,c.baseAsset,c.owner))return false;
    auto base=reinterpret_cast<Lookup>(gameBase+0xb414f0)(reinterpret_cast<void*>(part),c.key);
    if(base!=c.baseTalent)return false;
    auto index=reinterpret_cast<Index>(gameBase+0xb41590)(reinterpret_cast<void*>(part),c.key,c.subIndex);
    return index==c.talentIndex&&runtimeCurrent(table,runtimeCount,index,c.selectedTalent,c.selectedAsset,c.owner);
}

struct HitArray {
    void* data{};
    uint32_t count{},capacity{};
    int16_t allocator{0x27}, flags{-1};
    HitArray()=default;
    HitArray(const HitArray&)=delete;
    HitArray& operator=(const HitArray&)=delete;
    ~HitArray() {clear();}
    void clear() {
        if(!data)return;
        auto allocation=data;data=nullptr;
        // ba48b3-ba48ec destroys ALL allocated records, not just count.
        auto record=reinterpret_cast<unsigned char*>(allocation);
        for(uint32_t i=0;i<capacity;++i,record+=0x70) {
            auto vtable=*reinterpret_cast<uintptr_t*>(record);
            reinterpret_cast<void(__thiscall*)(void*,unsigned)>(*reinterpret_cast<uintptr_t*>(vtable))(record,0);
        }
        reinterpret_cast<Free>(gameBase+0x6fe180)(allocator,allocation);
        count=capacity=0;
    }
};
static_assert(sizeof(HitArray)==16,"Native x86 hit vector layout");

inline bool gather(uintptr_t physics,uint32_t owner,const float* from,const float* to,float radius,HitArray& hits,bool* completed=nullptr){
    if(completed)*completed=false;
    if constexpr(!customContactEnabled)return false;
    if(!ready||!queryFunction||!from||!to||hits.data
        ||!std::isfinite(radius)||radius<=0||radius>20||component(owner,15)!=physics)return false;
    for(unsigned i=0;i<3;++i)if(!std::isfinite(from[i])||!std::isfinite(to[i]))return false;
    auto query=reinterpret_cast<void*>(physics+0x250);
    alignas(16) float start[4]{from[0],from[1],from[2],0},end[4]{to[0],to[1],to[2],0};
    reinterpret_cast<Reset>(gameBase+0x958920)(query);
    reinterpret_cast<Radius>(gameBase+0x962e70)(query,radius,player_rig::word(gameBase+0x15feb2c));
    queryFunction(query,reinterpret_cast<uintptr_t>(start),reinterpret_cast<uintptr_t>(end),
        player_rig::word(physics+0x24),1,reinterpret_cast<uintptr_t>(&hits));
    if(completed)*completed=true;
    return hits.count>0;
}
inline bool resolveHits(uintptr_t physics,const Context& c,HitArray& hits,const float* from,const float* to){
    if constexpr(!customContactEnabled)return false;
    if(!ready||!hits.count||!current(c)||component(c.owner,15)!=physics)return false;
    auto combat=component(c.owner,0);if(!combat)return false;
    // Native resolver copies 16 bytes per endpoint, not a 12-byte Vec3.
    alignas(16) float start[4]{from[0],from[1],from[2],0},end[4]{to[0],to[1],to[2],0};
    reinterpret_cast<Resolve>(gameBase+0xb9d520)(reinterpret_cast<void*>(combat),
        c.flags,&hits,start,end,c.talentIndex,c.key);
    return true;
}

// Call on the native physics thread, outside another use of this query object.
// Parent owns swing deduplication, local-player/equipment eligibility and pose
// freshness. The returned count is contacts, NOT proof of applied damage.
inline unsigned contact(uintptr_t physics,const Context& c,const float* from,const float* to,float radius) {
    if(!customContactEnabled)return 0;
    if(!ready||!from||!to||!std::isfinite(radius)||radius<=0||radius>20||!current(c)
        ||component(c.owner,15)!=physics)return 0;
    for(unsigned i=0;i<3;++i)if(!std::isfinite(from[i])||!std::isfinite(to[i]))return 0;
    auto combat=component(c.owner,0);if(!combat)return 0;
    HitArray hits;
    auto query=reinterpret_cast<void*>(physics+0x250);
    reinterpret_cast<Reset>(gameBase+0x958920)(query);
    reinterpret_cast<Radius>(gameBase+0x962e70)(query,radius,player_rig::word(gameBase+0x15feb2c));
    queryFunction(query,reinterpret_cast<uintptr_t>(from),reinterpret_cast<uintptr_t>(to),
        player_rig::word(physics+0x24),1,reinterpret_cast<uintptr_t>(&hits));
    auto count=hits.count;
    if(count&&current(c))reinterpret_cast<Resolve>(gameBase+0xb9d520)(reinterpret_cast<void*>(combat),
        c.flags,&hits,from,to,c.talentIndex,c.key);
    return count;
}
}
