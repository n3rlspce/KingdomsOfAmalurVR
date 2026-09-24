#include "longsword_stroke.hpp"
#include "melee_stroke_targets.hpp"
#include <cassert>
#include <cstdio>
using namespace amalur;
struct Replay {
    LongswordStroke stroke;
    LongswordSeparation separation;
    MeleeStrokeTargets targets;
    mgs5vr::Vec3 hand{};
    uint64_t tick=100,serial{};
    void move(unsigned ms,float vx) {
        for(unsigned i=0;i<ms;i+=10){
            tick+=10;hand.x+=vx*.01f;
            stroke.sample(hand,{},{0,0,-1},tick,1,true,false);
            if(stroke.active()&&serial!=stroke.stroke()) {serial=stroke.stroke();targets={};}
        }
    }
    bool scan(uint32_t actor,bool complete=true) {
        separation.beginFrame(tick,serial);
        const bool allowed=actor&&separation.observe(actor,tick)&&stroke.contactReady()&&targets.available(actor);
        if(allowed){assert(targets.claim(actor));separation.hit(actor,tick);stroke.commit(tick);}
        separation.endFrame(complete);
        return allowed;
    }
    void first() {move(200,0);move(120,6);assert(scan(7));}
};
int main(){
    Replay quick;quick.first();auto first=quick.tick;
    // Sweep hits a second enemy but never the first enemy twice.
    quick.move(10,6);assert(quick.scan(8));quick.move(10,6);assert(!quick.scan(7));
    quick.move(10,-6);quick.scan(0);
    quick.move(70,-6);assert(quick.tick-first<180);assert(quick.scan(7));
    quick.move(10,-6);assert(!quick.scan(7));
    // A fresh physical stroke alone cannot damage an embedded target again.
    Replay embedded;embedded.first();
    for(int i=0;i<12;++i){embedded.move(10,-6);assert(!embedded.scan(7));}
    // Neither failed collision queries nor a tracking interruption prove exit.
    Replay partial;partial.first();partial.move(10,-6);partial.scan(0,false);
    partial.move(70,-6);assert(!partial.scan(7));
    Replay gap;gap.first();gap.move(10,-6);gap.scan(0);gap.separation.interrupt();
    gap.move(70,-6);assert(!gap.scan(7));
    // Time outside a target does not let one uninterrupted pass hit it twice.
    Replay pass;pass.first();
    for(int i=0;i<30;++i){pass.move(10,6);pass.scan(0);}
    pass.move(10,6);assert(!pass.scan(7));
    // An emitted air swing must not delay the next deliberate return cut.
    Replay air;air.move(200,0);air.move(120,6);assert(air.stroke.commit(air.tick));
    first=air.tick;air.move(80,-6);assert(air.tick-first<180);assert(air.scan(7));
    puts("PASS fresh separated cuts under180ms, per-target sweep dedup, embedded/partial/gap guards, air-to-hit return");
}
