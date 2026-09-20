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
    s.developerRow=10;s.held[VK_DOWN]=true;s.poll();
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
    puts("PASS: F11 state, row wrap, one-shot Enter, held-key rejection, panel exclusion and nine weapon commands.");
    return 0;
}
