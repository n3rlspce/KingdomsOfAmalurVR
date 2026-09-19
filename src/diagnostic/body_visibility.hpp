#pragma once
// Temporary first-person workaround: hide only the local player's appearance
// and armor attachments. The player rig and weapon attachment remain active.
namespace body_visibility {
inline std::atomic<bool> enabled{false};
inline bool previousWholeBody{};
// Head/face palettes contain the verified head bone but no hand or leg bones.
// Classify the current mesh, not an equipment slot that changes with gear.
inline bool headMesh(uintptr_t object){
    auto count=player_rig::word(object+0x38);if(!count||count>128)return false;
    auto manager=player_rig::word(gameBase+0x15fdf54),assetId=player_rig::word(object+0xf0);
    if(!manager||assetId<2||assetId>=100000)return false;
    auto flags=*reinterpret_cast<unsigned char*>(player_rig::word(manager+0x28)+assetId);
    if(!(flags&4)||(flags&0x10))return false;
    auto asset=player_rig::word(player_rig::word(manager+0x18)+assetId*4),blob=player_rig::word(asset+0x1c);
    if(player_rig::word(blob)!=0x45533033||player_rig::word(blob+0x10)!=count)return false;
    auto offset=player_rig::word(blob+0x20);if(!offset||offset>65536)return false;
    bool head=false;
    for(unsigned i=0;i<count;++i){auto id=player_rig::word(blob+0x20+offset+i*4);
        if(id==0x5a2e4c)head=true;
        if(id==0x88d0eb||id==0x87c3ed||id==0x91f42f||id==0x93012d)return false;
    }
    return head;
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
inline void update(){
    if(!hide||!show)return;
    __try {
        bool active=firstPerson.load()&&headTracking.load()&&haveCameraForFrame;
        bool wholeBody=enabled.load();
        if(previousWholeBody!=wholeBody){if(savedCount)restore();previousWholeBody=wholeBody;}
        auto p=reinterpret_cast<uintptr_t>(player_rig::player.load());
        bool validPlayer=p&&(player_rig::word(p)==gameBase+0x1359f14||player_rig::word(p)==gameBase+0x1359e94);
        auto owner=validPlayer?player_rig::word(p+0x1ec):0;
        auto entity=owner?player_rig::resolve(owner):0;
        if(!active||!entity){if(savedCount)restore();return;}
        if(savedCount&&currentOwner!=owner)restore();
        auto rendering=player_rig::part(entity,7,owner,0x13560e4);if(!rendering)return;
        auto root=weapon_control::fab(player_rig::word(rendering+0x9c));if(!root||player_rig::word(root+0xf8)!=owner)return;
        auto count=player_rig::word(root+0x28);if(count>32)return;
        Entry current[128]{};unsigned currentCount=0;
        for(unsigned i=0;i<count;++i){auto child=weapon_control::fab(player_rig::word(player_rig::word(root+0x24)+i*4));if(!child)continue;
            auto childOwner=player_rig::word(child+0xf8);auto item=player_rig::resolve(childOwner);
            if(player_rig::part(item,11,childOwner,0x135745c))continue;
            if(!player_rig::part(item,12,childOwner,0x13563e4)&&!player_rig::part(item,40,childOwner,0x1356bec))continue;
            if(!wholeBody&&!headMesh(child))continue;
            unsigned previous=currentCount;
            if(!capture(child,current,currentCount)){currentCount=previous;continue;}
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
}
}
