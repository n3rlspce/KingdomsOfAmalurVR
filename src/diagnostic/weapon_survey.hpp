#pragma once
#include <atomic>

// Observation only. Include after player_rig/weapon_control and the declaration
// of rig_probe::playerRoot. No equipment, pose, visibility or slot writes.
namespace weapon_survey {
struct Identity {
    uintptr_t root{},object{},buffer{};
    uint32_t rootOwner{},owner{},asset{},count{},visibility{};
    unsigned nativeSlot{},effectiveSlot{};
};
inline Identity observed[32]{};
inline unsigned censusCount{},censusCursor{};
inline uint64_t lastTransition{};
inline std::atomic_flag busy=ATOMIC_FLAG_INIT;

inline void maps(uintptr_t table,unsigned which,unsigned sourceCount,unsigned count,uint32_t owner){
    if(!table||which>=32)return;
    const auto entry=table+which*32;
    const auto n=player_rig::word(entry+4),tuples=player_rig::word(entry);
    if(n>8||(!tuples&&n))return;
    for(unsigned i=0;i<n;++i){
        const auto from=player_rig::word(tuples+i*12),to=player_rig::word(tuples+i*12+4);
        if(from>=sourceCount||to>=count)continue;
        log("Weapon survey map owner=%08x slot=%u tuple=%u src=%u dst=%u flags=%u\n",
            owner,which,i,from,to,player_rig::word(tuples+i*12+8));
    }
}

inline void inspect(void* mapper,uintptr_t slot,uintptr_t source,uintptr_t output,uintptr_t nativeSlot){
    const auto root=rig_probe::playerRoot();
    if(!root||source!=root+0x34||output<0x34||slot>=32||nativeSlot>=32)return;
    const auto object=output-0x34;
    if(weapon_control::fab(player_rig::word(object+0x194))!=object)return;
    const auto children=player_rig::word(root+0x24),childCount=player_rig::word(root+0x28);
    if(childCount>32||(!children&&childCount))return;
    bool child=false;
    for(unsigned i=0;i<childCount;++i)
        if(weapon_control::fab(player_rig::word(children+i*4))==object){child=true;break;}
    if(!child)return;
    const auto owner=player_rig::word(object+0xf8);
    if(!player_rig::part(player_rig::resolve(owner),11,owner,0x135745c))return;
    const auto count=player_rig::word(output+4),buffer=player_rig::word(output);
    const auto sourceCount=player_rig::word(source+4),assetId=player_rig::word(object+0xf0);
    if(!buffer||!count||count>64||!sourceCount||sourceCount>128||assetId<2||assetId>=100000)return;
    const auto manager=player_rig::word(gameBase+0x15fdf54);
    if(!manager)return;
    const auto states=player_rig::word(manager+0x28),assets=player_rig::word(manager+0x18);
    if(!states||!assets)return;
    const auto state=*reinterpret_cast<const unsigned char*>(states+assetId);
    if(!(state&4)||(state&16))return;
    const auto asset=player_rig::word(assets+assetId*4);
    if(!asset)return;
    const auto blob=player_rig::word(asset+0x1c);
    if(!blob||player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=count)return;
    const auto idsOffset=player_rig::word(blob+0x20),parentsOffset=player_rig::word(blob+0x1c);
    if(!idsOffset||idsOffset>65536||!parentsOffset||parentsOffset>65536)return;
    const auto ids=reinterpret_cast<const uint32_t*>(blob+0x20+idsOffset);
    const auto parents=reinterpret_cast<const int16_t*>(blob+0x1c+parentsOffset);
    for(unsigned i=0;i<count;++i)if(parents[i]<-1||parents[i]>=static_cast<int>(count))return;
    const auto rootOwner=player_rig::word(root+0xf8),visibility=player_rig::word(object+0x1d0);
    unsigned found=censusCount;
    for(unsigned i=0;i<censusCount;++i){const auto& old=observed[i];
        if(old.root==root&&old.rootOwner==rootOwner&&old.object==object&&old.owner==owner
            &&old.asset==assetId&&old.buffer==buffer&&old.count==count){found=i;break;}}
    const auto now=GetTickCount64();
    if(found==censusCount){
        // Reserve before logging: a bad native table cannot cause a census storm.
        const auto cacheIndex=censusCount<32?censusCount++:censusCursor++%32;
        observed[cacheIndex]={root,object,buffer,rootOwner,owner,assetId,count,visibility,
            static_cast<unsigned>(nativeSlot),static_cast<unsigned>(slot)};
        log("Weapon survey census tick=%llu object=%08x owner=%08x rootOwner=%08x asset=%u bones=%u visibility=%08x nativeSlot=%u effectiveSlot=%u\n",
            now,unsigned(object),owner,rootOwner,assetId,count,visibility,unsigned(nativeSlot),unsigned(slot));
        const auto bones=reinterpret_cast<const amalur::RigBone*>(buffer);
        for(unsigned i=0;i<count;++i){
            const auto pose=amalur::bonePose(bones[i]);
            log("Weapon survey bone owner=%08x index=%u id=%08x parent=%d valid=%d pos=%.4f,%.4f,%.4f quat=%.4f,%.4f,%.4f,%.4f\n",
                owner,i,ids[i],int(parents[i]),mgs5vr::valid(pose),pose.position.x,pose.position.y,pose.position.z,
                pose.orientation.x,pose.orientation.y,pose.orientation.z,pose.orientation.w);
        }
        const auto table=player_rig::word(reinterpret_cast<uintptr_t>(mapper));
        for(unsigned s=0;s<9;++s)maps(table,s,sourceCount,count,owner);
        if(nativeSlot>=9)maps(table,unsigned(nativeSlot),sourceCount,count,owner);
        if(slot>=9&&slot!=nativeSlot)maps(table,unsigned(slot),sourceCount,count,owner);
    }else{
        auto& old=observed[found];
        if(old.visibility==visibility&&old.nativeSlot==nativeSlot&&old.effectiveSlot==slot)return;
        if(lastTransition&&now-lastTransition<250)return;
        lastTransition=now;old.visibility=visibility;old.nativeSlot=unsigned(nativeSlot);old.effectiveSlot=unsigned(slot);
        log("Weapon survey transition tick=%llu object=%08x owner=%08x asset=%u visibility=%08x nativeSlot=%u effectiveSlot=%u\n",
            now,unsigned(object),owner,assetId,visibility,unsigned(nativeSlot),unsigned(slot));
    }
}

inline void attachment(void* mapper,uintptr_t slot,uintptr_t source,uintptr_t output,uintptr_t nativeSlot){
    // Never wait on another remap thread; a missed survey sample is harmless.
    if(busy.test_and_set(std::memory_order_acquire))return;
    __try {inspect(mapper,slot,source,output,nativeSlot);}
    __except(EXCEPTION_EXECUTE_HANDLER){}
    busy.clear(std::memory_order_release);
}
}
