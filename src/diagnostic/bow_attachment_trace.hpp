#pragma once
// Opt-in observation of small direct player attachments, including props whose
// owner is not a weapon(part11). No pose/slot/visibility/gameplay writes.
namespace bow_attachment_trace {
struct Seen {uintptr_t object{},buffer{};uint32_t owner{},asset{};unsigned slot{};bool solved{};};
inline Seen seen[32]{};inline unsigned count{};
inline std::atomic_flag busy=ATOMIC_FLAG_INIT;
inline const uint32_t* boneIds(uint32_t model,unsigned boneCount){
    if(model<2||model>=100000||!boneCount||boneCount>64)return nullptr;
    const auto manager=player_rig::word(gameBase+0x15fdf54);if(!manager)return nullptr;
    const auto states=player_rig::word(manager+0x28),assets=player_rig::word(manager+0x18);
    if(!states||!assets)return nullptr;
    const auto state=*reinterpret_cast<const unsigned char*>(states+model);
    if(!(state&4)||(state&16))return nullptr;
    const auto asset=player_rig::word(assets+model*4);if(!asset)return nullptr;
    const auto blob=player_rig::word(asset+0x1c);
    if(!blob||player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=boneCount)return nullptr;
    const auto offset=player_rig::word(blob+0x20);if(!offset||offset>65536)return nullptr;
    return reinterpret_cast<const uint32_t*>(blob+0x20+offset);
}
inline void inspect(void* mapper,uintptr_t slot,uintptr_t source,uintptr_t output,bool solved,const arm_rig::Scratch& scratch){
    if(count>=32||slot>=32||output<0x34||!firstPerson.load()||!headTracking.load())return;
    const auto root=rig_probe::playerRoot();if(!root||source!=root+0x34)return;
    const auto object=output-0x34;
    if(weapon_control::fab(player_rig::word(object+0x194))!=object)return;
    const auto childCount=player_rig::word(root+0x28),children=player_rig::word(root+0x24);
    if(childCount>32||(!children&&childCount))return;
    bool child=false;for(unsigned i=0;i<childCount;++i)if(weapon_control::fab(player_rig::word(children+4*i))==object){child=true;break;}
    if(!child)return;
    const auto owner=player_rig::word(object+0xf8),entity=player_rig::resolve(owner);if(!entity)return;
    const auto bones=player_rig::word(output+4),buffer=player_rig::word(output),model=player_rig::word(object+0xf0);
    const auto sourceCount=player_rig::word(source+4),sourceBuffer=player_rig::word(source);
    if(!bones||bones>16||!buffer||!sourceCount||sourceCount>64||!sourceBuffer)return;
    const auto ids=boneIds(model,bones),sourceIds=boneIds(player_rig::word(root+0xf0),sourceCount);
    if(!ids||!sourceIds)return;
    const auto table=player_rig::word(reinterpret_cast<uintptr_t>(mapper));if(!table)return;
    const auto entry=table+slot*32,n=player_rig::word(entry+4),tuples=player_rig::word(entry);
    if(!n||n>8||!tuples)return;
    for(unsigned i=0;i<n;++i)if(player_rig::word(tuples+i*12)>=sourceCount||player_rig::word(tuples+i*12+4)>=bones)return;
    for(unsigned i=0;i<count;++i){const auto& old=seen[i];if(old.object==object&&old.buffer==buffer&&old.owner==owner&&old.asset==model&&old.slot==slot&&old.solved==solved)return;}
    mgs5vr::Pose rootWorld;memcpy(&rootWorld.position,reinterpret_cast<void*>(root+0x124),12);
    memcpy(&rootWorld.orientation,reinterpret_cast<void*>(root+0x134),16);rootWorld=amalur::nativePose(rootWorld);
    if(!mgs5vr::valid(rootWorld))return;
    seen[count++]={object,buffer,owner,model,unsigned(slot),solved};
    const auto native=reinterpret_cast<const amalur::RigBone*>(sourceBuffer),mapped=reinterpret_cast<const amalur::RigBone*>(buffer);
    log("Bow attachment census record=%u tick=%llu object=%08x owner=%08x rootOwner=%08x model=%u bones=%u slot=%u solved=%d weapon=%d armor=%d shield=%d root=%.3f,%.3f,%.3f rootQ=%.5f,%.5f,%.5f,%.5f\n",
        count,GetTickCount64(),unsigned(object),owner,player_rig::word(root+0xf8),model,bones,unsigned(slot),solved,
        player_rig::part(entity,11,owner,0x135745c)!=0,player_rig::part(entity,12,owner,0x13563e4)!=0,player_rig::part(entity,40,owner,0x1356bec)!=0,
        rootWorld.position.x,rootWorld.position.y,rootWorld.position.z,rootWorld.orientation.x,rootWorld.orientation.y,rootWorld.orientation.z,rootWorld.orientation.w);
    for(unsigned i=0;i<n;++i){const auto from=player_rig::word(tuples+i*12),to=player_rig::word(tuples+i*12+4);
        const auto before=amalur::bonePose(native[from]),after=solved?amalur::bonePose(scratch.bones[from]):before,actual=amalur::bonePose(mapped[to]);
        log("Bow attachment tuple record=%u src=%u srcId=%08x dst=%u dstId=%08x flags=%u native=%.3f,%.3f,%.3f solved=%.3f,%.3f,%.3f mapped=%.3f,%.3f,%.3f valid=%d,%d,%d\n",
            count,from,sourceIds[from],to,ids[to],player_rig::word(tuples+i*12+8),before.position.x,before.position.y,before.position.z,
            after.position.x,after.position.y,after.position.z,actual.position.x,actual.position.y,actual.position.z,mgs5vr::valid(before),mgs5vr::valid(after),mgs5vr::valid(actual));
    }
    for(unsigned i=0;i<sourceCount;++i)if(sourceIds[i]==0x88d0eb){
        const auto wrist=mgs5vr::compose(rootWorld,amalur::bonePose(solved?scratch.bones[i]:native[i]));
        mgs5vr::Pose grip;uint64_t tick;AcquireSRWLockShared(&weapon_control::poseLock);grip=weapon_control::desired;tick=weapon_control::tick;ReleaseSRWLockShared(&weapon_control::poseLock);
        log("Bow attachment hand record=%u wristWorld=%.3f,%.3f,%.3f gripWorld=%.3f,%.3f,%.3f gripTick=%llu valid=%d,%d\n",
            count,wrist.position.x,wrist.position.y,wrist.position.z,grip.position.x,grip.position.y,grip.position.z,tick,mgs5vr::valid(wrist),mgs5vr::valid(grip));break;
    }
}
inline void observe(void* mapper,uintptr_t slot,uintptr_t source,uintptr_t output,bool solved,const arm_rig::Scratch& scratch){
    // Marker is read once per process: enable before the next authorized launch.
    static const bool enabled=GetFileAttributesW(L"amalur-bow-attachment.enable")!=INVALID_FILE_ATTRIBUTES;
    if(!enabled||busy.test_and_set(std::memory_order_acquire))return;
    __try{inspect(mapper,slot,source,output,solved,scratch);}__except(EXCEPTION_EXECUTE_HANDLER){}
    busy.clear(std::memory_order_release);
}
}
