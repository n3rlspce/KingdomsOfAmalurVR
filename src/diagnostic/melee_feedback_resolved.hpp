#pragma once
// Included inside melee_feedback, after its Kind and observation definitions.
// All records own scalar/string copies only. No native object is cached/replayed.
namespace resolved {
struct Scope {unsigned trace{};uint32_t owner{},weapon{},model{},selector{},attachment{},event{},selection{};Kind kind{};
    unsigned submissions{},filters{},starts{},fxResolves{},fxAttachments{},fxSchedules{};};
inline thread_local Scope scope;
inline std::atomic<unsigned> sequence{0};
inline uint64_t budgetTick{};inline unsigned budgetCount{},budgetDropped{};
inline Scope begin(Kind kind,uintptr_t event,uintptr_t record){
    Scope result{};
    __try{
        if(player_rig::word(event)!=gameBase+vtables[static_cast<unsigned>(kind)])return result;
        const auto context=player_rig::word(record),player=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!context||!player)return result;
        if(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94)return result;
        const auto owner=player_rig::word(context+0x44);
        if(!owner||player_rig::word(player+0x1ec)!=owner||!player_rig::resolve(owner))return result;
        result.owner=owner;result.kind=kind;result.trace=++sequence;result.event=static_cast<uint32_t>(event);
        result.selection=motion_controls::viewControls().selectedWeapon;
        uint32_t fields[4]{};readFields(kind,event,fields);
        result.attachment=fields[0];result.selector=kind==Kind::WeaponFx?fields[1]:fields[3];
        // This is a fresh tracked-weapon observation, not the context model.
        const auto now=GetTickCount64();
        AcquireSRWLockShared(&weapon_control::poseLock);
        if(weapon_control::visualTick&&weapon_control::visualTick<=now&&now-weapon_control::visualTick<100){
            result.weapon=weapon_control::visualWeapon;result.model=weapon_control::visualAsset;}
        ReleaseSRWLockShared(&weapon_control::poseLock);
    }__except(EXCEPTION_EXECUTE_HANDLER){result={};}
    return result;
}
inline bool allow(){
    unsigned dropped=0;const auto now=GetTickCount64();
    AcquireSRWLockExclusive(&stateLock);
    if(!budgetTick||now<budgetTick||now-budgetTick>=1000){dropped=budgetDropped;budgetDropped=budgetCount=0;budgetTick=now;}
    const bool emit=budgetCount<128;if(emit)++budgetCount;else ++budgetDropped;
    ReleaseSRWLockExclusive(&stateLock);
    if(dropped)log("VR melee resolved capture dropped tick=%llu count=%u\n",now,dropped);
    return emit;
}
inline void failure(const char* operation){if(scope.trace&&allow())log("VR melee resolved capture rejected tick=%llu trace=%u owner=%08x operation=%s reason=unreadable-or-inconsistent\n",GetTickCount64(),scope.trace,scope.owner,operation);}
inline void finish(){
    __try{if(scope.trace&&scope.kind!=Kind::GameSound&&allow())log("VR melee resolved scope tick=%llu trace=%u kind=%s owner=%08x event=%08x trackedWeapon=%08x trackedModel=%u selection=%u submit=%u filter=%u start=%u resolve=%u attach=%u schedule=%u\n",
        GetTickCount64(),scope.trace,names[static_cast<unsigned>(scope.kind)],scope.owner,scope.event,scope.weapon,scope.model,scope.selection,
        scope.submissions,scope.filters,scope.starts,scope.fxResolves,scope.fxAttachments,scope.fxSchedules);}
    __except(EXCEPTION_EXECUTE_HANDLER){}
}
inline bool nameCopy(uintptr_t string,char (&text)[512]){
    // Game string: +0 points to the shared record; record+0 points to chars.
    // 6f14b0 reads this same indirection; +8 is its16-bit character count.
    const auto record=player_rig::word(string);
    const auto length=*reinterpret_cast<const uint16_t*>(string+8);
    if(!record||!length||length>=sizeof(text))return false;
    const auto chars=player_rig::word(record);if(!chars)return false;
    for(unsigned i=0;i<length;++i){const auto c=*reinterpret_cast<const unsigned char*>(chars+i);
        if(c<32||c>126)return false;text[i]=static_cast<char>(c);}
    text[length]=0;
    return !*reinterpret_cast<const char*>(chars+length)&&player_rig::word(string)==record
        &&player_rig::word(record)==chars&&*reinterpret_cast<const uint16_t*>(string+8)==length;
}
inline bool safeNameCopy(uintptr_t string,char (&text)[512]){
    __try{return nameCopy(string,text);}__except(EXCEPTION_EXECUTE_HANDLER){text[0]=0;return false;}
}
inline void audioRecord(const char* route,uintptr_t manager,uintptr_t descriptor){
    if(!scope.trace||scope.kind!=Kind::DerivedSound)return;
    __try{
        const auto system=player_rig::word(gameBase+0x15fde70);
        if(!system||manager!=system+0x43d4||player_rig::word(descriptor+0x20)!=scope.owner){failure("audio-owner-manager");return;}
        char name[512]{};const bool copied=safeNameCopy(descriptor,name);
        const auto selector=player_rig::word(descriptor+0xc),derived=player_rig::word(descriptor+0x10);
        scope.selector=selector;
        const auto index=player_rig::word(descriptor+0x28),stamp=player_rig::word(descriptor+0x24);
        uint32_t position[3]{};memcpy(position,reinterpret_cast<const void*>(descriptor+0x14),12);
        unsigned char flags[4]{};memcpy(flags,reinterpret_cast<const void*>(descriptor+0x34),4);
        uint32_t values[11]{};memcpy(values,reinterpret_cast<const void*>(descriptor+0xc),sizeof(values));
        if(!allow())return;
        log("VR melee resolved audio tick=%llu trace=%u route=%s owner=%08x trackedWeapon=%08x trackedModel=%u optionalStringValid=%d optionalStringStatus=%s optionalString=\"%s\" selector=%08x derived=%u index=%u stamp=%u positionBits=%08x,%08x,%08x flags=%02x,%02x,%02x,%02x\n",
            GetTickCount64(),scope.trace,route,scope.owner,scope.weapon,scope.model,copied,copied?"copied":"empty-long-unreadable-or-inconsistent",copied?name:"",selector,derived,index,stamp,
            position[0],position[1],position[2],flags[0],flags[1],flags[2],flags[3]);
        log("VR melee resolved audio values trace=%u offset=0c words=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
            scope.trace,values[0],values[1],values[2],values[3],values[4],values[5],values[6],values[7],values[8],values[9],values[10]);
    }__except(EXCEPTION_EXECUTE_HANDLER){failure("audio-descriptor");}
}
using Audio=uintptr_t(__thiscall*)(void*,uintptr_t);
inline Audio originalSubmit{},originalFilter{};
inline uintptr_t __fastcall submit(void* self,void*,uintptr_t descriptor){
    if(scope.trace)++scope.submissions;
    audioRecord("submit",reinterpret_cast<uintptr_t>(self),descriptor);
    return originalSubmit(self,descriptor);
}
inline uintptr_t __fastcall filter(void* self,void*,uintptr_t descriptor){
    if(scope.trace)++scope.filters;
    audioRecord("filter-only",reinterpret_cast<uintptr_t>(self),descriptor);
    const auto result=originalFilter(self,descriptor);
    if(scope.trace&&allow())log("VR melee resolved audio filter-result tick=%llu trace=%u result=%u\n",GetTickCount64(),scope.trace,unsigned(result));
    return result;
}

