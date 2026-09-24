#pragma once
#include <cstdio>
#include "../tracking/walk_trace_schedule.hpp"
// Include after native_finisher_state.hpp. Called only by the existing locked
// XInput receiver, after all merges. No input changes or native predicate calls.
namespace walk_input_trace {
inline amalur::WalkTraceSchedule schedule;
inline void observe(uint64_t now,bool active,bool focused,float rawX,float rawY,
                    const amalur::MotionInputPacket& packet,const XINPUT_GAMEPAD& pad,
                    const native_finisher_state::Snapshot& snapshot,unsigned context){
    const bool moving=(active&&(rawX!=0||rawY!=0))||pad.sThumbLX!=0||pad.sThumbLY!=0;
    if(!schedule.sample(now,moving))return;
    uint32_t ids[128]{};unsigned count=0;bool nativeValid=false;
    __try {
        nativeValid=snapshot.valid&&snapshot.owner&&snapshot.entity
            &&reinterpret_cast<uintptr_t>(player_rig::player.load())==snapshot.player
            &&player_rig::word(snapshot.player+0x1ec)==snapshot.owner
            &&player_rig::resolve(snapshot.owner)==snapshot.entity
            &&native_finisher_state::part(snapshot.entity,snapshot.owner,6)
            &&native_finisher_state::states(snapshot.entity,ids,count);
    }__except(EXCEPTION_EXECUTE_HANDLER){nativeValid=false;count=0;}
    char states[1536]{};size_t used=0;
    if(nativeValid)for(unsigned i=0;i<count;++i){
        const int n=std::snprintf(states+used,sizeof(states)-used,"%s%u",i?",":"",ids[i]);
        if(n<0||size_t(n)>=sizeof(states)-used)break;used+=size_t(n);
    }
    log("VR walk input tick=%llu active=%d focused=%d session=%u age=%llu raw=%.4f,%.4f delivered=%d,%d buttons=%04x context=%u gameplay=%d native=%d owner=%08x position=%.3f,%.3f,%.3f states=%s\n",
        now,active,focused,active?packet.session:0,active&&now>=packet.tick?now-packet.tick:~uint64_t(0),
        active?rawX:0,active?rawY:0,int(pad.sThumbLX),int(pad.sThumbLY),unsigned(pad.wButtons),context,
        snapshot.gameplay,nativeValid,snapshot.owner,nativeValid?snapshot.playerPosition[0]:0,
        nativeValid?snapshot.playerPosition[1]:0,nativeValid?snapshot.playerPosition[2]:0,states);
}
}
