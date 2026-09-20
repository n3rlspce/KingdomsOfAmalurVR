#define NOMINMAX
#include "../../src/xr_smoke/vr_settings.hpp"
#include "../../src/tracking/developer_commands.hpp"
#include <cstdio>

int main(){
    VrSettings s;
    s.developerTogglePending=true;s.poll();
    if(!s.developerVisible||s.visible||s.developerAction!=-1)return 1;
    s.held[VK_DOWN]=true;s.poll();
    if(s.developerRow!=1)return 2;
    s.poll();if(s.developerRow!=1)return 3; // held key must not repeat
    s.held[VK_RETURN]=true;s.poll();
    if(s.developerAction!=1)return 4;
    s.developerAction=-1;s.poll();if(s.developerAction!=-1)return 5;
    s.held[VK_DOWN]=false;s.held[VK_RETURN]=false;s.poll();
    s.developerRow=amalur::developer::panelRows-1;s.held[VK_DOWN]=true;s.poll();
    if(s.developerRow!=0)return 6;
    s.held[VK_ESCAPE]=true;s.poll();if(s.developerVisible)return 7;
    s.held[VK_ESCAPE]=false;s.poll();s.developerTogglePending=true;s.poll();
    s.togglePending=true;s.poll();if(!s.visible||s.developerVisible)return 8;
    if(amalur::developer::command(1)!="amalur_dev.wolf()")return 9;
    for(int i=2;i<11;++i)if(amalur::developer::command(i).empty())return 10;
    s.visible=false;s.developerVisible=true;s.developerRow=2;
    s.held[VK_DOWN]=false;s.poll();
    s.held[VK_RIGHT]=true;s.poll();if(s.developerDestination!=1)return 11;
    s.held[VK_RETURN]=true;s.poll();
    if(amalur::developer::command(s.developerAction)!="amalur_dev.give_and_equip('sword1h_common01a',0)")return 12;
    s.held[VK_RETURN]=false;s.held[VK_RIGHT]=false;s.poll();
    s.held[VK_RIGHT]=true;s.poll();s.held[VK_RETURN]=true;s.poll();
    if(amalur::developer::command(s.developerAction)!="amalur_dev.give_and_equip('sword1h_common01a',1)")return 13;
    s.held[VK_RETURN]=false;s.held[VK_RIGHT]=false;s.poll();
    s.held[VK_RIGHT]=true;s.poll();s.held[VK_RETURN]=true;s.poll();
    if(amalur::developer::command(s.developerAction)!="amalur_dev.equip_existing('sword1h_common01a',0)")return 14;
    if(amalur::developer::command(amalur::developer::rows*4+amalur::developer::rows-1)!="amalur_dev.equip_existing('sword2h_unique12f',1)")return 15;
    VrSettings c;c.developerVisible=false;c.visible=false;
    amalur::TouchInput t;
    auto update=[&](uint64_t now,bool busy=false,bool active=true){return c.pollDeveloperControllers(t,active,busy,now);};
    update(0);t.leftClick=t.rightClick=true;
    if(!update(10)||c.developerVisible)return 16;
    update(659);if(c.developerVisible)return 17;
    update(660);if(!c.developerVisible)return 18;
    update(1500);if(!c.developerVisible)return 19; // held chord cannot close again
    t={};update(1600);
    t.leftY=-1;update(1700);if(c.developerRow!=1)return 20;
    update(2000);if(c.developerRow!=1)return 21; // no held-stick repeat
    t={};update(2100);t.leftY=-1;update(2200);if(c.developerRow!=2)return 22;
    t={};update(2300);t.leftX=1;update(2400);if(c.developerDestination!=1)return 23;
    t={};update(2500);t.a=true;update(2600);
    if(c.developerAction!=amalur::developer::panelAction(2,1))return 24;
    c.developerAction=-1;update(2700);if(c.developerAction!=-1)return 25;
    t={};update(2800);t.rightTrigger=.8f;update(2900,true);
    update(3000,false);if(c.developerAction!=-1)return 26; // no delayed action after busy
    t.rightTrigger=.5f;update(3100);if(c.developerAction!=-1)return 27;
    t={};update(3200);t.rightTrigger=.8f;update(3300);
    if(c.developerAction!=amalur::developer::panelAction(2,1))return 28;
    c.developerAction=-1;t.b=true;update(3400);if(c.developerVisible)return 29;
    if(!update(3500)||c.developerAction!=-1)return 30; // closing trigger stays captured
    t={};update(3600);if(update(3700))return 31;
    c.developerVisible=true;update(3800);update(3900);
    t.a=true;update(4000,false,false);update(4100);
    if(c.developerAction!=-1)return 32; // focus regain with A held cannot execute
    t={};update(4200);update(4300);t.a=true;update(4400);
    if(c.developerAction<0)return 33;
    if(amalur::developer::panelAction(amalur::developer::rows,4)!=amalur::developer::prepareCharacterAction)return 34;
    if(amalur::developer::panelAction(amalur::developer::rows+1,4)!=amalur::developer::invincibilityAction)return 35;
    // Centered hold must not replace the existing deflected-stick D-pad chord.
    amalur::DeveloperPanelInput input;input.update({},true,false,false,0);
    t={};t.leftClick=t.rightClick=true;t.leftX=1;
    if(input.update(t,true,false,false,1000).capture)return 36;
    if(input.update(t,true,false,false,2000).toggle)return 37;
    amalur::TouchMapper mapper;mapper.map({},true);t={};t.rightTrigger=1;
    auto blocked=mapper.map(t,false);if(blocked.active||blocked.buttons)return 38;
    auto held=mapper.map(t,true);if(held.buttons)return 39;
    puts("PASS: keyboard and controller panel; hold/open, navigation, slots, one-shot actions, busy/focus/release guards and gameplay suppression.");
    return 0;
}
