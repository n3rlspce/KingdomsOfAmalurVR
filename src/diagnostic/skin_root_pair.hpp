#pragma once
// Native render-publication pairing. All replacements are private draw copies.
namespace skin_root_pair {
constexpr unsigned maxBones=78, capacity=256;
struct Bones {
    amalur::LocomotionFrame locomotion;
    uintptr_t object{},owner{},root{},rootOwner{},buffer{};
    unsigned count{},center{},modes{}; uint64_t tick{};
    unsigned char world[48]{},data[64*48]{};
};
struct Palette {
    amalur::LocomotionFrame locomotion;
    uintptr_t object{},owner{},root{},rootOwner{},entry{},handle{},instance{};
    unsigned count{},center{},modes{};uint64_t tick{};
    unsigned char world[48]{},data[maxBones*48]{};
};
inline Bones bones[32];inline Palette palettes[capacity];inline unsigned cursor{};
inline SRWLOCK lock=SRWLOCK_INIT;
inline std::atomic<unsigned> captured{},matched{},rejected{},advanced{};
inline bool active(){return headTracking.load()&&!interfaceView.load()&&(firstPerson.load()||arm_rig::enabled.load());}
inline bool owned(uintptr_t object,uintptr_t root){
    if(!root||root!=rig_probe::playerRoot())return false;
    auto n=player_rig::word(root+0x28);if(n>32)return false;
    for(unsigned i=0;i<n;++i)
        if(weapon_control::fab(player_rig::word(player_rig::word(root+0x24)+4*i))==object)return true;
    return false;
}
inline void rememberUnsafe(uintptr_t object,uintptr_t root,bool solved,const amalur::LocomotionFrame& locomotion={}){
    Bones snapshot{};
    snapshot.object=object;snapshot.locomotion=locomotion;
    if(solved&&active()&&owned(object,root)){
        snapshot.count=player_rig::word(object+0x38);snapshot.buffer=player_rig::word(object+0x34);
        if(!snapshot.count||snapshot.count>64||!snapshot.buffer)return;
        snapshot.owner=player_rig::word(object+0xf8);snapshot.root=root;snapshot.rootOwner=player_rig::word(root+0xf8);
        snapshot.center=recenterGeneration.load();snapshot.modes=amalur::bodyDebug.read();snapshot.tick=GetTickCount64();
        memcpy(snapshot.world,reinterpret_cast<void*>(root+0x124),48);
        memcpy(snapshot.data,reinterpret_cast<void*>(snapshot.buffer),snapshot.count*48);
    }
    AcquireSRWLockExclusive(&lock);
    unsigned slot=32;
    for(unsigned i=0;i<32;++i)if(bones[i].object==object){slot=i;break;}
    if(slot==32&&snapshot.tick)for(unsigned i=0;i<32;++i)if(!bones[i].tick||snapshot.tick-bones[i].tick>250){slot=i;break;}
    if(slot<32)bones[slot]=snapshot;
    ReleaseSRWLockExclusive(&lock);
}
inline void remember(uintptr_t object,uintptr_t root,bool solved,const amalur::LocomotionFrame& locomotion={}){
    __try{rememberUnsafe(object,root,solved,locomotion);}__except(EXCEPTION_EXECUTE_HANDLER){}
}
// RVA8ae460 thiscall(entry, world, meshIndex, asset, bonesDescriptor, float).
using Publish=void(__thiscall*)(void*,uintptr_t,unsigned,uintptr_t,uintptr_t,float);
using Allocate=uintptr_t(__thiscall*)(void*,unsigned);
inline Publish originalPublish{};inline Allocate originalAllocate{},originalRigidAllocate{};
inline void* originalWorld{};
struct Pending {uintptr_t packet{};};
inline thread_local Pending* pending{};
inline thread_local unsigned diagnosticStage{};
inline std::atomic<unsigned> diagnosticCounts[24]{};
inline uintptr_t __fastcall allocate(void* self,void*,unsigned size){
    auto result=originalAllocate(self,size);
    if(pending&&size>=0x58&&size<=0x58+maxBones*48)pending->packet=result;
    return result;
}
inline std::atomic<unsigned> rigidCaptured{},rigidMatched{},rigidAdvanced{};
inline uintptr_t __fastcall allocateRigid(void* self,void*,unsigned size){
    auto result=originalRigidAllocate(self,size);
    if(pending&&size==0x54)pending->packet=result;
    return result;
}
inline bool prepareUnsafe(uintptr_t entry,unsigned mesh,uintptr_t asset,uintptr_t descriptor,Bones& copy){
    diagnosticStage=1;
    if(!active()||descriptor<0x34)return false;
    const auto object=descriptor-0x34,root=rig_probe::playerRoot();
    diagnosticStage=2;
    if(!owned(object,root)||mesh>=player_rig::word(object+0x18)||mesh>=64
        ||entry!=player_rig::word(object+0x14)+mesh*20)return false;
    AcquireSRWLockShared(&lock);
    bool found=false;
    for(auto& b:bones)if(b.object==object){copy=b;found=true;break;}
    ReleaseSRWLockShared(&lock);
    const auto now=GetTickCount64();
    const bool same=found&&copy.tick&&now>=copy.tick&&now-copy.tick<250&&copy.root==root
        &&copy.owner==player_rig::word(object+0xf8)&&copy.rootOwner==player_rig::word(root+0xf8)
        &&copy.center==recenterGeneration.load()&&copy.modes==unsigned(amalur::bodyDebug.read())
        &&copy.count==player_rig::word(descriptor+4)&&copy.buffer==player_rig::word(descriptor);
    diagnosticStage=3;
    if(!same||!asset)return false;
    // Native RVA8ae4b2 selects a 0x5c-byte mesh description. Its palette
    // builder (8ae674..8ae6bc) reads only these mapped bone indices.
    // Secondary cloth evaluation may change other bones after our snapshot.
    // Such unrelated changes must not exclude the entire torso.
    diagnosticStage=4;
    const auto meshTable=player_rig::word(asset+0x4c);if(!meshTable)return false;
    const auto model=meshTable+mesh*0x5c;
    if(!(*reinterpret_cast<unsigned char*>(model+0x50)&4)){
        // Rigid publication composes precisely this one bone with the root.
        const auto index=player_rig::word(model+0x34);
        diagnosticStage=5;
        if(index>=copy.count)return false;
        diagnosticStage=6;
        if(memcmp(copy.data+index*48,reinterpret_cast<void*>(copy.buffer+index*48),48))return false;
        diagnosticStage=0;return true;
    }
    const auto count=player_rig::word(model+0x20),mapping=player_rig::word(model+0x24);
    if(!count||count>maxBones||!mapping)return false;
    for(unsigned i=0;i<count;++i){
        const auto index=player_rig::word(mapping+i*0x34+0x30);
        if(index>=copy.count||memcmp(copy.data+index*48,reinterpret_cast<void*>(copy.buffer+index*48),48))return false;
    }
    return true;
}
inline bool prepare(uintptr_t entry,unsigned mesh,uintptr_t asset,uintptr_t descriptor,Bones& copy){
    __try{return prepareUnsafe(entry,mesh,asset,descriptor,copy);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool rebaseRigid(unsigned char* output,const unsigned char* published,const unsigned char* paired){
    amalur::RigBone from,to,result;
    memcpy(&from,published,48);memcpy(&to,paired,48);memcpy(&result,output,48);
    // Equal root scales cancel in pairedRoot * inverse(publishedRoot).
    if(memcmp(from.opaque,to.opaque,12)||!mgs5vr::valid(amalur::bonePose(from))
        ||!mgs5vr::valid(amalur::bonePose(to))||!mgs5vr::valid(amalur::bonePose(result)))return false;
    const auto delta=mgs5vr::compose(amalur::bonePose(to),mgs5vr::inverse(amalur::bonePose(from)));
    const auto moved=mgs5vr::compose(delta,amalur::bonePose(result));
    if(!mgs5vr::valid(moved))return false;
    result.position=moved.position;result.positionW=1.f;result.orientation=amalur::nativeQuaternion(moved.orientation);
    result.opaque[12]|=0x1e;memcpy(output,&result,48);return true;
}
// Native allocation RVA8d1ca0 uses the handle pool at manager+450.
// +458 is its capacity; +450 is the full generation-tagged handle table.
// +4ac belongs to a different structure and is not a render-handle limit.
inline uintptr_t renderInstance(uintptr_t manager,uintptr_t handle){
    if(!manager||!handle)return 0;
    const auto index=handle&0xffff,total=player_rig::word(manager+0x458);
    const auto generations=player_rig::word(manager+0x450),table=player_rig::word(manager+0x4a0);
    if(!total||total>65536||index>=total||!generations||!table
        ||player_rig::word(generations+index*4)!=handle)return 0;
    const auto header=player_rig::word(table+index*4);
    return header?player_rig::word(header+8):0;
}
inline void saveUnsafe(uintptr_t entry,uintptr_t packet,const Bones& b,uintptr_t publishedWorld=0){
    diagnosticStage=10;
    if(!packet)return;
    const auto type=*reinterpret_cast<const uint16_t*>(packet),size=*reinterpret_cast<const uint16_t*>(packet+2);
    const bool rigid=type==0x1c&&size==0x54;
    diagnosticStage=11;
    if(!rigid&&(type!=0x20||size<0x58))return;
    auto count=rigid?0:player_rig::word(packet+0x54),handle=player_rig::word(entry+8);
    diagnosticStage=12;
    if(!handle||player_rig::word(packet+4)!=handle
        ||(!rigid&&(!count||count>maxBones||size!=0x58+count*48)))return;
    Palette p{};p.locomotion=b.locomotion;p.object=b.object;p.owner=b.owner;p.root=b.root;p.rootOwner=b.rootOwner;
    p.entry=entry;p.handle=handle;p.count=count;p.center=b.center;p.modes=b.modes;p.tick=b.tick;
    const auto manager=player_rig::word(gameBase+0x15fdfb4);
    diagnosticStage=13;
    p.instance=renderInstance(manager,handle);if(!p.instance)return;
    if(rigid){
        diagnosticStage=15;
        if(!publishedWorld)return;
        // Keep the complete weapon attachment; only replace its parent frame.
        memcpy(p.world,reinterpret_cast<void*>(packet+8),45);
        memcpy(p.data,p.world,44); // Native copy excludes padding; flags are mutable caches.
        diagnosticStage=16;
        if(!rebaseRigid(p.world,reinterpret_cast<const unsigned char*>(publishedWorld),b.world))return;
    }else{memcpy(p.world,b.world,48);memcpy(p.data,reinterpret_cast<void*>(packet+0x58),count*48);}
    AcquireSRWLockExclusive(&lock);palettes[cursor++%capacity]=p;ReleaseSRWLockExclusive(&lock);++captured;
    if(rigid)++rigidCaptured;
    diagnosticStage=0;
}
inline void save(uintptr_t entry,uintptr_t packet,const Bones& b,uintptr_t publishedWorld=0){
    __try{saveUnsafe(entry,packet,b,publishedWorld);}__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline void auditRigid(uintptr_t entry,unsigned mesh,uintptr_t asset,uintptr_t descriptor,unsigned stage,uintptr_t packet){
    __try{
        if(!active()||!asset||descriptor<0x34||stage>=24)return;
        const auto object=descriptor-0x34;
        if(!owned(object,rig_probe::playerRoot())||mesh>=64)return;
        const auto table=player_rig::word(asset+0x4c);if(!table)return;
        const auto model=table+mesh*0x5c;if(*reinterpret_cast<unsigned char*>(model+0x50)&4)return;
        // Bound logging to three examples per reason, per process.
        if(diagnosticCounts[stage].fetch_add(1)>=3)return;
        const auto bone=player_rig::word(model+0x34);
        log("Rigid audit asset=%u mesh=%u stage=%u packet=%08x type=%u size=%u bone=%u entry=%08x descriptor=%08x\n",
            player_rig::word(object+0xf0),mesh,stage,packet,packet?*reinterpret_cast<const uint16_t*>(packet):0,
            packet?*reinterpret_cast<const uint16_t*>(packet+2):0,bone,entry,descriptor);
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline void __fastcall publish(void* self,void*,uintptr_t world,unsigned mesh,uintptr_t asset,uintptr_t descriptor,float value){
    Bones copy{};const bool eligible=prepare(reinterpret_cast<uintptr_t>(self),mesh,asset,descriptor,copy);
    const auto preparationStage=diagnosticStage;
    Pending local{};auto previous=pending;pending=eligible?&local:nullptr;
    originalPublish(self,world,mesh,asset,descriptor,value);
    pending=previous;if(eligible)save(reinterpret_cast<uintptr_t>(self),local.packet,copy,world);
    auditRigid(reinterpret_cast<uintptr_t>(self),mesh,asset,descriptor,eligible?diagnosticStage:preparationStage,local.packet);
}
inline uintptr_t selectUnsafe(uintptr_t instance,unsigned char* output,const float* vp=nullptr){
    if(!active())return instance;
    auto count=player_rig::word(instance+0x34),data=player_rig::word(instance+0x30);
    const bool rigid=!data&&!count;
    if(!rigid&&(!data||!count||count>maxBones))return instance;
    Palette selected{};bool found=false,ambiguous=false;
    // Copy the native palette first; never dereference game memory while holding the lock.
    unsigned char actual[maxBones*48];memcpy(actual,reinterpret_cast<void*>(rigid?instance:data),rigid?44:count*48);
    const auto manager=player_rig::word(gameBase+0x15fdfb4);
    // Resolve identities outside the lock; a snapshot keeps producer/render threads independent.
    const auto now=GetTickCount64();const auto center=recenterGeneration.load();const unsigned modes=amalur::bodyDebug.read();
    unsigned candidates[capacity],candidateCount=0;
    AcquireSRWLockShared(&lock);
    for(unsigned i=0;i<capacity;++i)if(palettes[i].instance==instance&&palettes[i].count==count)candidates[candidateCount++]=i;
    ReleaseSRWLockShared(&lock);
    for(unsigned j=0;j<candidateCount;++j){
        const auto i=candidates[j];Palette p;
        AcquireSRWLockShared(&lock);
        const bool candidate=palettes[i].instance==instance&&palettes[i].count==count;
        if(candidate)p=palettes[i];
        ReleaseSRWLockShared(&lock);
        if(!candidate)continue;
        if(!p.tick||now<p.tick||now-p.tick>=250||p.center!=center||p.modes!=modes||p.count!=count)continue;
        if(renderInstance(manager,p.handle)!=instance)continue;
        if(!owned(p.object,p.root)||player_rig::word(p.root+0xf8)!=p.rootOwner||player_rig::word(p.object+0xf8)!=p.owner)continue;
        auto entries=player_rig::word(p.object+0x14),n=player_rig::word(p.object+0x18);
        if(n>64||p.entry<entries||(p.entry-entries)%20||(p.entry-entries)/20>=n||player_rig::word(p.entry+8)!=p.handle)continue;
        if(memcmp(actual,p.data,rigid?44:count*48))continue;
        if(found&&memcmp(selected.world,p.world,48)){ambiguous=true;break;}
        if(found&&rigid&&(selected.locomotion.owner!=p.locomotion.owner||selected.locomotion.center!=p.locomotion.center
            ||memcmp(&selected.locomotion.pose,&p.locomotion.pose,sizeof(p.locomotion.pose)))){ambiguous=true;break;}
        selected=p;found=true;
    }
    if(!found||ambiguous){++rejected;return instance;}
    memcpy(output,reinterpret_cast<void*>(instance),128);memcpy(output,selected.world,48);++matched;
    if(rigid)++rigidMatched;
    amalur::LocomotionFrame drawn;
    if(render_pose::locomotionForVP(vp,drawn)){
        amalur::RigBone root;memcpy(&root,output,48);
        if(amalur::advanceLocomotion(root,selected.locomotion,drawn)){memcpy(output,&root,48);++advanced;if(rigid)++rigidAdvanced;}
    }
    return reinterpret_cast<uintptr_t>(output);
}
inline uintptr_t __cdecl select(uintptr_t instance,unsigned char* output,const float* vp=nullptr){
    __try{return selectUnsafe(instance,output,vp);}__except(EXCEPTION_EXECUTE_HANDLER){++rejected;return instance;}
}
// Native publisher is ECX/EDX + five caller-cleaned arguments, NOT fastcall.
// Preserve input registers and flags around selection and keep the clone alive
// until the original has produced world, normal and WVP matrices.
__declspec(naked) inline void world(){
    __asm {
        push ebp
        mov ebp,esp
        sub esp,128
        pushfd
        pushad
        lea eax,[ebp-128]
        push [ebp+8]
        push eax
        push edx
        call select
        add esp,12
        mov [esp+20],eax
        popad
        popfd
        push [ebp+24]
        push [ebp+20]
        push [ebp+16]
        push [ebp+12]
        push [ebp+8]
        call originalWorld
        add esp,20
        mov esp,ebp
        pop ebp
        ret
    }
}
inline void install(){
    auto pub=reinterpret_cast<unsigned char*>(gameBase+0x8ae460);
    auto alloc=reinterpret_cast<unsigned char*>(gameBase+0x880a90);
    auto rigidAlloc=reinterpret_cast<unsigned char*>(gameBase+0x880af0);
    auto draw=reinterpret_cast<unsigned char*>(gameBase+0x892150);
    const unsigned char a[]={0x81,0xec,0xd8,0,0,0,0xa1},b[]={0x8b,0x89,0xa8,1,0,0},c[]={0x81,0xec,0x0c,1,0,0,0xa1};
    if(memcmp(pub,a,sizeof(a))||memcmp(alloc,b,sizeof(b))||memcmp(draw,c,sizeof(c))
        ||player_rig::word(uintptr_t(pub)+7)!=gameBase+0x157713c||player_rig::word(uintptr_t(draw)+7)!=gameBase+0x157713c
        ||pub[0x40d]!=0xc2||pub[0x40e]!=0x14||draw[0x393]!=0xc3){log("Skin/root pairing: native signatures rejected\n");return;}
    hook(alloc,reinterpret_cast<void*>(&allocate),reinterpret_cast<void**>(&originalAllocate),"Skin packet provenance");
    if(!memcmp(rigidAlloc,b,sizeof(b))&&rigidAlloc[0x52]==0xb9
        &&player_rig::word(uintptr_t(rigidAlloc)+0x53)==0x1c&&rigidAlloc[0x5c]==0xc2&&rigidAlloc[0x5d]==4)
        hook(rigidAlloc,reinterpret_cast<void*>(&allocateRigid),reinterpret_cast<void**>(&originalRigidAllocate),"Rigid weapon packet provenance");
    else log("Rigid weapon pairing: signature rejected; body correction remains enabled\n");
    hook(pub,reinterpret_cast<void*>(&publish),reinterpret_cast<void**>(&originalPublish),"Player skin/root publication capture");
    hook(draw,reinterpret_cast<void*>(&world),&originalWorld,"Matched player skin/root draw correction");
}
}
