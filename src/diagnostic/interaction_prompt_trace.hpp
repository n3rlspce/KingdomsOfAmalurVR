#pragma once
#include <Xinput.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include "../tracking/interaction_trace_schedule.hpp"
namespace interaction_prompt_trace {
// Serialized by the caller's motion-controls lock. Reads only: no target
// selection, predicate invocation, native action, or prompt substitution.
inline amalur::InteractionTraceSchedule schedule;
inline bool receipt{};
inline unsigned processEdges{};
inline const char* phase="down";
struct PreviousPoll {uint64_t tick{};uint32_t owner{},selected{},candidates{},yaw{};bool valid{};};
inline PreviousPoll previousPoll;
inline void cachePrevious(uint64_t now){
    previousPoll={};
    __try{
        const auto p=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!p||(player_rig::word(p)!=gameBase+0x1359f14&&player_rig::word(p)!=gameBase+0x1359e94))return;
        const auto owner=static_cast<uint32_t>(player_rig::word(p+0x1ec));
        const auto loc=player_rig::part(player_rig::resolve(owner),6,owner,0x1355cdc);
        const auto globals=player_rig::word(gameBase+0x15fe9c4);
        if(!loc||!globals)return;
        previousPoll={now,owner,static_cast<uint32_t>(player_rig::word(globals+0x3c5c)),
            static_cast<uint32_t>(player_rig::word(globals+0x3c40)),static_cast<uint32_t>(player_rig::word(loc+0xb0)),true};
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
inline bool bindings(){
    const auto code=reinterpret_cast<const unsigned char*>(gameBase+0xa36620);
    constexpr unsigned char tail[]{0x8b,0x88,0x5c,0x3c,0,0,0x8b,0x44,0x24,0x04,0x85,0xc9};
    return code[0]==0xa1&&player_rig::word(reinterpret_cast<uintptr_t>(code)+1)==gameBase+0x15fe9c4
        &&!std::memcmp(code+5,tail,sizeof(tail));
}
inline uintptr_t interactionPart(uint32_t handle){
    const auto entity=player_rig::resolve(handle);
    if(!entity||!(player_rig::word(entity+0x10c)&1))return 0;
    const auto part=player_rig::word(entity+0x3c+34*4);
    // Part34 identity/active fields are native component headers. Exact vtable
    // is not captured; require a game image vtable rather than invent an RVA.
    if(!part||player_rig::word(part+0x18)!=handle||player_rig::word(part+0x1c)!=34
        ||!(player_rig::word(part+0x20)&1))return 0;
    const auto table=player_rig::word(part);
    return table>=gameBase&&table<gameBase+0x1600000?part:0;
}
inline void candidate(unsigned edge,unsigned index,uint32_t handle,uint32_t selected){
    __try{
        const auto entity=player_rig::resolve(handle),part=interactionPart(handle);
        const auto loc=entity?player_rig::part(entity,6,handle,0x1355cdc):0;
        if(!entity||!part||!loc||!(player_rig::word(loc+0x20)&1)){
            log("VR interaction candidate edge=%u index=%u handle=%08x selected=%d result=identity-unavailable entity=%08x part=%08x loc=%08x\n",
                edge,index,handle,handle==selected,unsigned(entity),unsigned(part),unsigned(loc));return;
        }
        float xyz[3];std::memcpy(xyz,reinterpret_cast<void*>(loc+0x24),sizeof(xyz));
        if(!std::isfinite(xyz[0])||!std::isfinite(xyz[1])||!std::isfinite(xyz[2])){
            log("VR interaction candidate edge=%u index=%u handle=%08x result=invalid-position\n",edge,index,handle);return;
        }
        const unsigned flags=*reinterpret_cast<const unsigned char*>(part+0x58);
        log("VR interaction candidate edge=%u index=%u handle=%08x selected=%d partVtable=%08x flags58=%02x promptBit=%u pos=%.3f,%.3f,%.3f yaw=%08x\n",
            edge,index,handle,handle==selected,unsigned(player_rig::word(part)-gameBase),flags,(flags>>1)&1,
            xyz[0],xyz[1],xyz[2],unsigned(player_rig::word(loc+0xb0)));
        const auto action=player_rig::word(part+0x44),actions=player_rig::word(part+0x48),actionCount=player_rig::word(part+0x4c);
        log("VR interaction actions edge=%u handle=%08x current=%u count=%u\n",edge,handle,unsigned(action),unsigned(actionCount));
        if(actionCount<=128&&(!actionCount||actions)){
            uint32_t assets[24]{};const unsigned captured=actionCount<24?unsigned(actionCount):24;
            for(unsigned j=0;j<captured;++j)assets[j]=uint32_t(player_rig::word(actions+j*4));
            if(player_rig::word(part+0x48)==actions&&player_rig::word(part+0x4c)==actionCount){
                for(unsigned j=0;j<captured;++j)log("VR interaction action edge=%u handle=%08x index=%u asset=%u\n",edge,handle,j,assets[j]);
                if(actionCount>captured)log("VR interaction actions edge=%u handle=%08x omitted=%u\n",edge,handle,unsigned(actionCount)-captured);
            }else log("VR interaction actions edge=%u handle=%08x result=vector-changed\n",edge,handle);
        }else log("VR interaction actions edge=%u handle=%08x result=vector-rejected\n",edge,handle);
        const auto range=player_rig::word(entity+0x3c+24*4);
        if(range&&player_rig::word(range+0x18)==handle&&player_rig::word(range+0x1c)==24&&(player_rig::word(range+0x20)&1))
            log("VR interaction range edge=%u handle=%08x flags71=%02x range68Bits=%08x\n",edge,handle,
                unsigned(*reinterpret_cast<const unsigned char*>(range+0x71)),unsigned(player_rig::word(range+0x68)));

    }__except(EXCEPTION_EXECUTE_HANDLER){log("VR interaction candidate edge=%u index=%u handle=%08x result=read-failed\n",edge,index,handle);}
}
// ACTOR.get_target_object: A850A0 -> active Part0+BC.
// ACTOR.get_current_health: A7F2E0 -> A390F0 -> active Part1+48.
// ACTOR.has_actor_state: A7FE90 -> A76F20 -> B0DF30 scans entity+14,
// entity+18 count, 12-byte entries whose first word is the state asset ID.
inline uintptr_t checkedPart(uintptr_t entity,unsigned index,uint32_t owner){
    if(!entity||!(player_rig::word(entity+0x10c)&1))return 0;
    const auto part=player_rig::word(entity+0x3c+index*4);
    if(!part||player_rig::word(part+0x18)!=owner||player_rig::word(part+0x1c)!=index||!(player_rig::word(part+0x20)&1))return 0;
    const auto vt=player_rig::word(part);return vt>=gameBase&&vt<gameBase+0x1600000?part:0;
}
inline void combatActor(unsigned edge,const char* role,uint32_t handle){
    __try{
        const auto entity=player_rig::resolve(handle);
        if(!entity){log("VR finisher actor edge=%u role=%s handle=%08x result=identity-unavailable\n",edge,role,handle);return;}
        const auto actor=checkedPart(entity,0,handle),health=checkedPart(entity,1,handle);
        const auto loc=player_rig::part(entity,6,handle,0x1355cdc);
        float xyz[3]{};if(loc)std::memcpy(xyz,reinterpret_cast<const void*>(loc+0x24),12);
        const auto count=player_rig::word(entity+0x18),states=player_rig::word(entity+0x14);
        log("VR finisher actor edge=%u role=%s handle=%08x actorValid=%u healthValid=%u health=%d target=%08x stateCount=%u locValid=%u pos=%.3f,%.3f,%.3f\n",
            edge,role,handle,actor!=0,health!=0,health?int(player_rig::word(health+0x48)):0,
            actor?unsigned(player_rig::word(actor+0xbc)):0,unsigned(count),loc!=0,xyz[0],xyz[1],xyz[2]);
        if(count<=512&&(!count||states)){
            uint32_t ids[64]{};const unsigned captured=count<64?unsigned(count):64;
            for(unsigned i=0;i<captured;++i)ids[i]=uint32_t(player_rig::word(states+i*12));
            if(player_rig::word(entity+0x14)==states&&player_rig::word(entity+0x18)==count){
                char text[768]{};unsigned used=0;
                for(unsigned i=0;i<captured;++i){const int n=std::snprintf(text+used,sizeof(text)-used,"%s%u",i?",":"",ids[i]);
                    if(n<0||unsigned(n)>=sizeof(text)-used)break;used+=unsigned(n);}
                log("VR finisher states edge=%u role=%s handle=%08x ids=%s\n",edge,role,handle,text);
                if(count>captured)log("VR finisher states edge=%u role=%s omitted=%u\n",edge,role,unsigned(count)-captured);
            }else log("VR finisher states edge=%u role=%s result=vector-changed\n",edge,role);
        }else log("VR finisher states edge=%u role=%s result=vector-rejected\n",edge,role);
    }__except(EXCEPTION_EXECUTE_HANDLER){log("VR finisher actor edge=%u role=%s handle=%08x result=read-failed\n",edge,role,handle);}
}
// InputDeviceModeStack RTTI verifies player+18. Snapshot only the native
// mode-resource array; the parallel flags are deliberately not interpreted.
inline void inputModes(unsigned edge,uintptr_t player){
    __try{
        const auto stack=player+0x18;
        if(player_rig::word(stack)!=gameBase+0x13369a4){log("VR finisher input-modes edge=%u result=stack-type-rejected\n",edge);return;}
        const auto count=player_rig::word(stack+0x28),list=player_rig::word(stack+0x24);
        const auto root=player_rig::word(gameBase+0x15fb170);
        const auto categories=root?player_rig::word(root+0xc):0;
        const auto table=categories?player_rig::word(categories+4):0;
        const auto manager=table?player_rig::word(table+27*4):0;
        if(!manager||player_rig::word(manager+0x7c)!=27){log("VR finisher input-modes edge=%u result=manager-identity-rejected\n",edge);return;}
        const auto resources=player_rig::word(manager+0x18),resourceCount=player_rig::word(manager+0x1c);
        const auto hashes=player_rig::word(manager+0x84),hashCount=player_rig::word(manager+0x88);
        const auto inverse=player_rig::word(manager+0xa8),inverseCount=player_rig::word(manager+0xac);
        if(count>32||(count&&!list)||!resources||resourceCount>256||!hashes||hashCount>256||!inverse||inverseCount<resourceCount||inverseCount>256){
            log("VR finisher input-modes edge=%u result=bounds-rejected count=%u\n",edge,unsigned(count));return;}
        uintptr_t pointers[32]{};for(unsigned i=0;i<count;++i)pointers[i]=player_rig::word(list+i*4);
        if(player_rig::word(stack+0x24)!=list||player_rig::word(stack+0x28)!=count){log("VR finisher input-modes edge=%u result=vector-changed\n",edge);return;}
        log("VR finisher input-modes edge=%u count=%u\n",edge,unsigned(count));
        for(unsigned i=0;i<count;++i){
            unsigned id=0,hash=0;
            if(pointers[i]&&player_rig::word(pointers[i])==gameBase+0x1336470){
                for(unsigned j=2;j<resourceCount;++j)if(player_rig::word(resources+j*4)==pointers[i]){id=j;break;}
                if(id){const auto sorted=player_rig::word(inverse+id*4);if(sorted<hashCount)hash=unsigned(player_rig::word(hashes+sorted*4));}
            }
            if(player_rig::word(manager+0x18)!=resources||player_rig::word(manager+0xa8)!=inverse||player_rig::word(manager+0x84)!=hashes){
                log("VR finisher input-modes edge=%u result=resource-table-changed\n",edge);return;}
            log("VR finisher input-mode edge=%u index=%u id=%u nameHash=%08x result=%s\n",edge,i,id,hash,id&&hash?"verified":"unresolved");
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){log("VR finisher input-modes edge=%u result=read-failed\n",edge);}
}
inline void capture(unsigned edge,const char* label,uint64_t now){
    processEdges=edge;phase=label;
    log("VR interaction snapshot edge=%u phase=%s tick=%llu\n",edge,label,now);
    __try{
        if(!gameBase||!bindings()){log("VR interaction A edge=%u tick=%llu result=binding-rejected\n",processEdges,now);return;}
        const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!player||(player_rig::word(player)!=gameBase+0x1359f14&&player_rig::word(player)!=gameBase+0x1359e94)){log("VR interaction A edge=%u tick=%llu result=player-unavailable\n",processEdges,now);return;}
        const auto owner=static_cast<uint32_t>(player_rig::word(player+0x1ec));
        const auto entity=player_rig::resolve(owner),loc=player_rig::part(entity,6,owner,0x1355cdc);
        const auto globals=player_rig::word(gameBase+0x15fe9c4);
        if(!loc||!globals||!(player_rig::word(loc+0x20)&1)){log("VR interaction A edge=%u tick=%llu owner=%08x result=location-or-manager-unavailable\n",processEdges,now,owner);return;}
        inputModes(processEdges,player);
        combatActor(processEdges,"player",owner);
        const auto actor=checkedPart(entity,0,owner);
        const auto target=actor?uint32_t(player_rig::word(actor+0xbc)):0;
        if(target)combatActor(processEdges,"combat-target",target);
        else log("VR finisher target edge=%u handle=00000000\n",processEdges);
        // Generic loot/pickpocket candidates do not explain finisher dispatch.
        // Avoid hundreds of synchronous file writes for every A-button edge.
    }__except(EXCEPTION_EXECUTE_HANDLER){log("VR interaction A edge=%u tick=%llu result=read-failed\n",processEdges,now);}
}
// active means final returned pad is valid, independently of XR focus/tracking.
inline void observe(bool active,WORD buttons,uint64_t now,unsigned nativeRT=0,int moveX=0,int moveY=0){
    if(!receipt){
        bool bound=false;__try{bound=gameBase&&bindings();}__except(EXCEPTION_EXECUTE_HANDLER){}
        log("VR interaction trace armed version=6 binding=%s edgeBudget=128-per-minute candidatesPerSnapshot=0 compactStates=1 phases=previous-poll,down,release,100ms,500ms\n",bound?"verified":"rejected");
        receipt=true;
    }
    const auto events=schedule.sample(active,(buttons&XINPUT_GAMEPAD_A)!=0,now);
    if(events.rearmed)log("VR interaction trace rearmed tick=%llu edgeBudget=128-per-minute\n",now);
    if(events.exhausted)log("VR interaction trace rate-limited edges=128; automatically rearms each minute\n");
    if(events.down){
        log("VR interaction delivered edge=%u tick=%llu buttons=%04x RT=%u LX=%d LY=%d\n",events.edge,now,buttons,nativeRT,moveX,moveY);
        log("VR interaction previous-poll edge=%u available=%d tick=%llu age=%llu owner=%08x selected=%08x candidates=%u yaw=%08x\n",
            events.edge,previousPoll.valid,previousPoll.tick,now>=previousPoll.tick?now-previousPoll.tick:~uint64_t(0),
            previousPoll.owner,previousPoll.selected,previousPoll.candidates,previousPoll.yaw);
        capture(events.edge,"down-before-native-consumption",now);
    }
    if(events.release)capture(events.edge,"release",now);
    if(events.post100)capture(events.edge,"post100-next-poll",now);
    if(events.post500)capture(events.edge,"post500-next-poll",now);
    if(active)cachePrevious(now);else previousPoll={};
}

}
