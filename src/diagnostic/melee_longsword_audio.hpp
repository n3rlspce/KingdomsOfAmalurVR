// Included inside melee_feedback, after resolved observers. Game-thread only.
namespace longsword_audio {
using Allocate=uintptr_t(__thiscall*)(void*,uintptr_t,uint32_t,uint32_t,uint32_t,uint32_t);
using Position=void(__thiscall*)(void*,float,float,float);
using Stop=void(__thiscall*)(void*,uint32_t);
struct Backend {Allocate allocate{};Position position{};Stop stop{};};
inline Backend backend;
inline bool ready{};
struct Voice {uint32_t handle{};uint64_t born{};};
struct Driver {
    struct Pending {uint32_t selector{};unsigned serial{};uint64_t due{};mgs5vr::Vec3 position{};};
    Voice voices[8]{};Pending pending[2]{};unsigned next{};uintptr_t manager{};bool eligible{};
    uint32_t owner{},weapon{};unsigned generation{},lastSerial[2]{};uint64_t lastSwing[2]{},lastVoice{};
    void cancelVoices(const Backend& api,uintptr_t currentManager,uint64_t now){
        for(auto& v:voices){
            // Only own pool3 handles, within a bounded age. Native resolver
            // checks slot generation before stop; never keep object pointers.
            if(v.born&&now>=v.born&&now-v.born<10000&&manager==currentManager&&manager&&api.stop)
                api.stop(reinterpret_cast<void*>(manager),v.handle);
            v={};
        }
        for(auto& p:pending)p={};
        next=0;
    }
    void reset(const Backend& api,uintptr_t currentManager,uint64_t now){
        cancelVoices(api,currentManager,now);eligible=false;
        owner=weapon=generation=0;for(auto& serial:lastSerial)serial=0;for(auto& tick:lastSwing)tick=0;lastVoice=0;manager=0;next=0;
    }
    void update(const Backend& api,uintptr_t currentManager,uint32_t currentOwner,uint32_t currentWeapon,unsigned center,bool allowed,uint64_t now){
        if(manager!=currentManager||owner!=currentOwner||weapon!=currentWeapon||generation!=center){
            reset(api,currentManager,now);
            manager=currentManager;owner=currentOwner;weapon=currentWeapon;generation=center;
        }
        eligible=allowed;
        // Temporary focus/menu/tracking loss stops all owned playback, but
        // preserves the consumed serials for this exact actor/weapon/center.
        // Replaying the same recent inbox event cannot make a second whoosh.
        if(!allowed){cancelVoices(api,currentManager,now);return;}
        // A native one-shot normally finishes itself. This also bounds a bank
        // wait or unexpected looping definition without holding native pointers.
        for(auto& v:voices)if(v.born&&(now<v.born||now-v.born>=4000)){
            if(now>=v.born&&now-v.born<10000&&api.stop&&manager==currentManager)
                api.stop(reinterpret_cast<void*>(manager),v.handle);
            v={};
        }
        for(unsigned hand=0;hand<2;++hand){auto& p=pending[hand];
            if(!p.selector)continue;
            // Never emit a stale delayed cue after a stalled update or a newer
            // strike. Identity/menu/focus cancellation above clears this queue.
            if(p.serial!=lastSerial[hand]||now+129<p.due||(now>=p.due&&now-p.due>100)){p={};continue;}
            if(now>=p.due){const auto cue=p;p={};emit(api,cue.selector,0xffffffff,cue.position,now);}
        }
    }
    bool emit(const Backend& api,uint32_t selector,uint32_t mode,const mgs5vr::Vec3& p,uint64_t now){
        if(!eligible||!manager||!api.allocate||!api.position||!api.stop)return false;
        auto& slot=voices[next++%8];
        if(slot.born&&now>=slot.born&&now-slot.born<10000)api.stop(reinterpret_cast<void*>(manager),slot.handle);
        slot={};uint32_t handle=0xffff;
        const auto object=api.allocate(reinterpret_cast<void*>(manager),reinterpret_cast<uintptr_t>(&handle),selector,mode,1,0);
        // Allocator824f30 emits pool3/index<100. Non-null object alone does not
        // prove audible success: the native bank updater starts it later.
        if(!object||((handle>>16)&255)!=3||(handle&65535)>=100)return false;
        slot={handle,now};api.position(reinterpret_cast<void*>(object),p.x,p.y,p.z);
        return true;
    }
    bool swing(const Backend& api,const amalur::MeleeSwingEvent& e,uint64_t now){
        if(!eligible||!manager||!amalur::physicalMeleeRecipe(e.asset).attack||e.owner!=owner||e.weapon!=weapon||e.generation!=generation||!amalur::supportedMeleeHand(e.asset,e.hand)||!e.serial||e.serial==lastSerial[e.hand]
            ||!e.tick||now<e.tick||now-e.tick>300||(lastSwing[e.hand]&&now>=lastSwing[e.hand]&&now-lastSwing[e.hand]<100))return false;
        const auto& p=e.weaponPose.position;
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||std::fabs(p.x)>1000000||std::fabs(p.y)>1000000||std::fabs(p.z)>1000000)return false;
        const auto sounds=amalur::nativeSwingSounds(e.asset,e.attackAsset,e.attackFlags);
        if(!sounds.motion&&!sounds.voice)return false;
        lastSerial[e.hand]=e.serial;lastSwing[e.hand]=now;
        const bool slash=sounds.motion&&emit(api,sounds.motion,0xffffffff,p,now);
        // Native attack81 schedules its additional event at129ms. Anchor that
        // delay to VR's committed release, not charge preparation or contact.
        // This is an animation-to-gesture timing adaptation; selectors/modes
        // remain exact captured values. A new same-hand strike replaces it.
        pending[e.hand]={};
        const bool supplemental=sounds.supplemental!=0;
        if(supplemental)pending[e.hand]={sounds.supplemental,e.serial,now+129,p};
        // Separate hands may swing together, but the actor only grunts once.
        bool voice=false;
        if(sounds.voice&&(!lastVoice||now<lastVoice||now-lastVoice>=100)){
            voice=emit(api,sounds.voice,0,p,now);if(voice)lastVoice=now;
        }
        return slash||voice||supplemental;
    }
};
inline Driver driver;
#ifndef AMALUR_AUDIO_DRIVER_CHECK
inline uintptr_t currentManager(){
    __try{const auto system=player_rig::word(gameBase+0x15fde70);return system?system+0x6174:0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
inline bool identity(uint32_t owner,uint32_t weapon){
    __try{
        const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!player||player_rig::word(player+0x1ec)!=owner)return false;
        const auto actor=player_rig::resolve(owner),held=player_rig::resolve(weapon);
        return actor&&held&&player_rig::word(actor+0x38)==owner&&player_rig::word(held+0x38)==weapon;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Root must call every native update, including disallowed/menu/focus states.
inline void update(uint32_t owner,uint32_t weapon,uint32_t model,unsigned generation,bool allowed){
    const auto manager=currentManager();
    driver.update(backend,manager,owner,weapon,generation,ready&&allowed&&amalur::physicalMeleeRecipe(model).attack&&identity(owner,weapon),GetTickCount64());
}
inline void cancel(){driver.reset(backend,currentManager(),GetTickCount64());}
inline void onSwing(const amalur::MeleeSwingEvent& e){
    if(!ready||!amalur::physicalMeleeRecipe(e.asset).attack||!identity(e.owner,e.weapon))return;
    const auto manager=currentManager();if(!manager)return;
    // The caller supplies an accepted/fresh game-thread gesture; capture inbox
    // already deduplicates serials. Regular update still handles cancellation.
    driver.update(backend,manager,e.owner,e.weapon,e.generation,true,GetTickCount64());
    const bool queued=driver.swing(backend,e,GetTickCount64());
    log("VR melee audio pilot tick=%llu attack=%u owner=%08x weapon=%08x serial=%u queued=%d classification=family-attack-pair-needs-listening\n",GetTickCount64(),e.attackAsset,e.owner,e.weapon,e.serial,queued);
}
inline void install(){
    __try{
        const unsigned char allocate[]{0x8b,0x44,0x24,0x04,0x53,0x56,0x57};
        const unsigned char position[]{0xf3,0x0f,0x7e,0x44,0x24,0x04};
        const unsigned char stop[]{0x56,0x8d,0x44,0x24,0x08,0x50};
        // The optional capture observer patches82B200, so its already-verified
        // trampoline takes precedence. Otherwise require the same retail entry
        // bytes used by resolved::install before calling the native allocator.
        const auto allocator=resolved::originalAllocate?resolved::originalAllocate:
            !memcmp(reinterpret_cast<void*>(gameBase+0x82b200),allocate,sizeof(allocate))?
                reinterpret_cast<Allocate>(gameBase+0x82b200):nullptr;
        if(!allocator||memcmp(reinterpret_cast<void*>(gameBase+0x823680),position,sizeof(position))
            ||memcmp(reinterpret_cast<void*>(gameBase+0x82b2f0),stop,sizeof(stop))){log("VR longsword audio pilot rejected reason=native-backend-signature\n");return;}
        backend={allocator,reinterpret_cast<Position>(gameBase+0x823680),reinterpret_cast<Stop>(gameBase+0x82b2f0)};
        ready=true;log("VR longsword audio pilot enabled models=2478,5457,1520,1250,1323 slots=8 lifetime=4000ms backend=%s\n",resolved::originalAllocate?"capture-trampoline":"native-direct");
    }__except(EXCEPTION_EXECUTE_HANDLER){ready=false;}
}
#endif
}
