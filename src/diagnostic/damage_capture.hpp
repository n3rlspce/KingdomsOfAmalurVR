#pragma once
#include "../tracking/damage_capture_policy.hpp"
#include <atomic>
// Read-only native-thread evidence. This header must follow player_rig,
// melee_native and melee_owned_source definitions. No retained engine pointers.
namespace melee_recipe_capture {
inline std::atomic_flag busy=ATOMIC_FLAG_INIT;
inline std::atomic<unsigned> concurrentDrops{},ownedSkipped{};
inline amalur::DamageCaptureBudget budget,creationBudget;
inline uint64_t lastSample{},lastNotice{};
struct Seen {uint32_t owner{},key{},role{},index{},hash{};};
inline Seen seen[128]{};inline unsigned cursor{};
inline bool allowed(uint64_t now,bool creation=false){unsigned dropped=0;const bool yes=(creation?creationBudget:budget).take(now,dropped);
    if(dropped)log("Talent capture truncated tick=%llu reason=renewable-budget dropped=%u limit=64-per-second\n",now,dropped);
    return yes;
}
inline uint32_t localOwner(){auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
    if(!player||(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94))return 0;
    return player_rig::word(player+0x1ec);
}
inline void unavailable(const char* where,unsigned code=0){const auto now=GetTickCount64();
    if(now-lastNotice>=1000){lastNotice=now;log("Talent capture unavailable tick=%llu where=%s code=%08x concurrentDropped=%u ownedSkipped=%u\n",now,where,code,concurrentDrops.exchange(0),ownedSkipped.exchange(0));}}
inline void runtime(const char* phase,uintptr_t address,uint32_t owner,uint32_t key,unsigned role,uint32_t index,int result,bool force){
    const auto now=GetTickCount64();
    if(!address){unavailable("runtime-null");return;}
    const auto asset=player_rig::word(address+4),actualIndex=player_rig::word(address+0x20),actualOwner=player_rig::word(address+0x24),active=player_rig::word(address+0x1c);
    if(!amalur::damageCaptureRuntime(owner,actualOwner,index,actualIndex,asset)){unavailable("runtime-identity");return;}
    uint32_t fields[10]{};constexpr unsigned offsets[]{0xc,0x94,0x1a0,0x1a8,0x1bc,0x1f8,0x1fc,0x200,0x208,0x20c};
    const auto definition=melee_owned_source::resident(player_rig::word(gameBase+0x15f4dfc),asset);
    const bool validDefinition=definition&&player_rig::word(definition)==gameBase+0x135807c;
    uintptr_t listener=0,callback=0;bool listenerMatches=false;
    if(validDefinition){for(unsigned i=0;i<10;++i)fields[i]=player_rig::word(definition+offsets[i]);
        auto list=player_rig::word(definition+8);listener=list?player_rig::word(list):0;
        if(listener){listenerMatches=player_rig::word(listener+4)==definition;callback=player_rig::word(player_rig::word(listener)+4);}}
    uint32_t digest=2166136261u;for(auto value:{asset,active,actualIndex,actualOwner})digest=amalur::captureHashWord(digest,value);
    for(auto value:fields)digest=amalur::captureHashWord(digest,value);
    digest=amalur::captureHashWord(digest,static_cast<uint32_t>(callback));
    digest=amalur::captureHashWord(digest,listenerMatches?1u:0u);
    if(validDefinition){const auto availableScript=melee_owned_source::resident(player_rig::word(gameBase+0x15f4d34),fields[1]);
        digest=amalur::captureHashWord(digest,availableScript?1u:0u);}

    Seen* previous=nullptr;for(auto& entry:seen)if(entry.owner==owner&&entry.key==key&&entry.role==role&&entry.index==index){previous=&entry;break;}
    if(!force&&previous&&previous->hash==digest)return;
    if(!allowed(now,force))return;
    if(!previous)previous=&seen[cursor++%128];*previous={owner,key,role,index,digest};
    log("Talent capture runtime tick=%llu phase=%s owner=%08x key=%08x role=%u index=%u asset=%u active=%08x result=%d definition=%s listenerOwner=%u callbackRva=%08x fields=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
        now,phase,owner,key,role,index,asset,active,result,validDefinition?"verified-type":"unavailable-type",listenerMatches,
        callback>=gameBase?unsigned(callback-gameBase):0,fields[0],fields[1],fields[2],fields[3],fields[4],fields[5],fields[6],fields[7],fields[8],fields[9]);
    if(!validDefinition)return;
    auto script=melee_owned_source::resident(player_rig::word(gameBase+0x15f4d34),fields[1]);
    if(!script||player_rig::word(script)!=gameBase+0x1331a6c){log("Talent capture script owner=%08x asset=%u script=%u status=unavailable-type\n",owner,asset,fields[1]);return;}
    auto nameRecord=player_rig::word(script+0x14),length=nameRecord?player_rig::word(nameRecord+4):0;
    auto nameBytes=nameRecord?player_rig::word(nameRecord):0;char name[65]{};
    const auto copied=length<64?length:64;
    if(nameBytes)for(unsigned i=0;i<copied;++i){auto ch=*reinterpret_cast<const unsigned char*>(nameBytes+i);name[i]=ch>=32&&ch<127?char(ch):'?';}
    const auto size=player_rig::word(script+0x24),bytes=player_rig::word(script+0x20);
    uint32_t hash=2166136261u;const bool hashValid=bytes&&size&&size<=65536;
    if(hashValid)for(unsigned i=0;i<size;++i)hash=(hash^*reinterpret_cast<const unsigned char*>(bytes+i))*16777619u;
    log("Talent capture script owner=%08x asset=%u script=%u name=%s nameTruncated=%u bytes=%u fnv=%08x status=%s\n",owner,asset,fields[1],name,length>64,size,hash,hashValid?"hashed":"truncated-or-unavailable");
}
inline void created(uintptr_t address,uint32_t asset,uint32_t owner,uint32_t index,int result,bool ownedRequest){
    native_cast_haptics::created(address,asset,owner,index,result,ownedRequest);
    if(ownedRequest){++ownedSkipped;return;}
    if(busy.test_and_set(std::memory_order_acquire)){++concurrentDrops;return;}
    __try{if(owner&&owner==localOwner()){
        if(result!=0||!address||!index){if(allowed(GetTickCount64(),true))log("Talent capture constructor failed tick=%llu owner=%08x asset=%u index=%u result=%d\n",GetTickCount64(),owner,asset,index,result);}
        else if(player_rig::word(address+4)!=asset){unavailable("constructor-asset-mismatch");}
        else runtime("native-created",address,owner,0,0,index,result,true);
    }}__except(EXCEPTION_EXECUTE_HANDLER){unavailable("constructor-read-exception",GetExceptionCode());}
    busy.clear(std::memory_order_release);
}
inline void inspect(uintptr_t physics){
    const auto owner=localOwner();if(!owner||melee_native::component(owner,15)!=physics)return;
    const auto now=GetTickCount64();if(lastSample&&now>=lastSample&&now-lastSample<20)return;lastSample=now;
    const auto part=melee_native::component(owner,18),manager=player_rig::word(gameBase+0x15fec38);if(!part||!manager){unavailable("talent-part");return;}
    const auto count=player_rig::word(part+0x28),keys=player_rig::word(part+0x24),records=player_rig::word(part+0x34);
    const auto table=player_rig::word(manager+0xd0),total=player_rig::word(manager+0xd4);
    if(count>4096||count!=player_rig::word(part+0x38)||(!keys&&count)||(!records&&count)||!table||total>1048576){unavailable("talent-table");return;}
    unsigned visited=0,truncated=0;
    for(unsigned i=0;i<count;++i){if(i>=64){truncated+=count-i;break;}
        const auto key=player_rig::word(keys+i*4),record=records+i*24;
        const auto base=player_rig::word(record),recordAsset=player_rig::word(record+4),secondary=player_rig::word(record+8),n=player_rig::word(record+12);
        const auto digest=amalur::captureHashWord(amalur::captureHashWord(2166136261u,base),recordAsset)^n;
        Seen* descriptor=nullptr;for(auto& entry:seen)if(entry.owner==owner&&entry.key==key&&entry.role==0xfffffffeu){descriptor=&entry;break;}
        if((!descriptor||descriptor->hash!=digest)&&allowed(now)){
            if(!descriptor)descriptor=&seen[cursor++%128];*descriptor={owner,key,0xfffffffeu,0,digest};
            log("Talent capture record tick=%llu owner=%08x key=%08x base=%u recordAsset=%u secondaryCount=%u\n",now,owner,key,base,recordAsset,n);
        }
        if(n>256||(!secondary&&n)){unavailable("secondary-table");continue;}
        for(unsigned role=0;role<=n;++role){if(visited>=128){truncated+=n+1-role;break;}++visited;
            const auto index=role?player_rig::word(secondary+(role-1)*4):base;
            if(index==0xffffffffu||!index)continue;
            if(index>=total){unavailable("runtime-index");continue;}
            runtime("talent-record",player_rig::word(table+index*4),owner,key,role,index,0,false);
        }
    }
    if(player_rig::word(part+0x28)!=count||player_rig::word(part+0x24)!=keys||player_rig::word(part+0x34)!=records
        ||player_rig::word(manager+0xd0)!=table||player_rig::word(manager+0xd4)!=total)unavailable("snapshot-changed");
    static uint64_t report{};if(now-report>=1000){report=now;log("Talent capture summary tick=%llu owner=%08x records=%u scanned=%u truncated=%u budgetDropped=%u creationDropped=%u concurrentDropped=%u ownedSkipped=%u\n",
        now,owner,count,visited,truncated,budget.dropped,creationBudget.dropped,concurrentDrops.exchange(0),ownedSkipped.exchange(0));}
}
inline void sample(uintptr_t physics){
    if(busy.test_and_set(std::memory_order_acquire)){++concurrentDrops;return;}
    __try{inspect(physics);}__except(EXCEPTION_EXECUTE_HANDLER){unavailable("sample-read-exception",GetExceptionCode());}
    busy.clear(std::memory_order_release);
}
}
