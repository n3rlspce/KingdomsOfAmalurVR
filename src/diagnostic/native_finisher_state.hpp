#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include "game_pause.hpp"

// Read-only identities verified against the resident resource managers and the
// successful physical RT-release -> A capture. No native predicates or writes.
namespace native_finisher_state {
struct Snapshot {
    bool valid{},gameplay{},magicMode{},targetKnown{},targetDown{},finisherReady{},specialBoss{},magicResidue{},nativeSequence{};
    uint32_t owner{},target{};
    uintptr_t player{},entity{},targetEntity{};
    float playerPosition[3]{};
    float distanceMetres{1000000.f};
};
inline uintptr_t part(uintptr_t entity,uint32_t owner,unsigned index){
    if(!entity||player_rig::word(entity+0x38)!=owner||!(player_rig::word(entity+0x10c)&1))return 0;
    const auto p=player_rig::word(entity+0x3c+index*4);
    if(!p||player_rig::word(p+0x18)!=owner||player_rig::word(p+0x1c)!=index||!(player_rig::word(p+0x20)&1))return 0;
    const auto vt=player_rig::word(p);
    return vt>=gameBase&&vt<gameBase+0x1600000?p:0;
}
inline bool states(uintptr_t entity,uint32_t* result,unsigned& count){
    const auto data=player_rig::word(entity+0x14),n=player_rig::word(entity+0x18);
    if(n>128||(n&&!data))return false;
    for(unsigned i=0;i<n;++i)result[i]=uint32_t(player_rig::word(data+i*12));
    if(player_rig::word(entity+0x14)!=data||player_rig::word(entity+0x18)!=n)return false;
    count=unsigned(n);return true;
}
inline bool contains(const uint32_t* values,unsigned count,uint32_t id){
    for(unsigned i=0;i<count;++i)if(values[i]==id)return true;
    return false;
}
inline Snapshot read(){
    Snapshot s{};
    __try {
        if(!gameBase)return s;
        s.player=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!s.player||(player_rig::word(s.player)!=gameBase+0x1359f14&&player_rig::word(s.player)!=gameBase+0x1359e94))return s;
        s.owner=uint32_t(player_rig::word(s.player+0x1ec));s.entity=player_rig::resolve(s.owner);
        const auto actor=part(s.entity,s.owner,0),location=part(s.entity,s.owner,6);
        if(!actor||!location)return s;
        std::memcpy(s.playerPosition,reinterpret_cast<const void*>(location+0x24),12);
        for(float value:s.playerPosition)if(!std::isfinite(value))return s;
        uint32_t playerStates[128]{};unsigned playerCount{};
        if(!states(s.entity,playerStates,playerCount))return s;
        s.magicResidue=contains(playerStates,playerCount,161);
        const auto stack=s.player+0x18;
        if(player_rig::word(stack)!=gameBase+0x13369a4)return s;
        const auto root=player_rig::word(gameBase+0x15fb170);
        const auto categories=root?player_rig::word(root+0xc):0;
        const auto table=categories?player_rig::word(categories+4):0;
        const auto manager=table?player_rig::word(table+27*4):0;
        if(!manager||player_rig::word(manager+0x7c)!=27)return s;
        const auto resources=player_rig::word(manager+0x18),resourceCount=player_rig::word(manager+0x1c);
        const auto list=player_rig::word(stack+0x24),count=player_rig::word(stack+0x28);
        if(!resources||resourceCount<=84||resourceCount>256||!list||!count||count>32)return s;
        const auto baseMode=player_rig::word(resources+66*4),meleeMode=player_rig::word(resources+46*4),magicMode=player_rig::word(resources+48*4),sequenceMode=player_rig::word(resources+84*4);
        bool basePresent=false,fateActionPresent=false,sequencePresent=false,ledgerPresent=false;
        const auto ledgerMode=player_rig::word(resources+31*4);
        for(unsigned i=0;i<count;++i){
            const auto mode=player_rig::word(list+i*4);
            if(!mode||player_rig::word(mode)!=gameBase+0x1336470)return s;
            basePresent|=mode==baseMode;sequencePresent|=mode==sequenceMode;ledgerPresent|=mode==ledgerMode;
            s.magicMode|=mode==magicMode;
            const auto actionCount=player_rig::word(mode+4),actions=player_rig::word(mode+8);
            if(actionCount>2048||(actionCount&&!actions))return s;
            for(unsigned j=0;j<actionCount;++j)fateActionPresent|=player_rig::word(actions+j*4)==712;
            if(player_rig::word(mode+4)!=actionCount||player_rig::word(mode+8)!=actions)return s;
        }
        if(player_rig::word(stack+0x24)!=list||player_rig::word(stack+0x28)!=count||player_rig::word(manager+0x18)!=resources)return s;
        s.nativeSequence=sequencePresent&&contains(playerStates,playerCount,314); // Fate_Shift
        // Configured finisher action is necessary, not proof of dispatch priority.
        // Pause, ledger, focus, dialogue and user-input gates remain independent.
        s.gameplay=basePresent&&(fateActionPresent||s.magicMode)&&!ledgerPresent&&game_pause::sample(true)==0;
        s.target=uint32_t(player_rig::word(actor+0xbc));
        s.targetEntity=s.target?player_rig::resolve(s.target):0;
        if(s.targetEntity){
            const auto targetLocation=part(s.targetEntity,s.target,6),health=part(s.targetEntity,s.target,1);
            uint32_t targetStates[128]{};unsigned targetCount{};
            if(targetLocation&&health&&states(s.targetEntity,targetStates,targetCount)){
                float targetPosition[3]{};std::memcpy(targetPosition,reinterpret_cast<const void*>(targetLocation+0x24),12);
                float distanceSquared=0;bool finite=true;
                for(unsigned i=0;i<3;++i){finite&=std::isfinite(targetPosition[i]);const auto d=targetPosition[i]-s.playerPosition[i];distanceSquared+=d*d;}
                s.targetKnown=finite;
                if(finite&&std::isfinite(distanceSquared))s.distanceMetres=std::sqrt(distanceSquared)/100.f;
                // Common enemies and bosses share Incapacitated. Do not require
                // troll rig599 or motion786; normal executions may already be0HP.
                // Balor/Tirnoch advertise additional paired player/boss states.
                // Native A dispatch retains its own phase/space/alias checks.
                const bool balor1=contains(playerStates,playerCount,114)&&contains(targetStates,targetCount,114);
                const bool balor2=contains(playerStates,playerCount,115)&&contains(targetStates,targetCount,115);
                const bool tirnoch=contains(playerStates,playerCount,828)&&contains(targetStates,targetCount,828);
                s.specialBoss=balor1||balor2||tirnoch;
                s.targetDown=finite&&(contains(targetStates,targetCount,785)||contains(targetStates,targetCount,114)
                    ||contains(targetStates,targetCount,115)||contains(targetStates,targetCount,828));
                s.finisherReady=finite&&!contains(playerStates,playerCount,481)&&
                    (contains(targetStates,targetCount,785)||s.specialBoss);
            }
        }
        if(player_rig::word(actor+0xbc)!=s.target||player_rig::resolve(s.owner)!=s.entity)return Snapshot{};
        s.valid=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return Snapshot{};}
    return s;
}
}
