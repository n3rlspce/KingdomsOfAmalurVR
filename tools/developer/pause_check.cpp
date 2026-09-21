#define NOMINMAX
#define AMALUR_PLAY_MODE_MAPPING L"Local\\AmalurPauseOfflineCheck"
#define AMALUR_BODY_DEBUG_MAPPING L"Local\\AmalurPauseBodyOfflineCheck"
#include "../../src/xr_smoke/vr_settings.hpp"
#include <cassert>
#include <cstdio>
int main(){
    VrSettings s;s.visible=true;s.developerVisible=false;
    s.selected=VrSettings::rowCount-2;
    amalur::TouchInput t;amalur::MotionInputPacket packet;
    s.pollDeveloperControllers(t,true,false,0);s.pollDeveloperControllers(t,true,false,1);
    t.a=true;s.pollDeveloperControllers(t,true,false,2);
    assert(s.pauseRequested&&!s.panelOpen());
    packet.buttons=XINPUT_GAMEPAD_X;packet.moveX=1;
    assert(s.applyPauseInput(t,true,packet,3)&&!packet.buttons&&!packet.moveX);
    t={};assert(s.applyPauseInput(t,true,packet,10));
    assert(packet.active&&!packet.buttons&&s.pauseNativeRequested);
    assert(s.applyPauseInput(t,true,packet,189)&&!packet.buttons);
    assert(s.applyPauseInput(t,true,packet,190)&&!packet.buttons);
    assert(!s.applyPauseInput(t,true,packet,191));
    s.requestPause();assert(!s.applyPauseInput(t,false,packet,200));
    assert(!s.applyPauseInput(t,true,packet,300)); // No delayed pause after focus returns.
    s.visible=true;s.held[VK_RETURN]=true;s.poll();
    assert(s.pauseRequested&&!s.panelOpen());
    puts("PASS: controller/keyboard pause row, release before native request, neutral input, input suppression and focus cancellation");
}
