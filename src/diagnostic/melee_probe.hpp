#pragma once
// Opt-in observation only. Never replay an animation event or retain its pointers.
namespace melee_contact {inline void capture(uintptr_t component,uintptr_t event);}
namespace melee_probe {
using Register=uintptr_t(__thiscall*)(void*,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
inline Register original{};
using Generate=void(__thiscall*)(void*,uintptr_t,uintptr_t,uintptr_t);
inline Generate originalGenerate{};
inline std::atomic<unsigned> samples{0};
inline std::atomic<uint64_t> sampleSecond{0};
inline std::atomic<unsigned> droppedSamples{0};
inline bool local(uintptr_t component){
    auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());if(!player)return false;
    if(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94)return false;
    auto owner=player_rig::word(player+0x1ec),entity=player_rig::resolve(owner);
    return entity&&player_rig::word(entity+0x3c+15*4)==component
        &&player_rig::word(component+0x18)==owner&&player_rig::word(component+0x1c)==15;
}
inline void observe(uintptr_t component,uintptr_t event,uintptr_t a,uintptr_t b,uintptr_t d){
    __try{
        if(!local(component)||player_rig::word(event)!=gameBase+0x13295ec)return;
        const auto now=GetTickCount64(),second=now/1000;
        if(sampleSecond.exchange(second)!=second)samples.store(0);
        if(samples.fetch_add(1)>=24){++droppedSamples;return;}
        const auto dropped=droppedSamples.exchange(0);
        auto n=player_rig::word(event+0x18),ids=player_rig::word(event+0x1c);
        log("Melee event tick=%llu id=%08x shapes=%u flag=%u times=%u,%u context=%08x active=%u dropped=%u\n",
            now,player_rig::word(event+0x28),n,player_rig::word(event+0x24),unsigned(a),unsigned(b),unsigned(d),player_rig::word(component+0x2e4),dropped);
        if(n<=16)for(unsigned i=0;i<n;++i)log("Melee shape index=%u id=%08x\n",i,player_rig::word(ids+i*4));
        // Synchronous flags/key observation complements the 50ms external
        // resource capture, which can miss very short native attack windows.
        auto count=player_rig::word(component+0x2e4),entries=player_rig::word(component+0x2e0);
        if(count>32||(!entries&&count))return;
        for(unsigned i=0;i<count;++i){auto e=entries+i*0x34;
            if(player_rig::word(e)!=player_rig::word(event+0x28))continue;
            log("Melee attack recipe tick=%llu owner=%08x event=%08x flags=%08x runtimeIndex=%u key=%08x\n",
                GetTickCount64(),player_rig::word(component+0x18),player_rig::word(e),player_rig::word(e+0x14),
                player_rig::word(e+0x2c),player_rig::word(e+0x30));
            const auto owner=player_rig::word(component+0x18),index=player_rig::word(e+0x2c);
            const auto manager=player_rig::word(gameBase+0x15fec38);
            const auto size=player_rig::word(manager+0xd4),table=player_rig::word(manager+0xd0);
            uint32_t asset=0;bool runtimeValid=false;
            if(table&&index&&index<size&&size<=1048576){
                const auto runtime=player_rig::word(table+index*4);
                if(runtime&&player_rig::word(runtime+0x24)==owner&&player_rig::word(runtime+0x20)==index
                    &&(player_rig::word(runtime+0x1c)&1)){asset=player_rig::word(runtime+4);runtimeValid=true;}
            }
            uint32_t model;uint64_t poseTick;
            AcquireSRWLockShared(&weapon_control::poseLock);model=weapon_control::visualAsset;poseTick=weapon_control::visualTick;
            ReleaseSRWLockShared(&weapon_control::poseLock);
            log("VR native combo event tick=%llu owner=%08x event=%08x key=%08x attackAsset=%u runtimeValid=%d flags=%u observedModel=%u modelFresh=%d selection=%u\n",
                now,owner,player_rig::word(e),player_rig::word(e+0x30),asset,runtimeValid,player_rig::word(e+0x14),model,
                poseTick&&poseTick<=now&&now-poseTick<100,motion_controls::viewControls().selectedWeapon);
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline uintptr_t __fastcall registration(void* self,void*,uintptr_t event,uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){
    auto result=original(self,event,a,b,c,d);
    melee_contact::capture(reinterpret_cast<uintptr_t>(self),event);
    observe(reinterpret_cast<uintptr_t>(self),event,a,b,d);return result;
}
inline void inspectShapes(uintptr_t self,uintptr_t array,unsigned before){
    __try{
        static unsigned count=0;if(count>=12||!weapon_control::isPlayerDaggers(self))return;
        auto n=player_rig::word(array+4),table=player_rig::word(array);
        if(n<=before||n>before+4)return;++count;
        for(unsigned i=before;i<n;++i){auto group=table+i*20,poses=player_rig::word(group+4),total=player_rig::word(group+8);
            if(total>16)continue;
            for(unsigned j=0;j<total;++j){auto p=reinterpret_cast<const float*>(poses+j*96);
                log("Melee dagger shape group=%u set=%u index=%u pos=%.3f,%.3f,%.3f tracked=%d\n",i,player_rig::word(group),j,p[0],p[1],p[2],headTracking.load());}
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline void __fastcall generate(void* self,void*,uintptr_t transform,uintptr_t array,uintptr_t includeHidden){
    unsigned before=0;__try{before=player_rig::word(array+4);}__except(EXCEPTION_EXECUTE_HANDLER){}
    originalGenerate(self,transform,array,includeHidden);
    inspectShapes(reinterpret_cast<uintptr_t>(self),array,before);
}
inline void install(){
    if(GetFileAttributesW(L"amalur-melee-probe.enable")==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(L"amalur-melee-contact.enable")==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(L"amalur-owned-melee.enable")==INVALID_FILE_ATTRIBUTES)return;
    auto target=reinterpret_cast<unsigned char*>(gameBase+0xb82a10);
    const unsigned char prefix[]{0x51,0x8b,0x44,0x24,0x08,0x53,0x55,0x8b,0x68,0x28};
    if(memcmp(target,prefix,sizeof(prefix))){log("Melee probe signature mismatch\n");return;}
    hook(target,reinterpret_cast<void*>(&registration),reinterpret_cast<void**>(&original),"Local-player melee event observation");
    auto gen=reinterpret_cast<unsigned char*>(gameBase+0x971510);
    const unsigned char genPrefix[]{0x83,0xec,0x5c,0xa1};
    if(!memcmp(gen,genPrefix,sizeof(genPrefix))&&player_rig::word(reinterpret_cast<uintptr_t>(gen)+4)==gameBase+0x157713c)
        hook(gen,reinterpret_cast<void*>(&generate),reinterpret_cast<void**>(&originalGenerate),"Dagger collision pose observation");
}
}
