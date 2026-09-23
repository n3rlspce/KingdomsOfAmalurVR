#include "../tracking/head_mesh.hpp"
#pragma once
// Temporary first-person workaround: hide only the local player's appearance
// and armor attachments. The player rig and weapon attachment remain active.
namespace body_visibility {
inline std::atomic<bool> enabled{false};
inline bool previousWholeBody{};
// Validate the whole skeleton rather than four distal limb IDs: starter skin
// can contain head/upper arms/calves while hands and feet are separate meshes.
inline bool skeleton(uintptr_t object,const uint32_t*& ids,const int16_t*& parents,unsigned& count){
    count=player_rig::word(object+0x38);if(!count||count>128)return false;
    auto manager=player_rig::word(gameBase+0x15fdf54),assetId=player_rig::word(object+0xf0);
    if(!manager||assetId<2||assetId>=100000)return false;
    auto flags=*reinterpret_cast<unsigned char*>(player_rig::word(manager+0x28)+assetId);
    if(!(flags&4)||(flags&0x10))return false;
    auto asset=player_rig::word(player_rig::word(manager+0x18)+assetId*4),blob=player_rig::word(asset+0x1c);
    if(player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=count)return false;
    auto idOffset=player_rig::word(blob+0x20),parentOffset=player_rig::word(blob+0x1c);
    if(!idOffset||idOffset>65536||!parentOffset||parentOffset>65536)return false;
    ids=reinterpret_cast<const uint32_t*>(blob+0x20+idOffset);
    parents=reinterpret_cast<const int16_t*>(blob+0x1c+parentOffset);return true;
}
inline bool headMesh(uintptr_t object,uintptr_t root){
    const uint32_t *rootIds{},*meshIds{};const int16_t *rootParents{},*meshParents{};unsigned rootCount{},meshCount{};
    return skeleton(root,rootIds,rootParents,rootCount)&&skeleton(object,meshIds,meshParents,meshCount)
        &&amalur::headOnlyPalette(rootIds,rootParents,rootCount,meshIds,meshCount);
}
// Bounded visibility evidence; no visibility mutations in these observers.
inline void observeHide(uintptr_t object){
    static unsigned records=0;if(records>=256)return;
    __try {
        auto owner=player_rig::word(object+0xf8),entity=player_rig::resolve(owner);
        if(!player_rig::part(entity,12,owner,0x13563e4)&&!player_rig::part(entity,40,owner,0x1356bec))return;
        ++records;log("Body visibility native-hide object=%08x owner=%08x asset=%u flags=%08x bones=%u\n",unsigned(object),unsigned(owner),unsigned(player_rig::word(object+0xf0)),unsigned(player_rig::word(object+0x1d0)),unsigned(player_rig::word(object+0x38)));
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline void audit(bool gameplayView){
    static ULONGLONG next=0;static unsigned records=0;auto now=GetTickCount64();if(now<next||records>=80)return;next=now+5000;
    __try {
        auto p=reinterpret_cast<uintptr_t>(player_rig::player.load());if(!p)return;
        auto owner=player_rig::word(p+0x1ec),entity=player_rig::resolve(owner);
        auto rendering=player_rig::part(entity,7,owner,0x13560e4);if(!rendering)return;
        auto root=weapon_control::fab(player_rig::word(rendering+0x9c));if(!root||player_rig::word(root+0xf8)!=owner)return;
        auto count=player_rig::word(root+0x28);if(count>32)return;++records;
        log("Body visibility audit root=%08x owner=%08x children=%u gameplay=%u normal=%u native=%u first=%u tracked=%u whole=%u\n",unsigned(root),unsigned(owner),unsigned(count),unsigned(gameplayView),unsigned(amalur::playMode.normal()),unsigned(amalur::playMode.nativeBody()),unsigned(firstPerson.load()),unsigned(trackedCameraAvailable.load()),unsigned(enabled.load()));
        for(unsigned i=0;i<count;++i){
            auto child=weapon_control::fab(player_rig::word(player_rig::word(root+0x24)+i*4));if(!child)continue;
            const uint32_t* ids{};const int16_t* parents{};unsigned n{};bool valid=skeleton(child,ids,parents,n);
            log("Body visibility mesh object=%08x owner=%08x asset=%u flags=%08x bones=%u layout=%u headOnly=%u\n",unsigned(child),unsigned(player_rig::word(child+0xf8)),unsigned(player_rig::word(child+0xf0)),unsigned(player_rig::word(child+0x1d0)),unsigned(player_rig::word(child+0x38)),unsigned(valid),unsigned(headMesh(child,root)));
            if(valid&&records<=3)for(unsigned j=0;j<n;++j)log("Body visibility joint object=%08x index=%u id=%08x parent=%d\n",unsigned(child),j,ids[j],int(parents[j]));
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){log("Body visibility audit unavailable\n");}
}
using Visibility=void(__thiscall*)(void*);
inline Visibility hide{},show{};
struct Entry {
    uint32_t index{},owner{},parentIndex{},parentOwner{};
    bool wasHidden{};
};
inline Entry saved[128];inline unsigned savedCount{};
inline uint32_t currentOwner{};
inline bool sameNode(const Entry& a,const Entry& b){return a.index==b.index&&a.owner==b.owner;}
inline bool sameLink(const Entry& a,const Entry& b){
    return sameNode(a,b)&&a.parentIndex==b.parentIndex&&a.parentOwner==b.parentOwner;
}
// Build a fresh tree each frame. A saved parent must not prevent discovery of
// newly attached hair/face meshes. Reject cycles/shared-parent ambiguity before
// calling the game's recursive visibility functions.
inline bool capture(uintptr_t object,Entry* entries,unsigned& size,unsigned depth=0,
    uint32_t parentIndex=0,uint32_t parentOwner=0){
    if(!object||depth>8)return false;
    Entry e{player_rig::word(object+0x194),player_rig::word(object+0xf8),
        parentIndex,parentOwner,(player_rig::word(object+0x1d0)&4)!=0};
    for(unsigned i=0;i<size;++i)if(entries[i].index==e.index)return sameLink(entries[i],e);
    if(size>=128)return false;
    entries[size++]=e;
    auto count=player_rig::word(object+0x28);if(count>32)return false;
    for(unsigned i=0;i<count;++i){auto child=weapon_control::fab(player_rig::word(player_rig::word(object+0x24)+i*4));
        if(child&&!capture(child,entries,size,depth+1,e.index,e.owner))return false;}
    return true;
}
inline void applyOriginal(const Entry* entries,unsigned count){
    // Parents precede children; restore descendants after recursive parent calls.
    for(unsigned i=0;i<count;++i){auto e=entries[i];auto p=weapon_control::fab(e.index);
        if(p&&player_rig::word(p+0xf8)==e.owner)(e.wasHidden?hide:show)(reinterpret_cast<void*>(p));}
}
inline void restore(){
    applyOriginal(saved,savedCount);savedCount=0;currentOwner=0;
}
inline void update(bool gameplayView,bool finisherView=false){
    audit(gameplayView);
    if(!hide||!show)return;
    __try {
        if(!finisherView&&!amalur::playMode.normal()&&amalur::playMode.nativeBody()){if(savedCount)restore();return;}
        const bool normal=!finisherView&&amalur::playMode.normal();
        bool active=(gameplayView||finisherView)&&firstPerson.load()&&headTracking.load()&&trackedCameraAvailable.load();
        // A native finisher owns body/arms animation; hide only verified head meshes at its eye camera.
        bool wholeBody=!finisherView&&!normal&&enabled.load();
        if(previousWholeBody!=wholeBody){if(savedCount)restore();previousWholeBody=wholeBody;}
        auto p=reinterpret_cast<uintptr_t>(player_rig::player.load());
        bool validPlayer=p&&(player_rig::word(p)==gameBase+0x1359f14||player_rig::word(p)==gameBase+0x1359e94);
        auto owner=validPlayer?player_rig::word(p+0x1ec):0;
        auto entity=owner?player_rig::resolve(owner):0;
        if(!active&&savedCount)restore();
        if((!active&&!normal)||!entity)return;
        if(savedCount&&currentOwner!=owner)restore();
        auto rendering=player_rig::part(entity,7,owner,0x13560e4);if(!rendering)return;
        auto root=weapon_control::fab(player_rig::word(rendering+0x9c));if(!root||player_rig::word(root+0xf8)!=owner)return;
        auto count=player_rig::word(root+0x28);if(count>32)return;
        Entry current[128]{};unsigned currentCount=0;
        for(unsigned i=0;i<count;++i){auto child=weapon_control::fab(player_rig::word(player_rig::word(root+0x24)+i*4));if(!child)continue;
            auto childOwner=player_rig::word(child+0xf8);auto item=player_rig::resolve(childOwner);
            if(player_rig::part(item,11,childOwner,0x135745c))continue;
            if(!player_rig::part(item,12,childOwner,0x13563e4)&&!player_rig::part(item,40,childOwner,0x1356bec))continue;
            if(!wholeBody&&!headMesh(child,root))continue;
            unsigned previous=currentCount;
            if(!capture(child,current,currentCount)){currentCount=previous;continue;}
            // Native hide is recursive. A head parent must never hide a body
            // attachment underneath it, even if the parent palette is head-only.
            if(!wholeBody){
                bool safe=true;
                for(unsigned j=previous;j<currentCount;++j){
                    auto node=weapon_control::fab(current[j].index);
                    if(!node||!headMesh(node,root)){safe=false;break;}
                }
                if(!safe){currentCount=previous;continue;}
            }
        }
        if(normal){
            // Native near-camera culling can leave the head marked hidden when
            // entering chase view. Reveal only verified current player head
            // attachments; never touch weapons or detached/reused objects.
            for(unsigned i=0;i<currentCount;++i){auto object=weapon_control::fab(current[i].index);
                if(object&&player_rig::word(object+0xf8)==current[i].owner
                    &&(player_rig::word(object+0x1d0)&4))show(reinterpret_cast<void*>(object));
            }
            return;
        }
        bool changed=currentCount!=savedCount;
        for(unsigned i=0;i<currentCount;++i){
            // Preserve the state before our first hide, including reparented
            // meshes. Fresh nodes retain their actual state at discovery.
            for(unsigned j=0;j<savedCount;++j)if(sameNode(current[i],saved[j])){
                current[i].wasHidden=saved[j].wasHidden;break;
            }
            if(i>=savedCount||!sameLink(current[i],saved[i]))changed=true;
        }
        if(changed){
            // Restore detached nodes as well. Then undo any recursive effect on
            // new descendants using the snapshot taken before restoration.
            restore();applyOriginal(current,currentCount);
            memcpy(saved,current,currentCount*sizeof(Entry));savedCount=currentCount;
            if(currentCount)log("First-person %s visibility tree refreshed: meshes=%u\n",wholeBody?"body":"head",currentCount);
        }
        currentOwner=owner;
        // A child can be revealed independently while its parent stays hidden.
        // Enforce every currently owned descendant, never detached old entries.
        for(unsigned i=0;i<savedCount;++i){auto object=weapon_control::fab(saved[i].index);
            if(object&&player_rig::word(object+0xf8)==saved[i].owner
                &&!(player_rig::word(object+0x1d0)&4))hide(reinterpret_cast<void*>(object));}
    } __except(EXCEPTION_EXECUTE_HANDLER){log("Body visibility unavailable; skipping\n");}
}
inline void install(){
    auto h=reinterpret_cast<unsigned char*>(gameBase+0x8ae970),s=reinterpret_cast<unsigned char*>(gameBase+0x8ae900);
    const unsigned char prologue[]={0x56,0x8b,0xf1};
    if(memcmp(h+7,prologue,3)||memcmp(s+7,prologue,3))return;
    if(h[0]!=0x53||h[1]!=0x8b||h[2]!=0x1d||s[0]!=0x53||s[1]!=0x8b||s[2]!=0x1d
        ||*reinterpret_cast<uintptr_t*>(h+3)!=gameBase+0x15fdf54||*reinterpret_cast<uintptr_t*>(s+3)!=gameBase+0x15fdf54)return;
    hide=reinterpret_cast<Visibility>(h);show=reinterpret_cast<Visibility>(s);
    log("Body visibility audit 001 installed; head classifier active\n");
}
}
