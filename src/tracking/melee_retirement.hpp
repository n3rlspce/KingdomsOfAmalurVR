#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace amalur {
// Build 10619381: manager+cc holds the notification pool. Its +14 vector has
// +18 records of 16 bytes. bbfcb0 cancels by ID alone, without checking index.
struct MeleeNotification {
    uint32_t completed{},expected{},runtimeIndex{},id{0xffffffffu};
};
static_assert(sizeof(MeleeNotification)==16);

struct MeleeRetirementPlan {
    uint32_t ids[2]{};
    unsigned count{};
    bool valid{};
};

// No writes or calls. Validate BOTH notification IDs before cancelling either.
// Missing IDs have already left the pool and need no cancellation. An ID that
// now points at another runtime, or appears twice, is ambiguous: reject it.
// Caller must retain the runtime's lifetime token and execute synchronously on
// the native update thread; this is not a replacement for lifetime validation.
inline MeleeRetirementPlan planMeleeRetirement(uint32_t index,uint32_t first,
    uint32_t second,const MeleeNotification* records,size_t count){
    MeleeRetirementPlan plan;
    if(!index||count>1048576||(count&&!records))return plan;
    for(auto id:{first,second}){
        if(id==0xffffffffu)continue;
        bool duplicate=false;
        for(unsigned j=0;j<plan.count;++j)if(plan.ids[j]==id)duplicate=true;
        if(duplicate)continue;
        unsigned found=0;
        for(size_t j=0;j<count;++j){
            if(records[j].id!=id)continue;
            if(records[j].runtimeIndex!=index||++found>1)return {};
        }
        if(found)plan.ids[plan.count++]=id;
    }
    plan.valid=true;
    return plan;
}
}
