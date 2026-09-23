#include "../tracking/melee_stroke_targets.hpp"
#pragma once
#include "melee_native.hpp"
#include "melee_lifetime_hooks.hpp"
#include "melee_owned_source.hpp"
#include "../tracking/melee_hand_recipe.hpp"
#include "damage_capture.hpp"
#include "../tracking/melee_owned_backend.hpp"
#include "../tracking/melee_hit_identity.hpp"
#include "game_pause.hpp"
#include "melee_debug.hpp"
#include "../tracking/weapon_contact_profile.hpp"
#include "physical_hitstop.hpp"
#include "weapon_selection.hpp"
#include "back_sheath_dispatch.hpp"
#include "../tracking/melee_family_policy.hpp"
#include "../tracking/melee_query_recovery.hpp"
namespace melee_contact {
using Update=void(__thiscall*)(void*);
inline Update originalUpdate{};
inline std::atomic<DWORD> updateThread{0};
inline std::atomic<bool> faulted{false};
inline bool querySuspended{};
inline amalur::MeleeQueryRecovery queryRecovery;
inline uint32_t lastArmingKey{},suspendedArmingKey{};
inline thread_local bool inContact=false;
inline melee_owned_source::Lease sources[2];
inline uint64_t nextEquipmentProbe{};
// Captured basic dagger199/longsword50/greatsword417 use flags0; hammer16 uses flags1.
// Retain only the exact model pairing and resident/script-validated definition.
// This does not execute an animation, synthesize input, or clear fault latches.
inline void armEquipped(uintptr_t physics,uint32_t actor,uint32_t weapon,unsigned side);
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
    unsigned separationRejected{},damageGateRejected{},commitRejected{};
};
inline Progress progress;
inline void reportProgress(){
    auto now=GetTickCount64();if(now-progress.reportTick<2000)return;
    if(sources[0].held||sources[1].held)log("VR owned melee progress blocked=%03x updates=%u ready=%u fresh=%u,%u continuous=%u,%u swings=%u,%u fast=%u,%u sweeps=%u queryHits=%u created=%u closed=%u actorHits=%u selfHits=%u otherHits=%u resolverCalls=%u resolverRejected=%u\n",
        progress.blocked,progress.updates,progress.ready,progress.fresh[0],progress.fresh[1],progress.continuous[0],progress.continuous[1],
        progress.swings[0],progress.swings[1],progress.fast[0],progress.fast[1],progress.sweeps,progress.queryHits,contextsCreated,contextsClosed,
        progress.actorHits,progress.selfHits,progress.otherHits,progress.resolverCalls,progress.resolverRejected);
    if(progress.actorHits)log("VR physical contact gates tick=%llu actorHits=%u separationRejected=%u damageGateRejected=%u commitRejected=%u\n",
        now,progress.actorHits,progress.separationRejected,progress.damageGateRejected,progress.commitRejected);
    progress={};progress.reportTick=now;
}
inline void fault(const char* reason){
    if(!faulted.exchange(true))log("VR owned melee FAULT: %s; physical contacts disabled until restart\n",reason);
    motion_controls::contactEnabled.store(false);motion_controls::meleeContextReady.store(false);
}
struct Equipped {uint32_t weapon{},model{},attack{};unsigned hands{},flags{},serial{},generation{};};
inline bool nativeDamageWeaponMatches(uint32_t actor,uint32_t weapon){
    __try{
        const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
        return player&&player_rig::word(player)==gameBase+0x1359f14&&player_rig::word(player+0x1ec)==actor
            &&amalur::currentPhysicalDamageWeapon(weapon,player_rig::word(player+0x350),player_rig::word(player+0x354));
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline Equipped equipped(unsigned side=0){
    if(side>=2)return {};
    uint32_t publishedOwner,publishedModel,publishedSelection;uint64_t publishedTick;
    amalur::MeleeSwingEvent strike;unsigned publishedGeneration;
    AcquireSRWLockShared(&weapon_control::poseLock);
    publishedOwner=weapon_control::visualWeapon;publishedModel=weapon_control::visualAsset;
    publishedSelection=weapon_control::visualSelection;publishedTick=weapon_control::visualTick;
    strike=weapon_control::contactEvent(side);publishedGeneration=weapon_control::generation;
    ReleaseSRWLockShared(&weapon_control::poseLock);
    if(!amalur::currentPhysicalPublication(publishedOwner,publishedModel,publishedSelection,
        motion_controls::viewControls().selectedWeapon,publishedTick,GetTickCount64()))return {};
    auto root=rig_probe::playerRoot();if(!root)return {};
    auto n=player_rig::word(root+0x28);if(n>32)return {};Equipped result{};
    for(unsigned i=0;i<n;++i){auto obj=weapon_control::fab(player_rig::word(player_rig::word(root+0x24)+i*4));if(!obj)continue;
        const auto model=player_rig::word(obj+0xf0);
        if(model!=publishedModel||player_rig::word(obj+0xf8)!=publishedOwner)continue;
        if(!weapon_control::authoritativeSelectedWeapon(obj))continue;
        const auto recipe=amalur::physicalMeleeRecipe(model);if(!recipe.attack)continue;
        const bool matches=model==1520?(weapon_control::isPlayerDaggers(obj)&&weapon_control::isOnlyActivePlayerWeapon(obj)):weapon_control::capturedHeldKind(obj)!=amalur::HeldWeaponKind::None;
        if(!matches)continue;
        auto owner=player_rig::word(obj+0xf8);if(result.weapon&&result.weapon!=owner)return {};
        result={owner,model,recipe.attack,recipe.hands,recipe.flags};
        const auto now=GetTickCount64();
        if(amalur::currentHandRecipe(strike,side,owner,model,publishedGeneration,now)){
            result.attack=strike.attackAsset;result.flags=strike.attackFlags;
            result.serial=strike.serial;result.generation=strike.generation;
        }
    }
    return result;
}
inline bool eligible(uintptr_t physics,bool recovering=false){
    if(weapon_control::weaponSheathed.load()||weapon_control::backGripClaimed.load())return false;
    return !faulted.load()&&(!querySuspended||recovering)&&motion_controls::contactEnabled.load()&&melee_probe::local(physics)
        &&headTracking.load()&&firstPerson.load()&&!interfaceView.load()&&arm_rig::enabled.load()
        &&motion_controls::gameFocused()&&!motion_controls::explicitSpellActive(GetTickCount64())&&motion_controls::viewControls().selectedWeapon<=1;
}
inline void armEquipped(uintptr_t physics,uint32_t actor,uint32_t weapon,unsigned side){
    auto& source=sources[side];
    if(source.held||!actor||!weapon||!melee_native::ready||!eligible(physics)
        ||updateThread.load()!=GetCurrentThreadId()||player_rig::word(physics+0x2e4))return;
    const auto equipment=equipped(side);if(equipment.weapon!=weapon||side>=equipment.hands||!equipment.attack)return;
    auto now=GetTickCount64();nextEquipmentProbe=now+1000;
    auto resources=player_rig::word(gameBase+0x15f4dfc);
    auto definition=melee_owned_source::resident(resources,equipment.attack);
    if(const auto reason=melee_owned_source::assetRejection(definition,true,equipment.attack)){
        static uint64_t report{};static uint32_t lastModel{};if(now>=report||lastModel!=equipment.model){report=now+2000;lastModel=equipment.model;
            log("VR physical recipe rejected tick=%llu reason=%s owner=%08x weapon=%08x model=%u attack=%u selection=%u\n",now,reason,actor,weapon,equipment.model,equipment.attack,motion_controls::viewControls().selectedWeapon);}
        return;
    }
    const bool retained=amalur::knownLongswordModel(equipment.model)?source.acquireLongsword(actor,weapon,equipment.attack,definition,equipment.flags)
        :source.acquireFamily(equipment.model,actor,weapon,equipment.attack,definition,equipment.flags);
    if(!retained){
        fault("verified equipment source could not be retained");return;
    }
    log("VR physical equipment armed without trigger owner=%08x weapon=%08x model=%u asset=%u flags=%08x epoch=%llu\n",
        actor,weapon,equipment.model,equipment.attack,source.flags,source.epoch);
}
inline void capture(uintptr_t physics,uintptr_t event){
    auto& source=sources[0];
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
    Access calls;uintptr_t physics;uint32_t owner,weapon;uint64_t epoch;unsigned side;melee_owned_source::Lease& source;
    explicit NativeEnvironment(uintptr_t p,unsigned hand):physics(p),owner(player_rig::word(p+0x18)),weapon(sources[hand].weapon),epoch(sources[hand].epoch),side(hand),source(sources[hand]){
        melee_lifetime_hooks::connect(calls);calls.onNativeUpdateThread=updateThread.load()==GetCurrentThreadId();
    }
    bool supported(const amalur::MeleeContextRecipe& r)const{
        return calls.onNativeUpdateThread&&!faulted.load()&&r.owner==owner&&r.baseAsset==source.asset
            &&r.equipmentGeneration==epoch&&source.epoch==epoch&&source.current(owner,weapon)
            &&equipped(side).weapon==weapon&&equipped(side).attack==source.asset&&eligible(physics)&&player_rig::word(physics+0x2e4)==0;
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
    explicit Operation(uintptr_t physics,unsigned side):env(physics,side),backend(env),scope(backend){}
};
struct Dedup {uintptr_t data{};uint32_t count{},capacity{};int16_t allocator{0x27},flags{-1};};
static_assert(sizeof(Dedup)==16);
struct HandState {unsigned side{};mgs5vr::Pose previous{};uint64_t tick{},retryAt{},epoch{};unsigned generation{},serial{},attempts{};uint32_t owner{},attack{},flags{};bool consumed{};amalur::MeleeStrokeTargets targets;amalur::MeleeSwingWindow window;Dedup hits[2];};
inline HandState hands[2];
inline amalur::LongswordSeparation swordSeparation[2];
inline uint32_t separationWeapon{};inline unsigned separationGeneration{};
inline bool swordQuery{},swordDamage{},queryFrameComplete{};inline unsigned queryHand{};
inline amalur::MeleeSwingEvent contactSnapshot;
inline uint64_t contactFrame{};
// The native resolver obtains its weapon through player virtual +44 (C0CBD0),
// independently of the tracked visual attachment. Observe that exact getter's
// fields; never rewrite attack state or call a synthetic attack to repair it.
inline void reportDamageSource(uint32_t owner,uint32_t visualWeapon,const char* phase){
    __try{
        const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!player||player_rig::word(player)!=gameBase+0x1359f14||player_rig::word(player+0x1ec)!=owner)return;
        const auto primary=player_rig::word(player+0x350),overrideWeapon=player_rig::word(player+0x354);
        const auto chosen=overrideWeapon?overrideWeapon:primary;
        log("VR physical damage source tick=%llu phase=%s owner=%08x visual=%08x native=%08x primary=%08x override=%08x pending=%08x matches=%d\n",
            GetTickCount64(),phase,owner,visualWeapon,chosen,primary,overrideWeapon,player_rig::word(player+0x200),chosen==visualWeapon);
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline bool commitContactStroke(){
    if(queryHand>=2)return false;
    const auto& source=sources[queryHand];
    AcquireSRWLockExclusive(&weapon_control::poseLock);
    const auto live=weapon_control::contactEvent(queryHand);
    const bool match=weapon_control::contactReady(queryHand)&&amalur::sameMeleeContact(contactSnapshot,live,
        weapon_control::visualWeapon,weapon_control::visualAsset,weapon_control::generation,queryHand,
        contactFrame,weapon_control::visualTick,source.asset,source.flags);
    const bool committed=match&&weapon_control::commitPhysicalLocked(queryHand,contactSnapshot,contactFrame,GetTickCount64());
    ReleaseSRWLockExclusive(&weapon_control::poseLock);
    return committed;
}
inline unsigned resolveScoped(uintptr_t physics,const melee_native::Context& c,HandState& hand,melee_native::HitArray& hits,const float* from,const float* to){
    auto& source=sources[hand.side];
    if(!nativeDamageWeaponMatches(c.owner,source.weapon)){++progress.resolverRejected;return 0;}
    auto combat=melee_native::component(c.owner,0);if(!combat)return 0;
    Dedup saved[2];memcpy(saved,reinterpret_cast<void*>(combat+0x78),sizeof(saved));
    for(unsigned i=0;i<2;++i)if(!hand.hits[i].data){hand.hits[i].allocator=saved[i].allocator;hand.hits[i].flags=saved[i].flags;}
    unsigned accepted=0;
    const auto priorFeedback=melee_feedback::resolved::scope;
    auto& impactFeedback=melee_feedback::resolved::scope;
    impactFeedback={};impactFeedback.trace=++melee_feedback::resolved::sequence;
    impactFeedback.owner=c.owner;impactFeedback.weapon=source.weapon;
    impactFeedback.model=equipped(hand.side).model;impactFeedback.kind=melee_feedback::Kind::Fx;
    log("VR impact capture begin trace=%u attack=%u flags=%u weapon=%08x swing=%u\n",impactFeedback.trace,source.asset,c.flags,source.weapon,hand.serial);
    reportDamageSource(c.owner,source.weapon,"contact");
    const auto previousHitstop=physical_hitstop::beginLocalPhysical(c.owner,player_rig::word(physics+0x18),eligible(physics));
    const auto hitstopBefore=physical_hitstop::suppressedCalls;
    __try{amalur::beginHandDedup(reinterpret_cast<Dedup*>(combat+0x78),hand.hits,saved);auto before=hand.hits[0].count;
        checkpoint("resolver-enter");
        if(melee_native::resolveHits(physics,c,hits,from,to))++progress.resolverCalls;else ++progress.resolverRejected;
        checkpoint("resolver-return");
        auto after=player_rig::word(combat+0x7c);accepted=after>before?after-before:0;
    }__finally{
        physical_hitstop::endLocalPhysical(previousHitstop);
        log("VR physical hitstop attack=%u suppressed=%llu\n",source.asset,physical_hitstop::suppressedCalls-hitstopBefore);
        melee_feedback::resolved::finish();
        log("VR impact capture end trace=%u attack=%u accepted=%u schedules=%u resolves=%u\n",impactFeedback.trace,source.asset,accepted,impactFeedback.fxSchedules,impactFeedback.fxResolves);
        melee_feedback::resolved::scope=priorFeedback;
        amalur::endHandDedup(reinterpret_cast<Dedup*>(combat+0x78),hand.hits,saved);
    }
    return accepted;
}
inline void closeOperation(Operation* op,bool opened){
    checkpoint("runtime-cleanup-enter");
    __try{if(!op->scope.close())fault("context cleanup failed");else if(opened)++contextsClosed;}
    __except(EXCEPTION_EXECUTE_HANDLER){fault("context cleanup exception");}
    checkpoint("runtime-cleanup-return");
}
inline unsigned runOperation(Operation* op,uintptr_t physics,HandState& hand,melee_native::HitArray& hits,const float* from,const float* to){
    auto& source=sources[hand.side];
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
    auto& source=sources[hand.side];
    ++progress.sweeps;
    bool gathered=false;
    const bool found=melee_native::gather(physics,source.owner,from,to,radius,hits,&gathered);
    if(swordQuery&&!gathered)queryFrameComplete=false;
    if(!found)return 0;
    progress.queryHits+=hits.count;
    if(!hits.data||hits.count>hits.capacity||hits.count>4096){fault("invalid query hit array");return 0;}
    bool actorContact=false;unsigned retained=0;uint32_t targets[64]{};unsigned targetCount=0;
    for(unsigned i=0;i<hits.count;++i){
        auto record=reinterpret_cast<uintptr_t>(hits.data)+i*0x70;
        auto raw=player_rig::word(record+0x14),actor=amalur::meleeHitActor(raw);
        auto part=actor&&actor!=source.owner?melee_native::component(actor,1):0;
        if(actor==source.owner)++progress.selfHits;
        else if(part){
            ++progress.actorHits;
            const bool separated=!swordQuery||swordSeparation[queryHand].observe(actor,GetTickCount64());
            if(!separated)++progress.separationRejected;
            else if(swordQuery&&!swordDamage)++progress.damageGateRejected;
            if(separated&&(!swordQuery||swordDamage)&&(!swordQuery||(hand.targets.available(actor)&&targetCount<64-hand.targets.count))){
                actorContact=true;
                if(swordQuery){
                    bool listed=false;for(unsigned n=0;n<targetCount;++n)listed=listed||targets[n]==actor;
                    if(!listed&&targetCount<64)targets[targetCount++]=actor;
                    // Exchange whole records: clear() destroys capacity, so a
                    // copying compaction would duplicate owned native references.
                    if(retained!=i){unsigned char saved[0x70];auto target=reinterpret_cast<unsigned char*>(hits.data)+retained*0x70;
                        memcpy(saved,target,0x70);memcpy(target,reinterpret_cast<void*>(record),0x70);memcpy(reinterpret_cast<void*>(record),saved,0x70);}
                    ++retained;
                }
            }
        }
        else ++progress.otherHits;
        static unsigned logged=0;static uint64_t logWindow{};const auto now=GetTickCount64();
        if(!logWindow||now-logWindow>=2000){logWindow=now;logged=0;}
        if(logged++<16)log("VR owned melee query target raw=%08x actor=%08x source=%08x actorPart=%08x from=%.2f,%.2f,%.2f to=%.2f,%.2f,%.2f\n",
            raw,actor,source.owner,unsigned(part),from[0],from[1],from[2],to[0],to[1],to[2]);
    }
    // Native resolution also rejects self and hits without active actor part1.
    // Do not let those contacts exhaust all four attempts before reaching an enemy.
    if(!actorContact)return 0;
    if(swordQuery){
        // Gather is still run for idle/consumed blades to observe real overlap.
        hits.count=retained;
        if(!swordDamage||!commitContactStroke()){++progress.commitRejected;return 0;}
    }
    // One attempt per distinct actor per stroke, shared across every sphere and
    // frame. The persistent native dedup arrays remain the authority inside
    // native resolution (including any native secondary effects).
    if(swordQuery){
        for(unsigned n=0;n<targetCount;++n)hand.targets.claim(targets[n]);
        ++hand.attempts;
    }else if(hand.attempts++>=4)return 0;
    traceContact=++traceSequence;checkpoint("actor-contact");
    Operation operation(physics,hand.side);auto accepted=runOperation(&operation,physics,hand,hits,from,to);
    // Separation is conservatively marked for ATTEMPTED actors. The aggregate
    // native dedup growth does not identify which actors received damage.
    if(swordQuery)for(unsigned n=0;n<targetCount;++n)swordSeparation[queryHand].hit(targets[n],GetTickCount64());
    if(swordQuery)log("VR cleave batch hand=%u serial=%u attemptedActors=%u strokeActors=%u nativeDedupAdded=%u\n",queryHand,hand.serial,targetCount,hand.targets.count,accepted);
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
inline int queryException(EXCEPTION_POINTERS* error,bool* recoverable,unsigned side){
    const auto& source=sources[side];
    // Only pre-context failures can be rearmed. Any ambiguous ownership or
    // cleanup failure remains a hard fault requiring restart.
    *recoverable=traceContact==0&&contextsCreated==contextsClosed&&!faulted.load();
    log("VR owned melee query exception code=%08x address=%p preContext=%d owner=%08x armingKey=%08x\n",
        unsigned(error->ExceptionRecord->ExceptionCode),error->ExceptionRecord->ExceptionAddress,*recoverable,source.owner,lastArmingKey);
    const auto* registers=error->ContextRecord;
    const auto* exception=error->ExceptionRecord;
    log("VR owned melee query fault detail physics=%08x world=%08x layer=%u ESI=%08x EDI=%08x ECX=%08x accessKind=%u accessAddress=%08x\n",
        unsigned(melee_native::queryPhysics),unsigned(melee_native::queryWorld),melee_native::queryLayer,
        unsigned(registers->Esi),unsigned(registers->Edi),unsigned(registers->Ecx),
        exception->NumberParameters>0?unsigned(exception->ExceptionInformation[0]):~0u,
        exception->NumberParameters>1?unsigned(exception->ExceptionInformation[1]):0u);
    if(!*recoverable)fault("query exception after context entry");
    return EXCEPTION_EXECUTE_HANDLER;
}
inline unsigned guardedContact(uintptr_t physics,HandState& hand,melee_native::HitArray& hits,const float* from,const float* to,float radius){
    unsigned result=0;bool recoverable=false;
    __try{__try{result=contactOperation(physics,hand,hits,from,to,radius);}
        __except(queryException(GetExceptionInformation(),&recoverable,hand.side)){}
    }__finally{
        bool clean=clearHits(hits);checkpoint("contact-return");traceContact=0;
        if(recoverable&&clean&&!faulted.load()){
            querySuspended=true;suspendedArmingKey=lastArmingKey;motion_controls::meleeContextReady.store(false);
            queryRecovery.suspend(GetTickCount64());
            log("VR owned melee query suspended with no owned context; bounded recovery awaits stable tracked gameplay\n");
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
    const auto baseEquipment=equipped();
    auto actor=player_rig::word(physics+0x18),weapon=baseEquipment.weapon;
    if(querySuspended){
        const auto now=GetTickCount64();mgs5vr::Pose grip;uint64_t trackedAt;
        AcquireSRWLockShared(&weapon_control::poseLock);grip=weapon_control::desired;trackedAt=weapon_control::tick;ReleaseSRWLockShared(&weapon_control::poseLock);
        const bool stable=eligible(physics,true)&&melee_native::queryWorldReady(physics)&&trackedCameraAvailable.load()&&!motion_controls::dialogueActive.load()
            &&game_pause::sample(true)==0&&baseEquipment.attack&&nativeDamageWeaponMatches(actor,weapon)
            &&!player_rig::word(physics+0x2e4)&&contextsCreated==contextsClosed
            &&amalur::freshWeaponPose(grip,trackedAt,now);
        if(queryRecovery.claim(physics,actor,weapon,now,stable)){
            querySuspended=false;
            for(auto& separation:swordSeparation)separation.interrupt();
            for(auto& hand:hands){hand.tick=0;hand.consumed=true;}
            log("VR owned melee query recovered after stable gameplay without native attack owner=%08x weapon=%08x\n",actor,weapon);
        }
    }
    static uint64_t lastStateReport{};static uint32_t lastStateModel{},lastStateWeapon{};
    const auto stateNow=GetTickCount64();
    if(stateNow-lastStateReport>=2000||lastStateModel!=baseEquipment.model||lastStateWeapon!=weapon){
        uint32_t visualModel,visualOwner,selection;uint64_t visualAt;
        AcquireSRWLockShared(&weapon_control::poseLock);visualModel=weapon_control::visualAsset;visualOwner=weapon_control::visualWeapon;
        selection=weapon_control::visualSelection;visualAt=weapon_control::visualTick;ReleaseSRWLockShared(&weapon_control::poseLock);
        lastStateReport=stateNow;lastStateModel=baseEquipment.model;lastStateWeapon=weapon;
        reportDamageSource(actor,visualOwner,"selection");
        const auto recipe=amalur::physicalMeleeRecipe(visualModel);
        log("VR physical selection tick=%llu owner=%08x equipped=%08x model=%u attack=%u visualWeapon=%08x visualModel=%u visualSelection=%u selected=%u visualAge=%llu source=%u reason=%s\n",
            stateNow,actor,weapon,baseEquipment.model,baseEquipment.attack,visualOwner,visualModel,selection,motion_controls::viewControls().selectedWeapon,
            visualAt&&stateNow>=visualAt?stateNow-visualAt:~uint64_t(0),sources[0].asset,
            !recipe.attack?"unsupported-model":!weapon?"equipment-identity-unverified":"recipe-selected");
    }
    bool sourceReady=false;
    for(unsigned side=0;side<2;++side){
        auto& retained=sources[side];const auto handEquipment=equipped(side);
        if(retained.held&&(side>=handEquipment.hands||!retained.current(actor,weapon)||retained.asset!=handEquipment.attack)){
            if(!retained.clear())fault("source release failed");
        }
        armEquipped(physics,actor,weapon,side);
        sourceReady=sourceReady||(side<handEquipment.hands&&retained.current(actor,weapon));
    }
    bool canContact=eligible(physics)&&sourceReady&&nativeDamageWeaponMatches(actor,weapon)&&player_rig::word(physics+0x2e4)==0;
    // OR the observed blockers over each reporting interval: tracking, first
    // person, interface, arms, focus, weapon slot, source, native attack window,
    // and native damage-weapon mismatch (0x100).
    progress.blocked|=(!headTracking.load()?1u:0u)|(!firstPerson.load()?2u:0u)
        |(interfaceView.load()?4u:0u)|(!arm_rig::enabled.load()?8u:0u)
        |(!motion_controls::gameFocused()?16u:0u)|(motion_controls::viewControls().selectedWeapon>1?32u:0u)
        |(!sourceReady?64u:0u)|(player_rig::word(physics+0x2e4)?128u:0u)
        |(!nativeDamageWeaponMatches(actor,weapon)?256u:0u);
    motion_controls::meleeContextReady.store(canContact);
    if(!canContact){for(auto& separation:swordSeparation)separation.interrupt();for(auto& h:hands){h.tick=0;h.consumed=true;}reportProgress();return;}
    const auto* profile=amalur::capturedContactProfile(baseEquipment.model);
    if(!profile||!profile->count){for(auto& separation:swordSeparation)separation.interrupt();motion_controls::meleeContextReady.store(false);for(auto& h:hands){h.tick=0;h.consumed=true;}return;}
    ++progress.ready;
    bool swordReady[2]{};float sampleScale=0;
    mgs5vr::Pose poses[2];amalur::MeleeSwingEvent swings[2];uint64_t ticks[2],frame;uint32_t owner,model,selection;unsigned generation;
    AcquireSRWLockShared(&weapon_control::poseLock);
    for(unsigned i=0;i<2;++i){poses[i]=weapon_control::visualPoses[i];swings[i]=weapon_control::contactEvent(i);swordReady[i]=weapon_control::contactReady(i);}
    sampleScale=weapon_control::worldScale;
    frame=weapon_control::visualTick;owner=weapon_control::visualWeapon;model=weapon_control::visualAsset;selection=weapon_control::visualSelection;generation=weapon_control::generation;
    ticks[0]=weapon_control::tick;ticks[1]=weapon_control::leftTick;ReleaseSRWLockShared(&weapon_control::poseLock);
    auto now=GetTickCount64();
    const bool sword=amalur::physicalMeleeRecipe(baseEquipment.model).attack!=0;
    if(separationWeapon!=weapon||separationGeneration!=generation){for(auto& separation:swordSeparation)separation={};separationWeapon=weapon;separationGeneration=generation;}
    for(unsigned i=0;i<2;++i){auto& h=hands[i];h.side=i;
        auto& source=sources[i];const auto equipment=equipped(i);
        const bool recipeMatches=amalur::knownLongswordModel(equipment.model)?amalur::supportedLongswordDamage(source.asset,source.flags)
            :amalur::supportedNativeFamilyAttack(equipment.model,source.asset,source.flags);
        if(i>=equipment.hands||!source.current(actor,weapon)||!recipeMatches){swordSeparation[i].interrupt();h.tick=0;h.consumed=true;continue;}
        if(h.owner!=weapon||h.epoch!=source.epoch){
            // Changing the retained recipe is not a tracking discontinuity.
            // Keep the previous pose only for the same freshly tracked weapon.
            const bool sameWeapon=h.owner==weapon;
            const auto previous=h.previous;const auto previousTick=h.tick;const auto previousGeneration=h.generation;
            const auto previousWindow=h.window;
            const bool sameStrike=sameWeapon&&h.attack==source.asset&&h.flags==source.flags
                &&h.serial==swings[i].serial&&h.generation==swings[i].generation
                &&swings[i].weapon==weapon&&swings[i].asset==model&&swings[i].hand==i
                &&swings[i].attackAsset==source.asset&&swings[i].attackFlags==source.flags;
            const auto oldSerial=h.serial,oldAttempts=h.attempts;const bool oldConsumed=h.consumed;
            const auto oldTargets=h.targets;Dedup oldHits[2];memcpy(oldHits,h.hits,sizeof(oldHits));
            if(!sameStrike&&h.hits[0].data)reinterpret_cast<melee_native::Free>(gameBase+0x6fe180)(h.hits[0].allocator,reinterpret_cast<void*>(h.hits[0].data));
            if(!sameStrike&&h.hits[1].data)reinterpret_cast<void(__cdecl*)(void*,unsigned,int)>(gameBase+0xb0a9c0)(reinterpret_cast<void*>(h.hits[1].data),h.hits[1].capacity,h.hits[1].allocator);
            h={};h.side=i;h.owner=weapon;h.epoch=source.epoch;h.consumed=true;
            if(sameWeapon){h.previous=previous;h.tick=previousTick;h.generation=previousGeneration;h.window=previousWindow;}
            h.attack=source.asset;h.flags=source.flags;
            if(sameStrike){h.serial=oldSerial;h.attempts=oldAttempts;h.consumed=oldConsumed;h.targets=oldTargets;memcpy(h.hits,oldHits,sizeof(oldHits));}
        }
        if(i>=equipment.hands||model!=equipment.model||selection>1||selection!=motion_controls::viewControls().selectedWeapon||owner!=weapon||!frame||frame>now||now-frame>=100||!ticks[i]||ticks[i]>now||now-ticks[i]>=100||!mgs5vr::valid(poses[i])){swordSeparation[i].interrupt();h.tick=0;h.consumed=true;continue;}
        ++progress.fresh[i];
        bool continuous=h.tick&&frame>h.tick&&frame-h.tick<=100&&h.generation==generation;
        if(frame==h.tick)continue;
        if(continuous)++progress.continuous[i];
        // One candidate per deliberate stroke; contact is independently gated.
        // Native recipe identity and per-hand consumption remain transactional.
        bool sameGesture=amalur::sameMeleeContact(swings[i],swings[i],weapon,model,generation,i,frame,frame,source.asset,source.flags);
        if(amalur::knownLongswordModel(equipment.model)||equipment.model==1250||equipment.model==1323||equipment.model==1520)sameGesture=sameGesture&&(equipment.model==1520||i==0)&&equipment.serial==swings[i].serial
            &&equipment.generation==swings[i].generation&&equipment.attack==swings[i].attackAsset
            &&equipment.flags==swings[i].attackFlags;
        const bool newStroke=sword?(continuous&&swings[i].serial&&h.serial!=swings[i].serial):(sameGesture&&h.window.accept(swings[i].serial,swings[i].tick,now,continuous));
        if(sameGesture&&newStroke){
            ++progress.swings[i];h.serial=swings[i].serial;
            h.attack=source.asset;h.flags=source.flags;
            h.consumed=false;h.attempts=0;h.targets={};h.hits[0].count=h.hits[1].count=0;
            log("VR physical stroke hand=%u serial=%u chain=%u contactDriven=%d\n",i,h.serial,swings[i].chainStep,sword);
        }
        if(!continuous||(!sword&&!h.window.active(now))||!sameGesture||h.serial!=swings[i].serial
            ||h.attack!=source.asset||h.flags!=source.flags)h.consumed=true;
        auto travel=poses[i].position-h.previous.position;
        if(continuous&&mgs5vr::dot(travel,travel)<2500.f&&(sword||!h.consumed)){
            ++progress.fast[i];
            swordQuery=sword;queryHand=i;queryFrameComplete=true;contactSnapshot=swings[i];contactFrame=frame;
            swordSeparation[i].beginFrame(frame,swings[i].serial);unsigned scanned=0;
            for(unsigned segment=0;segment<profile->count&&(sword||(!h.consumed&&h.attempts<4))&&!faulted.load()&&!querySuspended;++segment){
                auto from=amalur::contactCenter(*profile,segment,h.previous);
                auto to=amalur::contactCenter(*profile,segment,poses[i]);
                const auto axis=mgs5vr::rotate(poses[i].orientation,{0,0,1});
                swordDamage=sword&&swordReady[i]&&sameGesture&&!h.consumed&&h.targets.count<64
                    &&amalur::meleeContactSpeed(model,to-from,axis,frame-h.tick,sampleScale);
                melee_debug::record(i,segment,from,to,weapon);
                const auto accepted=contact(physics,h,&from.x,&to.x,profile->radius);
                // Native multi-runtime/heavy effects retain their existing
                // one-contact ownership. Ordinary direct strokes may cleave.
                if(accepted&&(!sword||swings[i].heavy))h.consumed=true;
                ++scanned;
            }
            swordSeparation[i].endFrame(queryFrameComplete&&scanned==profile->count&&!faulted.load()&&!querySuspended);
            swordQuery=swordDamage=false;
        }else swordSeparation[i].interrupt();
        h.previous=poses[i];h.tick=frame;h.generation=generation;
    }
    reportProgress();
}
inline void feedback(uintptr_t physics){
    if(!melee_probe::local(physics))return;
    amalur::MeleeSwingEvent events[2];uint32_t weapon,model,selection;unsigned center;uint64_t frame;
    AcquireSRWLockShared(&weapon_control::poseLock);
    for(unsigned i=0;i<2;++i)events[i]=weapon_control::swingEvents[i];
    weapon=weapon_control::visualWeapon;center=weapon_control::generation;frame=weapon_control::visualTick;
    model=weapon_control::visualAsset;selection=weapon_control::visualSelection;
    ReleaseSRWLockShared(&weapon_control::poseLock);
    const auto now=GetTickCount64();
    const bool allowed=!weapon_control::weaponSheathed.load()&&!weapon_control::backGripClaimed.load()&&headTracking.load()&&firstPerson.load()&&!interfaceView.load()
        &&motion_controls::gameFocused()&&!motion_controls::explicitSpellActive(now)&&!motion_controls::dialogueActive.load()&&arm_rig::enabled.load()
        &&motion_controls::contactEnabled.load()&&frame&&frame<=now&&now-frame<100
        &&selection<=1&&selection==motion_controls::viewControls().selectedWeapon;
    melee_feedback::longsword_audio::update(player_rig::word(physics+0x18),weapon,model,center,allowed);
    melee_feedback::native_vfx::update(player_rig::word(physics+0x18),weapon,model,center,allowed,now);
    weapon_control::physicalActor.store(allowed?player_rig::word(physics+0x18):0);
    if(!allowed)return;
    // Inbox deduplicates gestures; longsword audio owns independent one-shots.
    for(auto& event:events)if(event.weapon==weapon&&event.generation==center){
        event.owner=player_rig::word(physics+0x18);melee_feedback::onSwing(event);
    }
}
inline void __fastcall update(void* self,void*){
    __try{if(melee_probe::local(reinterpret_cast<uintptr_t>(self))){DWORD empty=0;updateThread.compare_exchange_strong(empty,GetCurrentThreadId());
        if(updateThread.load()!=GetCurrentThreadId())fault("native update thread changed");}}
    __except(EXCEPTION_EXECUTE_HANDLER){fault("native update identity exception");}
    originalUpdate(self);if(inContact||updateThread.load()!=GetCurrentThreadId())return;
    weapon_selection::sample(reinterpret_cast<uintptr_t>(self));
    back_sheath_dispatch::sample(reinterpret_cast<uintptr_t>(self));
    weapon_control::recoverCapturedVisibility();
    if(faulted.load()){melee_feedback::longsword_audio::cancel();melee_feedback::native_vfx::cancel();return;}inContact=true;
    melee_recipe_capture::sample(reinterpret_cast<uintptr_t>(self));
    __try{feedback(reinterpret_cast<uintptr_t>(self));contacts(reinterpret_cast<uintptr_t>(self));}__except(EXCEPTION_EXECUTE_HANDLER){for(auto& separation:swordSeparation)separation.interrupt();fault("physical contact update exception");}
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
    log("VR physical hitstop hook=%s\n",physical_hitstop::install()?"enabled":"signature-rejected");
    if(hook(u,reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&originalUpdate),"Owned physical dagger contact")){
        motion_controls::contactEnabled.store(true);log("VR owned melee enabled; verified dagger/longsword/greatsword/hammer; per-hand peak/contact strokes; native resident damage recipes\n");}
    }
}
}
