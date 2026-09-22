#define NOMINMAX
#include "../../src/bridge_tracking/developer_panel_input.hpp"
#include <cassert>
#include <cstdio>
int main(){
    amalur::DeveloperPanelInput input;amalur::TouchInput t;
    auto frame=[&](uint64_t ms,bool active=true,bool visible=true){return input.update(t,active,visible,false,ms);};
    frame(0);frame(1);frame(2);
    t.leftY=-1;assert(frame(10).row==1);
    assert(!frame(359).row);assert(frame(360).row==1);
    assert(!frame(434).row);assert(frame(435).row==1);
    assert(frame(5000).row==1);assert(!frame(5001).row);
    t.leftY=1;assert(frame(5010).row==-1);assert(!frame(5011).row);
    t.a=true;t.y=true;auto e=frame(5360);assert(e.row==-1&&!e.activate&&!e.reset);
    t={};frame(5400);t.leftX=1;assert(frame(5410).destination==1);
    assert(!frame(6000).destination&&!frame(6500).row); // No adjustment repeats.
    t={};frame(6600);t.leftY=-1;assert(frame(6610).row==1);
    frame(6620,false);assert(!frame(9000).row); // Held stick after focus regain waits for neutral.
    t={};frame(9010);frame(9020);t.leftY=-1;assert(frame(9030).row==1);
    t.x=true;e=frame(9400);assert(e.tab&&!e.row&&!e.activate);
    t.x=false;assert(!frame(10000).row); // Tab transition must release the stick.
    puts("PASS: 350ms delay, 75ms repeat, reversal, no catch-up, no action/reset or horizontal repeat, focus/tab guards");
}
