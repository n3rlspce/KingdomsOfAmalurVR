#define NOMINMAX
#include "../../src/xr_smoke/vr_settings.hpp"
#include <cassert>
#include <cstdio>
int main(){
    VrSettings s;
    s.path=L"tabs-check.ini";
    s.hudSize=.8f;
    amalur::TouchInput t;
    auto frame=[&]{return s.pollDeveloperControllers(t,true,false,0);};
    t.leftClick=t.rightClick=true;assert(frame());
    assert(s.visible&&!s.developerVisible);
    t={};frame();
    t.leftX=1;frame();assert(s.hudSize>.84f);
    frame();assert(s.hudSize<.86f); // no held-stick repeat
    t={};frame();t.y=true;frame();assert(s.hudSize==.8f);
    t={};frame();t.x=t.a=true;frame();
    assert(s.developerVisible&&!s.visible&&s.developerAction==-1);
    t.x=false;frame();assert(s.developerAction==-1); // A must release after switching
    t={};frame();t.a=true;frame();assert(s.developerAction==0);
    s.developerAction=-1;t={};frame();t.x=true;frame();
    assert(s.visible&&!s.developerVisible);
    t={};frame();s.selected=VrSettings::rowCount-1;t.a=true;frame();
    assert(s.recenter==1);frame();assert(s.recenter==1);
    t={};frame();t.b=true;assert(frame());assert(!s.panelOpen());
    assert(frame()); // Closing inputs remain captured until released.
    t={};frame();assert(!frame());
    s.togglePending=true;s.poll();assert(s.visible);
    s.held[VK_TAB]=true;s.poll();assert(s.developerVisible&&!s.visible);
    s.poll();assert(s.developerVisible); // Keyboard tab does not repeat.
    s.developerTogglePending=true;s.poll();assert(!s.panelOpen());
    DeleteFileW(s.path.c_str());
    puts("PASS: shared tabs, controller settings, reset/recenter, aliases and action-release guards");
}