// Numeric object identities only, bounded lifetime. Dereference only the live
// object passed by the engine; never call native code using a cached pointer.
struct Pending {uintptr_t identity{};uint32_t selector{},generation{};uint64_t tick{};Scope source{};};
inline Pending pending[64]{};inline unsigned pendingNext{},pendingOverwrites{};
inline SRWLOCK pendingLock=SRWLOCK_INIT;
inline void remember(uintptr_t identity){
    Pending item{};
    if(scope.trace&&scope.kind==Kind::DerivedSound){__try{
        item.identity=identity;item.selector=scope.selector;item.generation=*reinterpret_cast<const unsigned char*>(identity+0x49);
        item.tick=GetTickCount64();item.source=scope;
    }__except(EXCEPTION_EXECUTE_HANDLER){failure("audio-object-generation");}}
    unsigned overwritten=0;
    AcquireSRWLockExclusive(&pendingLock);
    for(auto& old:pending)if(old.identity==identity)old={};
    if(item.identity){auto& slot=pending[pendingNext++%64];if(slot.identity&&item.tick-slot.tick<10000)overwritten=++pendingOverwrites;slot=item;}
    ReleaseSRWLockExclusive(&pendingLock);
    if(overwritten&&allow())log("VR melee deferred audio correlation overwritten total=%u\n",overwritten);
}
inline Scope deferred(uintptr_t identity){
    Pending found{};const auto now=GetTickCount64();
    AcquireSRWLockShared(&pendingLock);
    for(const auto& item:pending)if(item.identity==identity&&item.tick<=now&&now-item.tick<10000){found=item;break;}
    ReleaseSRWLockShared(&pendingLock);
    if(!found.identity)return {};
    __try{
        // Pool generation byte comes from allocator824f30 parent+5d; audio
        // subobject starts at parent+14. Reject reused pool slots/owner reload.
        if(*reinterpret_cast<const unsigned char*>(identity+0x49)!=found.generation)return {};
        const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!player||player_rig::word(player+0x1ec)!=found.source.owner)return {};
        if(player_rig::word(identity)!=found.selector)return {};
        return found.source;
    }__except(EXCEPTION_EXECUTE_HANDLER){return {};}
}
using Allocate=uintptr_t(__thiscall*)(void*,uintptr_t,uint32_t,uint32_t,uint32_t,uint32_t);
inline Allocate originalAllocate{};
inline uintptr_t __fastcall allocate(void* self,void*,uintptr_t output,uint32_t selector,uint32_t mode,uint32_t flag1,uint32_t flag2){
    const auto result=originalAllocate(self,output,selector,mode,flag1,flag2);
    if(result)remember(result); // Clear reused identities even for nonlocal sounds.
    if(scope.trace&&scope.kind==Kind::DerivedSound){__try{if(allow())log("VR melee resolved audio allocation tick=%llu trace=%u owner=%08x selector=%08x objectIdentity=%08x handle=%08x mode=%08x flags=%u,%u result=%s\n",
        GetTickCount64(),scope.trace,scope.owner,selector,unsigned(result),output?unsigned(player_rig::word(output)):0,mode,flag1&255,flag2&255,result?"allocated-awaiting-bank":"allocation-rejected");}
        __except(EXCEPTION_EXECUTE_HANDLER){failure("audio-allocation-output");}}
    return result;
}
using Initialize=uintptr_t(__thiscall*)(void*,uintptr_t,uintptr_t);
inline Initialize originalInitialize{};
inline uintptr_t __fastcall initialize(void* self,void*,uintptr_t selectorPointer,uintptr_t definition){
    const auto result=originalInitialize(self,selectorPointer,definition);
    const auto saved=scope;if(!scope.trace)scope=deferred(reinterpret_cast<uintptr_t>(self));
    if(scope.trace&&scope.kind==Kind::DerivedSound){__try{
        uint32_t words[18]{};memcpy(words,reinterpret_cast<const void*>(definition),sizeof(words));
        if(allow())log("VR melee resolved audio definition tick=%llu trace=%u owner=%08x selector=%08x success=%u words=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x meaning=raw-definition-not-owned\n",
            GetTickCount64(),scope.trace,scope.owner,unsigned(player_rig::word(reinterpret_cast<uintptr_t>(self))),unsigned(result&255),
            words[0],words[1],words[2],words[3],words[4],words[5],words[6],words[7],words[8],words[9],words[10],words[11],words[12],words[13],words[14],words[15],words[16],words[17]);
    }__except(EXCEPTION_EXECUTE_HANDLER){failure("audio-definition");}}
    scope=saved;return result;
}
using Start=uintptr_t(__thiscall*)(void*,uintptr_t,uintptr_t);
inline Start originalStart{};
inline uintptr_t __fastcall start(void* self,void*,uintptr_t a,uintptr_t b){
    const auto identity=reinterpret_cast<uintptr_t>(self);
    const auto saved=scope;if(!scope.trace)scope=deferred(identity);
    if(scope.trace)++scope.starts;
    uintptr_t result=0;
    __try{
        result=originalStart(self,a,b);
        if(scope.trace&&scope.kind==Kind::DerivedSound){
            __try{if(allow())log("VR melee resolved audio start tick=%llu trace=%u owner=%08x trackedWeapon=%08x trackedModel=%u selector=%08x success=%u instance=%u deferred=%d args=%08x,%08x\n",
                GetTickCount64(),scope.trace,scope.owner,scope.weapon,scope.model,unsigned(player_rig::word(identity)),unsigned(result&0xff),unsigned(player_rig::word(identity+8)),!saved.trace,unsigned(a),unsigned(b));}
            __except(EXCEPTION_EXECUTE_HANDLER){failure("audio-engine-result");}
        }
    }__finally{scope=saved;}
    return result;
}
using Resolve=uintptr_t(__thiscall*)(void*,uintptr_t,uint32_t);
inline Resolve originalResolve{};
inline uintptr_t __fastcall fx(void* self,void*,uintptr_t output,uint32_t weapon){
    if(scope.trace)++scope.fxResolves;
    const auto result=originalResolve(self,output,weapon);
    if(scope.trace&&scope.kind==Kind::WeaponFx){
        __try{
            if(result==output&&output){const auto asset=player_rig::word(output);
                uintptr_t definition=0;const auto manager=player_rig::word(gameBase+0x15f4d50);
                if(manager&&asset>=2&&asset<1000000){const auto flags=player_rig::word(manager+0x28),table=player_rig::word(manager+0x18);
                    if(flags&&table){const auto state=*reinterpret_cast<const unsigned char*>(flags+asset);
                        if((state&4)&&!(state&16))definition=player_rig::word(table+asset*4);}}
                if(allow())log("VR melee resolved fx tick=%llu trace=%u owner=%08x weapon=%08x trackedModel=%u selector=%08x attachment=%08x asset=%u resident=%d type=%08x\n",
                    GetTickCount64(),scope.trace,scope.owner,weapon,scope.model,scope.selector,scope.attachment,asset,definition!=0,
                    definition?unsigned(player_rig::word(definition)-gameBase):0);
            }else failure("fx-output-identity");
        }__except(EXCEPTION_EXECUTE_HANDLER){failure("fx-resolution");}
    }
    return result;
}

