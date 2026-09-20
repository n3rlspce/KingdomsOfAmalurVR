#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace amalur {
// Bounded identity registry for observed native lifetimes. External callers
// serialize access. No allocations and no game pointer dereferences occur here.
// Exhaustion fails closed globally: forgetting an old slot must never make an
// expired token look current. Do not reset while any tokens remain outstanding.
template<size_t Capacity>
class MeleeLifetimeRegistry {
    static_assert(Capacity>0);
public:
    uint64_t replace(uintptr_t object,uint32_t key=0){
        if(!healthy_||!object)return 0;
        if(serial_==(std::numeric_limits<uint64_t>::max)()){healthy_=false;return 0;}
        auto* entry=find(object,key,true);
        if(!entry){healthy_=false;return 0;}
        entry->object=object;entry->key=key;entry->serial=++serial_;entry->occupied=true;
        return entry->serial;
    }
    uint64_t current(uintptr_t object,uint32_t key=0) const {
        if(!healthy_||!object)return 0;
        for(size_t i=0,start=hash(object,key);i<Capacity;++i){
            const auto& entry=entries_[(start+i)%Capacity];
            if(!entry.occupied)return 0;
            if(entry.object==object&&entry.key==key)return entry.serial;
        }
        return 0;
    }
    bool matches(uintptr_t object,uint32_t key,uint64_t serial)const{
        return serial!=0&&current(object,key)==serial;
    }
    // Forget a completed lifetime without forgetting its serial history. A
    // tombstone preserves collision chains; a later claim gets a NEW serial.
    // Stale cleanup is never allowed to retire a replacement lifetime.
    bool retire(uintptr_t object,uint32_t key,uint64_t serial){
        if(!matches(object,key,serial))return false;
        auto* entry=find(object,key,false);
        entry->object=0;entry->serial=0;entry->occupied=true;
        return true;
    }
    bool healthy() const{return healthy_;}
private:
    struct Entry {uintptr_t object{};uint32_t key{};uint64_t serial{};bool occupied{};};
    std::array<Entry,Capacity> entries_{};
    uint64_t serial_{};
    bool healthy_{true};
    static size_t hash(uintptr_t object,uint32_t key){return ((object>>2)^key)%Capacity;}
    Entry* find(uintptr_t object,uint32_t key,bool empty){
        Entry* reusable=nullptr;
        for(size_t i=0,start=hash(object,key);i<Capacity;++i){
            auto& entry=entries_[(start+i)%Capacity];
            if(entry.object==object&&entry.key==key)return &entry;
            if(!entry.object){
                if(!reusable)reusable=&entry;
                if(!entry.occupied)break;
            }
        }
        return empty?reusable:nullptr;
    }
};
}
