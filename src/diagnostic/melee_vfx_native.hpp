#pragma once
#include "../tracking/melee_vfx_selection.hpp"
// Included inside melee_feedback after resolved. Native functions run only on
// the caller's game update thread. Hook callbacks change private ownership only.
namespace native_vfx {
using GroupCall=void(__thiscall*)(uint32_t*);
using ResetCall=void(__thiscall*)(void*);
using SetPose=void(__thiscall*)(void*,unsigned,const void*);
inline GroupCall originalCreate{},originalRelease{};
inline ResetCall originalReset{};
inline SetPose setPose{};
inline resolved::Resolve resolve{};
inline resolved::Attach attach{};
inline resolved::Schedule schedule{};
inline SRWLOCK ownershipLock=SRWLOCK_INIT;
struct Owned {uint32_t group{},spatial{},epoch{},flags{},token{};};
inline Owned owned[2];
inline uint32_t poolEpoch=1,nextToken=1;
inline bool ready{};inline std::atomic<bool> failed{false};
inline void invalidate(uint32_t index){
    AcquireSRWLockExclusive(&ownershipLock);
    for(auto& v:owned)if(v.group==index)v={};
    ReleaseSRWLockExclusive(&ownershipLock);
}
inline void __fastcall created(uint32_t* self,void*){originalCreate(self);invalidate(*self);}
inline void __fastcall released(uint32_t* self,void*){invalidate(*self);originalRelease(self);}
inline void __fastcall reset(void* self,void*){
    // 935460 frees the group's entire record vector. Pointer equality cannot
    // detect an in-place world reset, therefore invalidate BEFORE native free.
    AcquireSRWLockExclusive(&ownershipLock);++poolEpoch;for(auto& v:owned)v={};
    if(!poolEpoch){failed=true;poolEpoch=1;}
    ReleaseSRWLockExclusive(&ownershipLock);originalReset(self);
}
inline uint32_t epoch(){AcquireSRWLockShared(&ownershipLock);auto e=poolEpoch;ReleaseSRWLockShared(&ownershipLock);return e;}
inline uintptr_t word(uintptr_t address){return player_rig::word(address);}
inline uintptr_t resident(uintptr_t manager,uint32_t asset){
    if(!manager||asset<2||asset>=1000000)return 0;
    const auto states=word(manager+0x28),table=word(manager+0x18);
    if(!states||!table)return 0;
    const auto flags=*reinterpret_cast<const unsigned char*>(states+asset);
    return (flags&4)&&!(flags&16)?word(table+asset*4):0;
}
inline bool current(const amalur::MeleeVfxEpoch& e){
    return ready&&!failed&&e.poolGeneration==epoch()&&word(gameBase+0x15fdf60)==e.groups
        &&word(gameBase+0x15fdf5c)==e.instances&&word(gameBase+0x15fe010)==e.spatial;
}
inline uintptr_t groupRecord(const amalur::MeleeVfxEpoch& e,uint32_t group){
    const auto count=word(e.groups+8),table=word(e.groups+4);
    return table&&group&&group<count&&count<1048576?table+group*36:0;
}
inline Owned lookup(uint32_t token){
    AcquireSRWLockShared(&ownershipLock);Owned result{};
    for(const auto& v:owned)if(v.token==token){result=v;break;}
    ReleaseSRWLockShared(&ownershipLock);return result;
}
inline bool trailDefinition(uintptr_t definition){
    if(!definition)return false;
    const auto vt=word(definition);
    // 622F90 returns exact native type hash2670B8 (FXTrail). Checking the
    // function address avoids invoking an unknown virtual with side effects.
    return vt>=gameBase&&vt<gameBase+0x1600000&&word(vt+0x1c)==gameBase+0x622f90;
}
inline bool trailResource(uint32_t asset,unsigned& flags){
    const auto resource=resident(word(gameBase+0x15f4d50),asset);
    if(!resource||word(resource)!=gameBase+0x133dd94)return false;
    const auto count=word(resource+4),definitions=word(resource+8);
    if(!count||count>16||!definitions)return false;
    for(unsigned i=0;i<count;++i)if(!trailDefinition(word(definitions+i*4)))return false;
    flags=*reinterpret_cast<const unsigned char*>(resource+0x24)&3;return true;
}
// Full28-byte descriptor built by9DD2F0. +18 is the authored bone index,
// distinct from spatial handle+4. Asset resources are retained by the native
// scheduler/instance copy82B0D0, never by a borrowed captured pointer.
struct Descriptor {uint32_t asset{},spatial{},secondary{},tertiary{},owner{};float scale{1.f};int16_t bone{-1};uint16_t pad{};};
static_assert(sizeof(Descriptor)==28);
#include "../tracking/melee_vfx_attachment.hpp"
struct PoseSource {amalur::RigBone world{};uint32_t modelMask{};int16_t bone{-1};};
inline bool finiteScale(const amalur::RigBone& bone){
    float scales[3];memcpy(scales,bone.opaque,12);
    return std::isfinite(scales[0])&&std::isfinite(scales[1])&&std::isfinite(scales[2]);
}
inline bool poseSource(const amalur::MeleeSwingEvent& event,PoseSource& out){
    const auto effect=amalur::meleeVfxSelection(event);if(!effect.attachment)return false;
    mgs5vr::Pose tracked;uint64_t trackedTick;
    AcquireSRWLockShared(&weapon_control::poseLock);
    tracked=event.hand?weapon_control::desiredLeft:weapon_control::desired;
    trackedTick=event.hand?weapon_control::leftTick:weapon_control::tick;
    ReleaseSRWLockShared(&weapon_control::poseLock);
    if(!amalur::freshWeaponPose(tracked,trackedTick,GetTickCount64()))return false;
    const auto root=rig_probe::playerRoot();if(!root)return false;
    const auto n=word(root+0x28),children=word(root+0x24);if(n>32||!children)return false;
    uintptr_t selected{};
    for(unsigned i=0;i<n;++i){const auto object=weapon_control::fab(word(children+i*4));
        if(object&&word(object+0xf8)==event.weapon&&word(object+0xf0)==event.asset
            &&weapon_control::authoritativeSelectedWeapon(object)){
            if(selected&&selected!=object)return false;selected=object;
        }
    }
    if(!selected)return false;
    const auto skeleton=resident(word(gameBase+0x15fdf54),word(selected+0x198));
    if(!skeleton)return false;
    const auto count=word(skeleton+0x30),aliases=word(skeleton+0x34),indices=word(skeleton+0x40);
    if(!count||count>4096||!aliases||!indices)return false;
    int index=-1;
    for(unsigned i=0;i<count;++i)if(word(aliases+i*4)==effect.attachment){
        if(index!=-1)return false;index=*reinterpret_cast<const int16_t*>(indices+i*2);
    }
    const auto bones=word(selected+0x34),boneCount=word(selected+0x38);
    if(index<0||unsigned(index)>=boneCount||boneCount>64||!bones)return false;
    if(event.asset==1520&&trailAttachmentIndex(event.asset,event.hand,boneCount,count,
        reinterpret_cast<const uint32_t*>(aliases),reinterpret_cast<const int16_t*>(indices))!=index)return false;
    amalur::RigBone world{},local{};
    memcpy(&world,reinterpret_cast<const void*>(selected+0x124),48);
    memcpy(&local,reinterpret_cast<const void*>(bones+index*48),48);
    if(!mgs5vr::valid(amalur::bonePose(world))||!mgs5vr::valid(amalur::bonePose(local))
        ||!finiteScale(world)||!finiteScale(local))return false;
    // Native scale is handled by6C5770; do not approximate its flags or scale
    // rules with a quaternion-only composition. Both inputs are private copies.
    using Compose=void(__thiscall*)(void*,const void*,const void*);
    reinterpret_cast<Compose>(gameBase+0x6c5770)(&out.world,&world,&local);
    if(!mgs5vr::valid(amalur::bonePose(out.world))||!finiteScale(out.world))return false;
    out.bone=static_cast<int16_t>(index);out.modelMask=word(selected+0x1cc);return true;
}
struct Backend {
    bool owns(const amalur::MeleeVfxEpoch& e,uint32_t group){
        __try{const auto entry=lookup(group);return current(e)&&entry.group&&entry.epoch==e.poolGeneration&&groupRecord(e,entry.group);}
        __except(EXCEPTION_EXECUTE_HANDLER){failed=true;return false;}
    }
    bool create(const amalur::MeleeVfxEpoch& e,uint32_t& token){
        __try{
            if(!current(e)||!word(e.groups+8))return false;
            uint32_t group{};originalCreate(&group);
            if(!group||!groupRecord(e,group)){
                failed=true;log("VR native VFX fault allocation returned invalid group=%u; ownership quarantined\n",group);return false;
            }
            bool stored=false;AcquireSRWLockExclusive(&ownershipLock);
            if(poolEpoch==e.poolGeneration&&nextToken&&nextToken<0xffffffff)for(auto& v:owned)if(!v.group){
                token=nextToken++;v={group,0,poolEpoch,0,token};stored=true;break;
            }
            ReleaseSRWLockExclusive(&ownershipLock);
            if(!stored){originalRelease(&group);return false;}return true;
        }__except(EXCEPTION_EXECUTE_HANDLER){failed=true;return false;}
    }
    bool cancel(const amalur::MeleeVfxEpoch& e,uint32_t token){
        __try{if(!owns(e,token))return false;auto group=lookup(token).group;invalidate(group);originalRelease(&group);return group==0;}
        __except(EXCEPTION_EXECUTE_HANDLER){failed=true;return false;}
    }
    bool attach(const amalur::MeleeVfxEpoch& e,uint32_t group,const amalur::MeleeSwingEvent& event){
        __try{
            PoseSource pose;if(!owns(e,group)||!poseSource(event,pose))return false;
            unsigned flags{};uint32_t eventObject[7]{};eventObject[6]=0x0032dcd4;uint32_t asset{};
            resolve(eventObject,reinterpret_cast<uintptr_t>(&asset),event.weapon);
            if(!trailResource(asset,flags))return false;
            uint32_t spatial{},emptyBones[2]{};
            native_vfx::attach(reinterpret_cast<void*>(groupRecord(e,lookup(group).group)),reinterpret_cast<uintptr_t>(&spatial),0xffffffff,
                reinterpret_cast<uintptr_t>(&pose.world),reinterpret_cast<uintptr_t>(emptyBones),flags);
            if(!spatial)return false;
            bool stored=false;AcquireSRWLockExclusive(&ownershipLock);
            for(auto& v:owned)if(v.token==group&&v.epoch==e.poolGeneration){v.spatial=spatial;v.flags=flags;stored=true;break;}
            ReleaseSRWLockExclusive(&ownershipLock);return stored;
        }__except(EXCEPTION_EXECUTE_HANDLER){failed=true;return false;}
    }
    bool schedule(const amalur::MeleeVfxEpoch& e,uint32_t group,const amalur::MeleeSwingEvent& event,const amalur::MeleeVfxRecipe& recipe){
        __try{
            PoseSource pose;unsigned flags{};
            if(!owns(e,group)||!poseSource(event,pose)||!trailResource(recipe.asset,flags))return false;
            const auto binding=lookup(group);if(!binding.spatial)return false;
            Descriptor d;d.asset=recipe.asset;d.spatial=binding.spatial;d.owner=event.owner;d.bone=pose.bone;
            const uint32_t params[]{recipe.durationMs,0,0xffffffff,event.serial,pose.modelMask,0};
            native_vfx::schedule(reinterpret_cast<void*>(e.instances),binding.group,reinterpret_cast<uintptr_t>(&d),reinterpret_cast<uintptr_t>(params));
            return current(e);
        }__except(EXCEPTION_EXECUTE_HANDLER){failed=true;return false;}
    }
    bool pose(const amalur::MeleeVfxEpoch& e,uint32_t group,const amalur::MeleeSwingEvent& event){
        __try{PoseSource pose;if(!owns(e,group)||!poseSource(event,pose))return false;
            //8B1738 applies this exact scale/rotation flag adjustment for
            //cache flags1. Direct8B1780 is only a copy, so repeat it here.
            if(lookup(group).flags&1)pose.world.opaque[12]&=0xe3;
            setPose(reinterpret_cast<void*>(groupRecord(e,lookup(group).group)),0,&pose.world);return true;
        }__except(EXCEPTION_EXECUTE_HANDLER){failed=true;return false;}
    }
};

inline Backend backend;
inline amalur::MeleeVfxLifetime driver;
inline amalur::MeleeVfxEpoch observed;
inline uint64_t updatedAt{};
inline amalur::MeleeVfxPoseIdentity poseIdentity[2];
inline bool allowedNow{};
inline bool knownModel(uint32_t model){return model==2478||model==5457||model==1520||model==1250||amalur::knownHammerModel(model);}
inline void cancel(){
    __try{auto now=observed;now.groups=word(gameBase+0x15fdf60);now.instances=word(gameBase+0x15fdf5c);now.spatial=word(gameBase+0x15fe010);now.poolGeneration=epoch();
        driver.reset(backend,now);allowedNow=false;for(auto& pose:poseIdentity)pose.clear();
    }__except(EXCEPTION_EXECUTE_HANDLER){failed=true;}
}
inline void update(uint32_t owner,uint32_t weapon,uint32_t model,unsigned generation,bool allowed,uint64_t now){
    if(!ready)return;
    __try{
        observed={word(gameBase+0x15fdf60),word(gameBase+0x15fdf5c),word(gameBase+0x15fe010),owner,weapon,model,generation,epoch()};
        allowedNow=allowed&&knownModel(model)&&!failed;updatedAt=now;
        driver.update(backend,observed,allowedNow,now);
        // The spatial provider follows the currently remapped selected weapon's
        // exact named attachment, not a controller pose or historical pointer.
        for(unsigned hand=0;hand<2;++hand){
            const auto p=poseIdentity[hand].current(owner,weapon,model,generation,now);
            driver.pose(backend,hand,p,now);
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){failed=true;cancel();}
}
inline void onSwing(const amalur::MeleeSwingEvent& event){
    const auto effect=amalur::meleeVfxSelection(event);
    if(!ready||!allowedNow||failed||!effect.selector)return;
    __try{
        if(event.owner!=observed.owner||event.weapon!=observed.weapon||event.asset!=observed.model)return;
        uint32_t object[7]{};object[6]=effect.selector;uint32_t asset{};unsigned flags{};
        resolve(object,reinterpret_cast<uintptr_t>(&asset),event.weapon);
        if(!trailResource(asset,flags)){
            static unsigned rejected{};if(rejected++<12)log("VR native VFX rejected model=%u asset=%u reason=nonresident-or-not-only-FXTrail\n",event.asset,asset);return;
        }
        // Captured ordinary dagger=160ms and sword=200ms trail windows.
        // Other proven normal melee families use the same bounded VR stroke
        // duration adaptation; their effect resource still resolves natively.
        amalur::MeleeVfxRecipe recipe{asset,effect.durationMs,true,true,true,true};
        const bool queued=driver.start(backend,event,recipe,updatedAt);
        if(queued)poseIdentity[event.hand].committed(event);
        log("VR native VFX request model=%u hand=%u serial=%u asset=%u queued=%u fault=%u\n",event.asset,event.hand,event.serial,asset,unsigned(queued),unsigned(failed.load()||driver.faulted()));
    }__except(EXCEPTION_EXECUTE_HANDLER){failed=true;cancel();}
}
inline bool prefix(uintptr_t rva,const unsigned char* bytes,unsigned n){return !memcmp(reinterpret_cast<void*>(gameBase+rva),bytes,n);}
inline void install(){
    if(ready)return;
    __try{
        constexpr unsigned char createTail[]{0x83,0x78,0x18,0,0x57,0x8b,0xf9};
        constexpr unsigned char releaseTail[]{0x8b,0x50,0x04,0x53,0x56,0x8b,0xd9};
        constexpr unsigned char resetCode[]{0x56,0x8b,0xf1,0x8b,0x46,0x04,0x57};
        constexpr unsigned char poseCode[]{0x8b,0x49,0x10,0x8b,0x44,0x24,0x04,0x8b,0x54,0x24,0x08};
        constexpr unsigned char composeCode[]{0x83,0xec,0x20,0x53,0x55,0x33,0xd2,0x56,0x8b,0x74,0x24,0x30};
        constexpr unsigned char typeCode[]{0xb8,0xb8,0x70,0x26,0,0xc3};
        constexpr unsigned char resolveCode[]{0x83,0xec,0x14,0x8b,0x44,0x24,0x1c};
        constexpr unsigned char attachCode[]{0x53,0x55,0x56,0x57,0x8b,0x7c,0x24,0x14};
        constexpr unsigned char scheduleCode[]{0x83,0xec,0x5c,0x56,0x57,0x8b,0xf9};
        if(*reinterpret_cast<unsigned char*>(gameBase+0x93cea0)!=0xa1||word(gameBase+0x93cea1)!=gameBase+0x15fdf60
            ||*reinterpret_cast<unsigned char*>(gameBase+0x91bce0)!=0xa1||word(gameBase+0x91bce1)!=gameBase+0x15fdf60
            ||!prefix(0x93cea5,createTail,sizeof(createTail))||!prefix(0x91bce5,releaseTail,sizeof(releaseTail))
            ||!prefix(0x935460,resetCode,sizeof(resetCode))||!prefix(0x8b1780,poseCode,sizeof(poseCode))
            ||!prefix(0x6c5770,composeCode,sizeof(composeCode))||!prefix(0x622f90,typeCode,sizeof(typeCode)))return;
        resolve=resolved::originalResolve;attach=resolved::originalAttach;schedule=resolved::originalSchedule;
        if(!resolve&&prefix(0x9cfc80,resolveCode,sizeof(resolveCode)))resolve=reinterpret_cast<resolved::Resolve>(gameBase+0x9cfc80);
        if(!attach&&prefix(0x90baf0,attachCode,sizeof(attachCode)))attach=reinterpret_cast<resolved::Attach>(gameBase+0x90baf0);
        if(!schedule&&prefix(0x8f52f0,scheduleCode,sizeof(scheduleCode)))schedule=reinterpret_cast<resolved::Schedule>(gameBase+0x8f52f0);
        if(!resolve||!attach||!schedule)return;
        // Install AFTER optional resolved observers, using their validated
        // trampolines. Unknown patches without a trampoline fail closed above.
        if(!hook(reinterpret_cast<unsigned char*>(gameBase+0x935460),reinterpret_cast<void*>(&reset),reinterpret_cast<void**>(&originalReset),"Owned VFX group reset"))return;
        if(!hook(reinterpret_cast<unsigned char*>(gameBase+0x93cea0),reinterpret_cast<void*>(&created),reinterpret_cast<void**>(&originalCreate),"Owned VFX group reuse"))return;
        if(!hook(reinterpret_cast<unsigned char*>(gameBase+0x91bce0),reinterpret_cast<void*>(&released),reinterpret_cast<void**>(&originalRelease),"Owned VFX group release"))return;
        setPose=reinterpret_cast<SetPose>(gameBase+0x8b1780);ready=true;
        log("VR native VFX backend armed normal sword/dual-dagger/greatsword/hammer and verified sword release81; fresh FXTrail-only resources\n");
    }__except(EXCEPTION_EXECUTE_HANDLER){failed=true;}
}
}