using Attach=uintptr_t(__thiscall*)(void*,uintptr_t,uint32_t,uintptr_t,uintptr_t,uint32_t);
inline Attach originalAttach{};
inline uintptr_t __fastcall attach(void* self,void*,uintptr_t output,uint32_t bone,uintptr_t pose,uintptr_t context,uint32_t flags){
    if(scope.trace)++scope.fxAttachments;
    const auto result=originalAttach(self,output,bone,pose,context,flags);
    if(scope.trace){__try{if(allow())log("VR melee resolved attachment tick=%llu trace=%u owner=%08x bone=%u flags=%u binding=%u outputMatch=%d poseIdentity=%08x contextIdentity=%08x\n",
        GetTickCount64(),scope.trace,scope.owner,bone,flags&255,output?unsigned(player_rig::word(output)):0,result==output,unsigned(pose),unsigned(context));}
        __except(EXCEPTION_EXECUTE_HANDLER){failure("fx-attachment");}}
    return result;
}
using Schedule=uintptr_t(__thiscall*)(void*,uint32_t,uintptr_t,uintptr_t);
inline Schedule originalSchedule{};
inline uintptr_t __fastcall schedule(void* self,void*,uint32_t resource,uintptr_t token,uintptr_t params){
    if(scope.trace){++scope.fxSchedules;__try{
        uint32_t values[6]{};memcpy(values,reinterpret_cast<const void*>(params),sizeof(values));
        if(allow())log("VR melee resolved schedule tick=%llu trace=%u owner=%08x binding=%u fxAsset=%u durationRaw=%08x elapsedRaw=%08x params=%08x,%08x,%08x,%08x,%08x,%08x lifecycle=schedule-only\n",
            GetTickCount64(),scope.trace,scope.owner,resource,unsigned(player_rig::word(token)),values[0],values[1],values[0],values[1],values[2],values[3],values[4],values[5]);
    }__except(EXCEPTION_EXECUTE_HANDLER){failure("fx-schedule-input");}}
    return originalSchedule(self,resource,token,params);
}

