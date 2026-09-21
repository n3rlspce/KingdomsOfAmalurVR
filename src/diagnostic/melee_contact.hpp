#pragma once
#include "melee_native.hpp"
#include "melee_lifetime_hooks.hpp"
#include "melee_owned_source.hpp"
#include "damage_capture.hpp"
#include "../tracking/melee_owned_backend.hpp"
#include "../tracking/melee_hit_identity.hpp"
#include "game_pause.hpp"
#include "melee_debug.hpp"
#include "../tracking/weapon_contact_profile.hpp"
namespace melee_contact {
using Update=void(__thiscall*)(void*);
inline Update originalUpdate{};
inline std::atomic<DWORD> updateThread{0};
inline std::atomic<bool> faulted{false};
inline bool querySuspended{};
inline uint32_t lastArmingKey{},suspendedArmingKey{};
inline thread_local bool inContact=false;
inline melee_owned_source::Lease source;
inline uint64_t nextEquipmentProbe{};
// Captured basic dagger199/longsword50/greatsword417 recipes use flags0.
// Retain only the exact model pairing and resident/script-validated definition.
// This does not execute an animation, synthesize input, or clear fault latches.
inline void armEquipped(uintptr_t physics,uint32_t actor,uint32_t weapon);
inline uint32_t nextPrivateKey=0xf0000000;
inline unsigned contextsCreated{},contextsClosed{};
inline thread_local unsigned traceContact{};
inline unsigned traceSequence{};
inline void traceClocks(const char* phase){
    __try{
        auto manager=player_rig::word(gameBase+0x15fb228);if(!manager)return;
        auto count=player_rig::word(manager+0xc),table=player_rig::word(manager+8);
        if(count>64||!table)return;
        for(unsigned i=0;i<count;++i){auto clock=player_rig::word(table+i*4);
            if(!clock||player_rig::word(clock)!=gameBase+0x132f100)continue;
            log("VR owned melee clock id=%u phase=%s index=%u current=%u flags=%02x value30=%g modifiers=%u\n",
                traceContact,phase,i,player_rig::word(manager+4),unsigned(*reinterpret_cast<const unsigned char*>(clock+0x4a)),
                double(*reinterpret_cast<const float*>(clock+0x30)),player_rig::word(clock+0x3c));
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){log("VR owned melee clock snapshot unavailable id=%u phase=%s\n",traceContact,phase);}
}
inline void checkpoint(const char* phase){
    if(traceContact)log("VR owned melee checkpoint id=%u phase=%s tick=%llu paused=%d created=%u closed=%u fault=%d\n",
        traceContact,phase,GetTickCount64(),game_pause::sample(true),contextsCreated,contextsClosed,faulted.load());
    if(traceContact)traceClocks(phase);
}
struct Progress {
    uint64_t reportTick{};
    unsigned updates{},ready{},fresh[2]{},continuous[2]{},swings[2]{},fast[2]{},sweeps{},queryHits{};
    uint32_t blocked{};
    unsigned actorHits{},selfHits{},otherHits{},resolverCalls{},resolverRejected{};
};
inline Progress progress;
inline void reportProgress(){
    auto now=GetTickCount64();if(now-progress.reportTick<2000)return;
    if(source.held)log("VR owned melee progress blocked=%03x updates=%u ready=%u fresh=%u,%u continuous=%u,%u swings=%u,%u fast=%u,%u sweeps=%u queryHits=%u created=%u closed=%u actorHits=%u selfHits=%u otherHits=%u resolverCalls=%u resolverRejected=%u\n",
        progress.blocked,progress.updates,progress.ready,progress.fresh[0],progress.fresh[1],progress.continuous[0],progress.continuous[1],
        progress.swings[0],progress.swings[1],progress.fast[0],progress.fast[1],progress.sweeps,progress.queryHits,contextsCreated,contextsClosed,
        progress.actorHits,progress.selfHits,progress.otherHits,progress.resolverCalls,progress.resolverRejected);
    progress={};progress.reportTick=now;
}
inline void fault(const char* reason){
    if(!faulted.exchange(true))log("VR owned melee FAULT: %s; physical contacts disabled until restart\n",reason);
    motion_controls::contactEnabled.store(false);motion_controls::meleeContextReady.store(false);
}
struct Equipped {uint32_t weapon{},model{},attack{};unsigned hands{};};
inline Equipped equipped(){
    uint32_t publishedOwner,publishedModel,publishedSelection;uint64_t publishedTick;
    AcquireSRWLockShared(&weapon_control::poseLock);
    publishedOwner=weapon_control::visualWeapon;publishedModel=weapon_control::visualAsset;
    publishedSelection=weapon_control::visualSelection;publishedTick=weapon_control::visualTick;
    ReleaseSRWLockShared(&weapon_control::poseLock);
    if(!amalur::currentPhysicalPublication(publishedOwner,publishedModel,publishedSelection,
        motion_controls::viewControls().selectedWeapon,publishedTick,GetTickCount64()))return {};
    auto root=rig_probe::playerRoot();if(!root)return {};
    auto n=player_rig::word(root+0x28);if(n>32)return {};Equipped result{};
    for(unsigned i=0;i<n;++i){auto obj=weapon_control::fab(player_rig::word(player_rig::word(root+0x24)+i*4));if(!obj)continue;
        const auto model=player_rig::word(obj+0xf0);
        if(model!=publishedModel||player_rig::word(obj+0xf8)!=publishedOwner)continue;
        const auto recipe=amalur::physicalMeleeRecipe(model);if(!recipe.attack)continue;
        const bool matches=model==1520?weapon_control::isPlayerDaggers(obj):weapon_control::capturedHeldKind(obj)!=amalur::HeldWeaponKind::None;
        if(!matches)continue;
        auto owner=player_rig::word(obj+0xf8);if(result.weapon&&result.weapon!=owner)return {};
        result={owner,model,recipe.attack,recipe.hands};
    }
    return result;
}
inline bool eligible(uintptr_t physics){
    return !faulted.load()&&!querySuspended&&motion_controls::contactEnabled.load()&&melee_probe::local(physics)
        &&headTracking.load()&&firstPerson.load()&&!interfaceView.load()&&arm_rig::enabled.load()
        &&motion_controls::gameFocused()&&motion_controls::viewControls().selectedWeapon==0;
}
inline void armEquipped(uintptr_t physics,uint32_t actor,uint32_t weapon){
    if(source.held||!actor||!weapon||!melee_native::ready||!eligible(physics)
        ||updateThread.load()!=GetCurrentThreadId()||player_rig::word(physics+0x2e4))return;
    auto now=GetTickCount64();if(now<nextEquipmentProbe)return;nextEquipmentProbe=now+1000;
    const auto equipment=equipped();if(equipment.weapon!=weapon||!equipment.attack)return;
    auto resources=player_rig::word(gameBase+0x15f4dfc);
    auto definition=melee_owned_source::resident(resources,equipment.attack);
    if(const auto reason=melee_owned_source::assetRejection(definition,true)){
        static uint64_t report{};static uint32_t lastModel{};if(now>=report||lastModel!=equipment.model){report=now+2000;lastModel=equipment.model;
            log("VR physical recipe rejected tick=%llu reason=%s owner=%08x weapon=%08x model=%u attack=%u selection=%u\n",now,reason,actor,weapon,equipment.model,equipment.attack,motion_controls::viewControls().selectedWeapon);}
        return;
    }
    if(!source.acquire(actor,weapon,equipment.attack,definition,0)){
        fault("verified equipment source could not be retained");return;
    }
    log("VR physical equipment armed without trigger owner=%08x weapon=%08x model=%u asset=%u flags=00000000 epoch=%llu\n",
        actor,weapon,equipment.model,equipment.attack,source.epoch);
}
inline void capture(uintptr_t physics,uintptr_t event){
    if constexpr(!melee_native::customContactEnabled)return;
    if(!motion_controls::contactEnabled.load()||faulted.load()
        ||inContact||updateThread.load()!=GetCurrentThreadId())return;
    __try{
        if(!melee_native::ready||!melee_probe::local(physics)||player_rig::word(event)!=gameBase+0x13295ec)return;
        const auto equipment=equipped();auto weapon=equipment.weapon;if(!weapon)return;
        if(player_rig::word(event+0x14)!=0)return;
        if(equipment.model==1520&&(player_rig::word(event+0x18)!=1
            ||player_rig::word(player_rig::word(event+0x1c))!=0x0053e8fd))return;
        auto count=player_rig::word(physics+0x2e4),entries=player_rig::word(physics+0x2e0);if(count>32)return;
        for(unsigned i=0;i<count;++i){auto e=entries+i*0x34;if(player_rig::word(e)!=player_rig::word(event+0x28))continue;
            auto owner=player_rig::word(physics+0x18),part=melee_native::component(owner,18);
            auto index=player_rig::word(e+0x2c),key=player_rig::word(e+0x30);
            auto manager=player_rig::word(gameBase+0x15fec38);if(!manager||!part)continue;
            auto table=player_rig::word(manager+0xd0),size=player_rig::word(manager+0xd4);
            if(!table||!index||index>=size||size>1048576)continue;
            auto runtime=player_rig::word(table+index*4);
            if(!melee_native::runtimeCurrent(table,size,index,runtime,equipment.attack,owner))continue;
            melee_native::OwnedCalls access;amalur::MeleeCreation<melee_native::OwnedCalls> creation(access);
            auto record=creation.record(part,key);
            if(!record.tableValid||!record.address||player_rig::word(record.address)!=index||player_rig::word(record.address+12))continue;
            auto definition=melee_owned_source::resident(player_rig::word(gameBase+0x15f4dfc),equipment.attack);
            bool wasReady=source.current(owner,weapon);
            if(source.acquire(owner,weapon,equipment.attack,definition,player_rig::word(e+0x14))){
                lastArmingKey=key;
                if(querySuspended&&key!=suspendedArmingKey){
                    querySuspended=false;
                    log("VR owned melee query rearmed by fresh native attack owner=%08x key=%08x\n",owner,key);
                }
                motion_controls::meleeContextReady.store(!querySuspended);
                if(!wasReady)log("VR owned melee source ready owner=%08x weapon=%08x asset=%u flags=%08x epoch=%llu\n",owner,weapon,equipment.attack,source.flags,source.epoch);
            }
            break;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){fault("source capture exception");}
}
struct NativeEnvironment {
    using Access=melee_native::OwnedCalls;
    Access calls;uintptr_t physics;uint32_t owner,weapon;uint64_t epoch;
    explicit NativeEnvironment(uintptr_t p):physics(p),owner(player_rig::word(p+0x18)),weapon(source.weapon),epoch(source.epoch){
        melee_lifetime_hooks::connect(calls);calls.onNativeUpdateThread=updateThread.load()==GetCurrentThreadId();
    }
    bool supported(const amalur::MeleeContextRecipe& r)const{
        return calls.onNativeUpdateThread&&!faulted.load()&&r.owner==owner&&r.baseAsset==source.asset
            &&r.equipmentGeneration==epoch&&source.epoch==epoch&&source.current(owner,weapon)
            &&equipped().weapon==weapon&&equipped().attack==source.asset&&eligible(physics)&&player_rig::word(physics+0x2e4)==0;
    }
    uintptr_t part()const{return melee_native::component(owner,18);}
    uintptr_t pool()const{auto m=player_rig::word(gameBase+0x15fec38);return m?m+0xcc:0;}
    uint32_t target()const{return 0;}
    uint32_t nextKey(){return nextPrivateKey<0xfffffffe?++nextPrivateKey:0;}
    bool current(uintptr_t address,uint64_t generation)const{return melee_lifetime_hooks::matches(address,generation);}
    bool release(uintptr_t address,uint64_t generation,uint32_t index,uint32_t asset,uint32_t actor){
        melee_lifetime_hooks::RuntimeRegistry registry;
        auto result=melee_native::retireOwnedRuntime(registry,address,generation,index,asset,actor);
        return result==melee_native::RetirementResult::Released||result==melee_native::RetirementResult::Expired;
    }
    void fault(const char* reason){melee_contact::fault(reason);}
};
using Backend=amalur::MeleeOwnedBackend<NativeEnvironment>;
struct Operation {NativeEnvironment env;Backend backend;amalur::MeleeContextScope<Backend> scope;
    explicit Operation(uintptr_t physics):env(physics),backend(env),scope(backend){}
};
struct Dedup {uintptr_t data{};uint32_t count{},capacity{};int16_t allocator{0x27},flags{-1};};
static_assert(sizeof(Dedup)==16);
struct HandState {mgs5vr::Pose previous{};uint64_t tick{},retryAt{},epoch{};unsigned generation{},serial{},attempts{};uint32_t owner{};bool consumed{};amalur::MeleeSwingWindow window;Dedup hits[2];};
inline HandState hands[2];
inline unsigned resolveScoped(uintptr_t physics,const melee_native::Context& c,HandState& hand,melee_native::HitArray& hits,const float* from,const float* to){
    auto combat=melee_native::component(c.owner,0);if(!combat)return 0;
    Dedup saved[2];memcpy(saved,reinterpret_cast<void*>(combat+0x78),sizeof(saved));
    for(unsigned i=0;i<2;++i)if(!hand.hits[i].data){hand.hits[i].allocator=saved[i].allocator;hand.hits[i].flags=saved[i].flags;}
    unsigned accepted=0;memcpy(reinterpret_cast<void*>(combat+0x78),hand.hits,sizeof(hand.hits));
    __try{auto before=hand.hits[0].count;
        checkpoint("resolver-enter");
        if(melee_native::resolveHits(physics,c,hits,from,to))++progress.resolverCalls;else ++progress.resolverRejected;
        checkpoint("resolver-return");
        auto after=player_rig::word(combat+0x7c);accepted=after>before?after-before:0;
    }__finally{memcpy(hand.hits,reinterpret_cast<void*>(combat+0x78),sizeof(hand.hits));memcpy(reinterpret_cast<void*>(combat+0x78),saved,sizeof(saved));}
    return accepted;
}
inline void closeOperation(Operation* op,bool opened){
    checkpoint("runtime-cleanup-enter");
    __try{if(!op->scope.close())fault("context cleanup failed");else if(opened)++contextsClosed;}
    __except(EXCEPTION_EXECUTE_HANDLER){fault("context cleanup exception");}
    checkpoint("runtime-cleanup-return");
}
inline unsigned runOperation(Operation* op,uintptr_t physics,HandState& hand,melee_native::HitArray& hits,const float* from,const float* to){
    unsigned accepted=0;bool opened=false;
    __try{__try{
        amalur::MeleeContextRecipe recipe{op->env.owner,source.asset,source.asset,source.epoch};
        if(op->scope.open(recipe)==amalur::MeleeScopeResult::Ready){
            opened=true;++contextsCreated;auto key=op->scope.key();auto runtime=op->scope.selected();
            checkpoint("context-ready");
            melee_native::Context c;c.owner=recipe.owner;c.key=key.key;c.flags=source.flags;c.talentIndex=runtime.index;c.subIndex=0;
            c.baseTalent=c.selectedTalent=op->backend.runtimeAddress();c.baseAsset=c.selectedAsset=source.asset;
            if(op->scope.current()&&player_rig::word(physics+0x2e4)==0)accepted=resolveScoped(physics,c,hand,hits,from,to);
            if(player_rig::word(physics+0x2e4))fault("owned context unexpectedly opened an animation window");
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){op->scope.quarantine();fault("contact execution exception; partial ownership quarantined");}
    }__finally{closeOperation(op,opened);}
    return accepted;
}
inline unsigned contactOperation(uintptr_t physics,HandState& hand,melee_native::HitArray& hits,const float* from,const float* to,float radius){
    ++progress.sweeps;
    if(!melee_native::gather(physics,source.owner,from,to,radius,hits))return 0;
    progress.queryHits+=hits.count;
    if(!hits.data||hits.count>hits.capacity||hits.count>4096){fault("invalid query hit array");return 0;}
    bool actorContact=false;
    for(unsigned i=0;i<hits.count;++i){
        auto record=reinterpret_cast<uintptr_t>(hits.data)+i*0x70;
        auto raw=player_rig::word(record+0x14),actor=amalur::meleeHitActor(raw);
        auto part=actor&&actor!=source.owner?melee_native::component(actor,1):0;
        if(actor==source.owner)++progress.selfHits;
        else if(part){++progress.actorHits;actorContact=true;}
        else ++progress.otherHits;
        static unsigned logged=0;static uint64_t logWindow{};const auto now=GetTickCount64();
        if(!logWindow||now-logWindow>=2000){logWindow=now;logged=0;}
        if(logged++<16)log("VR owned melee query target raw=%08x actor=%08x source=%08x actorPart=%08x from=%.2f,%.2f,%.2f to=%.2f,%.2f,%.2f\n",
            raw,actor,source.owner,unsigned(part),from[0],from[1],from[2],to[0],to[1],to[2]);
    }
    // Native resolution also rejects self and hits without active actor part1.
    // Do not let those contacts exhaust all four attempts before reaching an enemy.
    if(!actorContact)return 0;
    if(hand.attempts++>=4)return 0;
    traceContact=++traceSequence;checkpoint("actor-contact");
    Operation operation(physics);auto accepted=runOperation(&operation,physics,hand,hits,from,to);
    log("VR owned melee contact attack=%u weapon=%08x swing=%u queryHits=%u acceptedTargets=%u nativeWindows=%u created=%u closed=%u fault=%d\n",
        source.asset,source.weapon,hand.serial,hits.count,accepted,player_rig::word(physics+0x2e4),contextsCreated,contextsClosed,faulted.load());
    return accepted;
}
inline bool clearHits(melee_native::HitArray& hits){
    checkpoint("hit-vector-cleanup-enter");
    __try{hits.clear();}__except(EXCEPTION_EXECUTE_HANDLER){fault("hit vector cleanup exception");return false;}
    checkpoint("hit-vector-cleanup-return");
    return true;
}
inline int queryException(EXCEPTION_POINTERS* error,bool* recoverable){
    // Only pre-context failures can be rearmed. Any ambiguous ownership or
    // cleanup failure remains a hard fault requiring restart.
    *recoverable=traceContact==0&&contextsCreated==contextsClosed&&!faulted.load();
    log("VR owned melee query exception code=%08x address=%p preContext=%d owner=%08x armingKey=%08x\n",
        unsigned(error->ExceptionRecord->ExceptionCode),error->ExceptionRecord->ExceptionAddress,*recoverable,source.owner,lastArmingKey);
    if(!*recoverable)fault("query exception after context entry");
    return EXCEPTION_EXECUTE_HANDLER;
}
inline unsigned guardedContact(uintptr_t physics,HandState& hand,melee_native::HitArray& hits,const float* from,const float* to,float radius){
    unsigned result=0;bool recoverable=false;
    __try{__try{result=contactOperation(physics,hand,hits,from,to,radius);}
        __except(queryException(GetExceptionInformation(),&recoverable)){}
    }__finally{
        bool clean=clearHits(hits);checkpoint("contact-return");traceContact=0;
        if(recoverable&&clean&&!faulted.load()){
            querySuspended=true;suspendedArmingKey=lastArmingKey;motion_controls::meleeContextReady.store(false);
            log("VR owned melee query suspended with no owned context; make a fresh normal supported weapon attack after gameplay resumes to rearm\n");
        }
    }
    return result;
}
inline unsigned contact(uintptr_t physics,HandState& hand,const float* from,const float* to,float radius){
    melee_native::HitArray hits;return guardedContact(physics,hand,hits,from,to,radius);
}
inline void contacts(uintptr_t physics){
    if(!melee_probe::local(physics)||faulted.load())return;
    ++progress.updates;
    const auto equipment=equipped();
    auto actor=player_rig::word(physics+0x18),weapon=equipment.weapon;
    static uint64_t lastStateReport{};static uint32_t lastStateModel{},lastStateWeapon{};
    const auto stateNow=GetTickCount64();
    if(stateNow-lastStateReport>=2000||lastStateModel!=equipment.model||lastStateWeapon!=weapon){
        uint32_t visualModel,visualOwner,selection;uint64_t visualAt;
        AcquireSRWLockShared(&weapon_control::poseLock);visualModel=weapon_control::visualAsset;visualOwner=weapon_control::visualWeapon;
        selection=weapon_control::visualSelection;visualAt=weapon_control::visualTick;ReleaseSRWLockShared(&weapon_control::poseLock);
        lastStateReport=stateNow;lastStateModel=equipment.model;lastStateWeapon=weapon;
        const auto recipe=amalur::physicalMeleeRecipe(visualModel);
        log("VR physical selection tick=%llu owner=%08x equipped=%08x model=%u attack=%u visualWeapon=%08x visualModel=%u visualSelection=%u selected=%u visualAge=%llu source=%u reason=%s\n",
            stateNow,actor,weapon,equipment.model,equipment.attack,visualOwner,visualModel,selection,motion_controls::viewControls().selectedWeapon,
            visualAt&&stateNow>=visualAt?stateNow-visualAt:~uint64_t(0),source.asset,
            !recipe.attack?"unsupported-model":!weapon?"equipment-identity-unverified":"recipe-selected");
    }
    if(source.held&&(!source.current(actor,weapon)||source.asset!=equipment.attack)){if(!source.clear())fault("source release failed");log("VR owned melee source invalidated\n");}
    armEquipped(physics,actor,weapon);
    bool canContact=eligible(physics)&&source.current(actor,weapon)&&player_rig::word(physics+0x2e4)==0;
    // OR the observed blockers over each reporting interval: tracking, first
    // person, interface, arms, focus, weapon slot, source, native attack window.
    progress.blocked|=(!headTracking.load()?1u:0u)|(!firstPerson.load()?2u:0u)
        |(interfaceView.load()?4u:0u)|(!arm_rig::enabled.load()?8u:0u)
        |(!motion_controls::gameFocused()?16u:0u)|(motion_controls::viewControls().selectedWeapon!=0?32u:0u)
        |(!source.current(actor,weapon)?64u:0u)|(player_rig::word(physics+0x2e4)?128u:0u);
    motion_controls::meleeContextReady.store(canContact);
    if(!canContact){for(auto& h:hands){h.tick=0;h.consumed=true;}reportProgress();return;}
    const auto* profile=amalur::capturedContactProfile(equipment.model);
    if(!profile||!profile->count||!amalur::matchesPhysicalRecipe(equipment.model,source.asset,source.flags)){motion_controls::meleeContextReady.store(false);for(auto& h:hands){h.tick=0;h.consumed=true;}return;}
    ++progress.ready;
    mgs5vr::Pose poses[2];amalur::MeleeSwingEvent swings[2];uint64_t ticks[2],frame;uint32_t owner,model,selection;unsigned generation;
    AcquireSRWLockShared(&weapon_control::poseLock);
    for(unsigned i=0;i<2;++i){poses[i]=weapon_control::visualPoses[i];swings[i]=weapon_control::swingEvents[i];}
    frame=weapon_control::visualTick;owner=weapon_control::visualWeapon;model=weapon_control::visualAsset;selection=weapon_control::visualSelection;generation=weapon_control::generation;
    ticks[0]=weapon_control::tick;ticks[1]=weapon_control::leftTick;ReleaseSRWLockShared(&weapon_control::poseLock);
    auto now=GetTickCount64();
    for(unsigned i=0;i<2;++i){auto& h=hands[i];
        if(h.owner!=weapon||h.epoch!=source.epoch){
            if(h.hits[0].data)reinterpret_cast<melee_native::Free>(gameBase+0x6fe180)(h.hits[0].allocator,reinterpret_cast<void*>(h.hits[0].data));
            if(h.hits[1].data)reinterpret_cast<void(__cdecl*)(void*,unsigned,int)>(gameBase+0xb0a9c0)(reinterpret_cast<void*>(h.hits[1].data),h.hits[1].capacity,h.hits[1].allocator);
            h={};h.owner=weapon;h.epoch=source.epoch;h.consumed=true;
        }
        if(i>=equipment.hands||model!=equipment.model||selection!=0||owner!=weapon||!frame||frame>now||now-frame>=100||!ticks[i]||ticks[i]>now||now-ticks[i]>=100||!mgs5vr::valid(poses[i])){h.tick=0;h.consumed=true;continue;}
        ++progress.fresh[i];
        bool continuous=h.tick&&frame>h.tick&&frame-h.tick<=100&&h.generation==generation;
        if(frame==h.tick)continue;
        if(continuous)++progress.continuous[i];
        // A deliberate tracked swing opens one short contact window. An idle
        // overlap no longer rearms every250ms; contact still decides damage.
        bool sameGesture=swings[i].weapon==weapon&&swings[i].generation==generation;
        if(h.window.accept(swings[i].serial,swings[i].tick,now,continuous&&sameGesture)){
            ++progress.swings[i];h.serial=swings[i].serial;
            h.consumed=false;h.attempts=0;h.hits[0].count=h.hits[1].count=0;
            log("VR physical swing hand=%u serial=%u chain=%u window=450ms\n",i,h.serial,swings[i].chainStep);
        }
        if(!continuous||!h.window.active(now))h.consumed=true;
        auto travel=poses[i].position-h.previous.position;
        if(continuous&&mgs5vr::dot(travel,travel)<2500.f&&!h.consumed){
            ++progress.fast[i];
            for(unsigned s=0;s<profile->count&&!h.consumed&&h.attempts<4&&!faulted.load()&&!querySuspended;++s){
                auto from=amalur::contactCenter(*profile,s,h.previous);
                auto to=amalur::contactCenter(*profile,s,poses[i]);
                melee_debug::record(i,s,from,to,weapon);
                if(contact(physics,h,&from.x,&to.x,profile->radius))h.consumed=true;
            }
        }
        h.previous=poses[i];h.tick=frame;h.generation=generation;
    }
    reportProgress();
}
inline void feedback(uintptr_t physics){
    if(!melee_probe::local(physics)||!headTracking.load()||!firstPerson.load()||interfaceView.load()
        ||!motion_controls::gameFocused()||motion_controls::dialogueActive.load())return;
    amalur::MeleeSwingEvent events[2];uint32_t weapon;unsigned center;uint64_t frame;
    AcquireSRWLockShared(&weapon_control::poseLock);
    for(unsigned i=0;i<2;++i)events[i]=weapon_control::swingEvents[i];
    weapon=weapon_control::visualWeapon;center=weapon_control::generation;frame=weapon_control::visualTick;
    ReleaseSRWLockShared(&weapon_control::poseLock);
    const auto now=GetTickCount64();
    if(!frame||frame>now||now-frame>=100)return;
    // Observation only: the feedback inbox validates age/pose and deduplicates.
    // No captured model or event pointer is passed into a native playback call.
    for(auto& event:events)if(event.weapon==weapon&&event.generation==center){
        event.owner=player_rig::word(physics+0x18);melee_feedback::onSwing(event);
    }
}
inline void __fastcall update(void* self,void*){
    __try{if(melee_probe::local(reinterpret_cast<uintptr_t>(self))){DWORD empty=0;updateThread.compare_exchange_strong(empty,GetCurrentThreadId());
        if(updateThread.load()!=GetCurrentThreadId())fault("native update thread changed");}}
    __except(EXCEPTION_EXECUTE_HANDLER){fault("native update identity exception");}
    originalUpdate(self);if(inContact||faulted.load()||updateThread.load()!=GetCurrentThreadId())return;inContact=true;
    melee_recipe_capture::sample(reinterpret_cast<uintptr_t>(self));
    __try{feedback(reinterpret_cast<uintptr_t>(self));contacts(reinterpret_cast<uintptr_t>(self));}__except(EXCEPTION_EXECUTE_HANDLER){fault("physical contact update exception");}
    inContact=false;
}
inline void install(){
    if constexpr(!melee_native::customContactEnabled){return;}else{
    if(GetFileAttributesW(L"amalur-owned-melee.enable")==INVALID_FILE_ATTRIBUTES)return;
    if(!melee_native::initialize()||!melee_probe::original){log("VR owned melee prerequisites rejected\n");return;}
    auto u=reinterpret_cast<unsigned char*>(gameBase+0xba57b0);const unsigned char prefix[]{0x83,0xec,0x44,0xa1};
    if(memcmp(u,prefix,sizeof(prefix))||player_rig::word(reinterpret_cast<uintptr_t>(u)+4)!=gameBase+0x157713c)return;
    melee_lifetime_hooks::creationObserver=&melee_recipe_capture::created;
    if(!melee_lifetime_hooks::install())return;
    if(hook(u,reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&originalUpdate),"Owned physical dagger contact")){
        motion_controls::contactEnabled.store(true);log("VR owned melee enabled; automatic verified dagger/longsword/greatsword equipment arming; deliberate swing threshold=0.9m/s; contact window=450ms\n");}
    }
}
}
