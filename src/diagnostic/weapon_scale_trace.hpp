#pragma once
// Read-only scale transitions. Native scale getter RVA 6c62b0 reads +0x20
// only when transform flag +0x2c has 0x40; otherwise effective scale is one.
namespace weapon_scale_trace {
struct Scale {float xyz[3]{1,1,1};unsigned flags{};};
struct Snapshot {uintptr_t object{},buffer{};unsigned owner{},asset{},count{},nativeSlot{},heldSlot{};Scale world{},bone[16]{};};
inline Snapshot previous[8]{};inline unsigned records{};inline uint64_t lastRecord{};
inline std::atomic_flag busy=ATOMIC_FLAG_INIT;
inline Scale readScale(uintptr_t transform){
    Scale s;s.flags=*reinterpret_cast<const unsigned char*>(transform+0x2c);
    if(s.flags&0x40)memcpy(s.xyz,reinterpret_cast<const void*>(transform+0x20),12);
    return s;
}
inline bool equalScale(const Scale& a,const Scale& b){return !memcmp(a.xyz,b.xyz,12)&&((a.flags^b.flags)&0x40)==0;}
inline void inspect(void* mapper,uintptr_t nativeSlot,uintptr_t heldSlot,uintptr_t source,uintptr_t output){
    if(records>=64||output<0x34||nativeSlot>=32||heldSlot>=32)return;
    const auto root=weapon_control::currentWeaponRoot(),object=output-0x34;
    if(!root||source!=root+0x34||!weapon_control::isOnlyActivePlayerWeapon(object))return;
    Snapshot s;s.object=object;s.buffer=player_rig::word(output);s.count=player_rig::word(output+4);
    s.owner=player_rig::word(object+0xf8);s.asset=player_rig::word(object+0xf0);
    s.nativeSlot=unsigned(nativeSlot);s.heldSlot=unsigned(heldSlot);
    if(!s.buffer||!s.count||s.count>16)return;
    // Published Fab world transform starts at +0x124 (position/quaternion/scale).
    s.world=readScale(object+0x124);
    for(unsigned i=0;i<s.count;++i)s.bone[i]=readScale(s.buffer+i*48);
    unsigned index=8;
    for(unsigned i=0;i<8;++i)if(previous[i].object==object||!previous[i].object){index=i;break;}
    if(index==8)return;
    const auto& old=previous[index];bool changed=old.owner!=s.owner||old.asset!=s.asset||old.buffer!=s.buffer
        ||old.count!=s.count||old.nativeSlot!=s.nativeSlot||old.heldSlot!=s.heldSlot||!equalScale(old.world,s.world);
    for(unsigned i=0;i<s.count&&!changed;++i)changed=!equalScale(old.bone[i],s.bone[i]);
    const auto now=GetTickCount64();if(!changed||(lastRecord&&now-lastRecord<100))return;
    lastRecord=now;previous[index]=s;++records;
    log("Weapon scale record=%u tick=%llu object=%08x owner=%08x asset=%u nativeSlot=%u heldSlot=%u world=%.5f,%.5f,%.5f flags=%02x\n",
        records,now,unsigned(object),s.owner,s.asset,s.nativeSlot,s.heldSlot,s.world.xyz[0],s.world.xyz[1],s.world.xyz[2],s.world.flags);
    for(unsigned i=0;i<s.count;++i){const auto& b=s.bone[i];log("Weapon scale bone record=%u index=%u effective=%.5f,%.5f,%.5f flags=%02x\n",records,i,b.xyz[0],b.xyz[1],b.xyz[2],b.flags);}
    const auto table=player_rig::word(reinterpret_cast<uintptr_t>(mapper));if(!table)return;
    const auto n=player_rig::word(table+heldSlot*32+4),tuples=player_rig::word(table+heldSlot*32);
    const auto sourceCount=player_rig::word(source+4),sourceBuffer=player_rig::word(source);
    if(n>8||!tuples||!sourceBuffer||sourceCount>64)return;
    for(unsigned i=0;i<n;++i){const auto from=player_rig::word(tuples+i*12),to=player_rig::word(tuples+i*12+4);
        if(from>=sourceCount||to>=s.count)continue;const auto b=readScale(sourceBuffer+from*48);
        log("Weapon scale source record=%u from=%u to=%u effective=%.5f,%.5f,%.5f flags=%02x\n",records,from,to,b.xyz[0],b.xyz[1],b.xyz[2],b.flags);}
}
inline void observe(void* mapper,uintptr_t nativeSlot,uintptr_t heldSlot,uintptr_t source,uintptr_t output){
    static const bool enabled=GetFileAttributesW(L"amalur-weapon-scale.enable")!=INVALID_FILE_ATTRIBUTES;
    if(!enabled||busy.test_and_set(std::memory_order_acquire))return;
    __try{inspect(mapper,nativeSlot,heldSlot,source,output);}__except(EXCEPTION_EXECUTE_HANDLER){}
    busy.clear(std::memory_order_release);
}
}