inline void install(){
    // Independent instruction-prefix guards; none of these calls is synthesized.
    struct Site {uintptr_t rva;unsigned count;unsigned char bytes[8];void* replacement;void** trampoline;};
    Site sites[]{
        {0x82fb30,4,{0x83,0xec,0x30,0xa1},reinterpret_cast<void*>(&submit),reinterpret_cast<void**>(&originalSubmit)},
        {0x82d700,3,{0x56,0x8b,0xf1},reinterpret_cast<void*>(&filter),reinterpret_cast<void**>(&originalFilter)},
        {0x823360,7,{0x56,0x8b,0xf1,0x83,0x3e,0,0x0f},reinterpret_cast<void*>(&start),reinterpret_cast<void**>(&originalStart)},
        {0x9cfc80,7,{0x83,0xec,0x14,0x8b,0x44,0x24,0x1c},reinterpret_cast<void*>(&fx),reinterpret_cast<void**>(&originalResolve)},
        {0x90baf0,8,{0x53,0x55,0x56,0x57,0x8b,0x7c,0x24,0x14},reinterpret_cast<void*>(&attach),reinterpret_cast<void**>(&originalAttach)},
        {0x8f52f0,7,{0x83,0xec,0x5c,0x56,0x57,0x8b,0xf9},reinterpret_cast<void*>(&schedule),reinterpret_cast<void**>(&originalSchedule)},
        {0x82b200,7,{0x8b,0x44,0x24,0x04,0x53,0x56,0x57},reinterpret_cast<void*>(&allocate),reinterpret_cast<void**>(&originalAllocate)},
        {0x825d60,6,{0x8b,0x44,0x24,0x04,0x8b,0x00},reinterpret_cast<void*>(&initialize),reinterpret_cast<void**>(&originalInitialize)}};
    unsigned installed=0,bit=1;
    for(const auto& site:sites){const auto currentBit=bit;bit<<=1;auto target=reinterpret_cast<unsigned char*>(gameBase+site.rva);
        if(memcmp(target,site.bytes,site.count)||(site.rva==0x82fb30&&player_rig::word(reinterpret_cast<uintptr_t>(target)+4)!=gameBase+0x157713c)){
            log("VR melee resolved feedback signature mismatch rva=%08x\n",unsigned(site.rva));continue;}
        if(hook(target,site.replacement,site.trampoline,"Resolved melee feedback observation"))installed|=currentBit;
    }
    log("VR melee resolved capture enabled mask=%02x expected=ff budget=128-per-second lifetimeCap=none playback=disabled lifecycle=schedule-only deferredAudio=64-slots-10s\n",installed);
}
}
