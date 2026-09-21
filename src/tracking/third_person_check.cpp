#define NOMINMAX
#define AMALUR_PLAY_MODE_MAPPING L"Local\\AmalurThirdPersonOfflineCheck"
#define AMALUR_BODY_DEBUG_MAPPING L"Local\\AmalurThirdPersonBodyOfflineCheck"
#include "motion_input.hpp"
#include "body_debug_settings.hpp"
#include "../xr_smoke/vr_settings.hpp"
#include <cassert>
#include <limits>
#include <cstdio>
int main(){
    using namespace amalur;
    playMode.set(false);
    FirstPersonPreference preference;
    assert(preference.load());
    auto before=bodyDebug.read();
    playMode.set(true);
    assert(!preference.load());
    assert(bodyDebug.enabled(nativeTorso)&&bodyDebug.enabled(nativeArms));
    TouchMapper mapper;TouchInput t;mapper.map(t,true);
    t.rightX=.8f;t.rightY=.4f;
    auto p=mapper.map(t,true);p.tick=10;
    assert(p.lookX>0&&p.lookY>0&&p.turnYawDegrees==0&&validMotionInput(p,10));
    XINPUT_GAMEPAD pad{};mergeMotion(pad,p);
    assert(pad.sThumbRX>0&&pad.sThumbRY>0);
    p.lookX=std::numeric_limits<float>::quiet_NaN();assert(!validMotionInput(p,10));
    p=mapper.map(t,false);assert(!p.active&&!p.lookX&&!p.lookY);
    t={};mapper.map(t,true);t.rightTrigger=1;p=mapper.map(t,true);
    assert(p.buttons&XINPUT_GAMEPAD_X); // normal weapon attack remains available
    playMode.set(false);
    assert(preference.load()&&bodyDebug.read()==before);
    t={};mapper.map(t,true);t.rightX=1;p=mapper.map(t,true);
    assert(p.lookX==0&&p.turnYawDegrees==30); // first-person snap turn restored
    VrSettings settings;settings.path=L"third-person-test.ini";
    playMode.set(false);settings.selected=0;settings.adjustSetting(1,false);
    assert(playMode.normal());settings.load();assert(playMode.normal());
    settings.adjustSetting(0,true);assert(!playMode.normal());
    settings.closePanel();t={};t.leftClick=t.rightClick=true;
    settings.pollDeveloperControllers(t,true,false);t={};settings.pollDeveloperControllers(t,true,false);
    t.a=true;settings.pollDeveloperControllers(t,true,false);assert(playMode.normal());
    settings.pollDeveloperControllers(t,true,false);assert(playMode.normal());
    playMode.set(false);DeleteFileW(settings.path.c_str());
    puts("PASS: native-body gates, restored first-person preference, orbit axes, attack, focus gating, persistence and panel toggle");
}
