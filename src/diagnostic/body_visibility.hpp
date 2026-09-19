#pragma once
// Temporary first-person workaround: hide only the local player's appearance
// and armor attachments. The player rig and weapon attachment remain active.
namespace body_visibility {
inline std::atomic<bool> enabled{false};
using Visibility=void(__thiscall*)(void*);
inline Visibility hide{},show{};
struct Entry {uint32_t index{},owner{};bool wasHidden{};};
inline Entry saved[128];inline unsigned savedCount{};
inline uint32_t currentOwner{};
inline Entry roots[32];inline unsigned rootCount{};
inline bool capture(uintptr_t object,unsigned depth=0){
    if(!object||depth>8||savedCount>=128)return false;
    auto index=player_rig::word(object+0x194);
    for(unsigned i=0;i<savedCount;++i)if(saved[i].index==index)return true;
    saved[savedCount++]={index,player_rig::word(object+0xf8),(player_rig::word(object+0x1d0)&4)!=0};
    auto count=player_rig::word(object+0x28);if(count>32)return false;
    for(unsigned i=0;i<count;++i){auto child=weapon_control::fab(player_rig::word(player_rig::word(object+0x24)+i*4));
        if(child&&!capture(child,depth+1))return false;}
    return true;
}
inline void restore(){
    // Parents precede children. Reapply each child's original hidden state after
    // the native parent operation recursively changes its descendants.
    for(unsigned i=0;i<savedCount;++i){auto e=saved[i];auto p=weapon_control::fab(e.index);
        if(p&&player_rig::word(p+0xf8)==e.owner)(e.wasHidden?hide:show)(reinterpret_cast<void*>(p));}
    savedCount=0;currentOwner=0;rootCount=0;
}
inline void update(){
    if(!hide||!show)return;
    __try {
        bool active=enabled.load()&&firstPerson.load()&&headTracking.load()&&haveCameraForFrame;
        auto p=reinterpret_cast<uintptr_t>(player_rig::player.load());
        bool validPlayer=p&&(player_rig::word(p)==gameBase+0x1359f14||player_rig::word(p)==gameBase+0x1359e94);
        auto owner=validPlayer?player_rig::word(p+0x1ec):0;
        auto entity=owner?player_rig::resolve(owner):0;
        if(!active||!entity){if(savedCount)restore();return;}
        if(savedCount&&currentOwner!=owner)restore();
        auto rendering=player_rig::part(entity,7,owner,0x13560e4);if(!rendering)return;
        auto root=weapon_control::fab(player_rig::word(rendering+0x9c));if(!root||player_rig::word(root+0xf8)!=owner)return;
        auto count=player_rig::word(root+0x28);if(count>32)return;
        bool changed=false;
        for(unsigned i=0;i<rootCount;++i){bool found=false;
            for(unsigned j=0;j<count;++j)if(player_rig::word(player_rig::word(root+0x24)+j*4)==roots[i].index){auto child=weapon_control::fab(roots[i].index);found=child&&player_rig::word(child+0xf8)==roots[i].owner;break;}
            if(!found)changed=true;
        }
        if(changed)restore();
        for(unsigned i=0;i<count;++i){auto child=weapon_control::fab(player_rig::word(player_rig::word(root+0x24)+i*4));if(!child)continue;
            auto childOwner=player_rig::word(child+0xf8);auto item=player_rig::resolve(childOwner);
            if(player_rig::part(item,11,childOwner,0x135745c))continue;
            if(!player_rig::part(item,12,childOwner,0x13563e4)&&!player_rig::part(item,40,childOwner,0x1356bec))continue;
            bool known=false;for(unsigned j=0;j<savedCount;++j)if(saved[j].index==player_rig::word(child+0x194)&&saved[j].owner==childOwner){known=true;break;}
            if(!known){unsigned previous=savedCount;if(!capture(child)){savedCount=previous;continue;}
                if(rootCount<32)roots[rootCount++]={player_rig::word(child+0x194),childOwner,false};
                log("First-person body attachment hidden: slot=%u owner=%08x\n",i,childOwner);}
            currentOwner=owner;
            if(!(player_rig::word(child+0x1d0)&4))hide(reinterpret_cast<void*>(child));
        }
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
