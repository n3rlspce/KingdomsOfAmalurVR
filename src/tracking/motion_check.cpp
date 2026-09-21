#define NOMINMAX
#include "motion_input.hpp"
#include "weapon_pose.hpp"
#include "camera_inputs.hpp"
#include <cstdio>
#include <cstdlib>
static void check(bool pass,const char* label){if(!pass){printf("FAIL: %s\n",label);std::exit(1);}}
static bool closeEnough(float a,float b){return std::abs(a-b)<.002f;}
int main(){
    for(const auto attack:{XINPUT_GAMEPAD_X,XINPUT_GAMEPAD_Y}){
        XINPUT_GAMEPAD moving{};amalur::MotionInputPacket attackMotion{};
        attackMotion.moveY=.75f;attackMotion.buttons=attack;
        amalur::mergeMotion(moving,attackMotion);
        check(moving.sThumbLY>24000&&(moving.wButtons&attack),"native attack input does not suppress forward locomotion");
    }
    unsigned char core[0x400]{};
    amalur::CameraPose flat{{1,2,3},{4,5,6},{0,0,1}},vr{{10,20,30},{40,50,60},{0,0,1}};
    float vrFov=130;
    memcpy(core+4,&vr.eye,12);memcpy(core+0x14,&vr.target,12);memcpy(core+0x1c0,&vr.up,12);memcpy(core+0x2c,&vrFov,4);
    amalur::CameraInputs lease{core,flat,vr,90,vrFov};
    // Engine may update its target before the next rebuild. Keep that update.
    mgs5vr::Vec3 nativeTarget{7,8,9};memcpy(core+0x14,&nativeTarget,12);
    lease.restore();lease.restore();
    check(memcmp(core+4,&flat.eye,12)==0&&memcmp(core+0x14,&nativeTarget,12)==0,"restore own camera offset without discarding engine updates");
    float restoredFov;memcpy(&restoredFov,core+0x2c,4);check(restoredFov==90,"restore native FOV at frame boundary");
    float x=.1f,y=.1f;amalur::deadzone(x,y);check(x==0&&y==0,"stick drift suppressed");
    x=1;y=1;amalur::deadzone(x,y);check(closeEnough(x*x+y*y,1),"diagonal normalized");
    x=.6f;y=0;amalur::deadzone(x,y);check(closeEnough(x,.5f)&&y==0,"analog range after dead zone");
    amalur::MotionInputPacket input;input.active=1;input.tick=1000;input.moveX=.5f;
    check(amalur::validMotionInput(input,1100),"fresh input accepted");
    check(!amalur::validMotionInput(input,1300)&&!amalur::validMotionInput(input,900),"stale and future input rejected");
    input.active=0;check(!amalur::validMotionInput(input,1100),"focus loss neutral");
    input.active=1;input.moveX=NAN;check(!amalur::validMotionInput(input,1100),"NaN rejected");
    amalur::TouchMapper mapper;amalur::TouchInput touch;
    uint64_t testNow=1000;auto map=[&](bool gameplay=true){testNow+=100;return mapper.map(touch,true,gameplay,testNow);};
    map(); // Fresh neutral arms controls.
    touch.a=touch.b=touch.x=touch.y=true;touch.leftTrigger=.8f;touch.rightGrip=.9f;touch.leftGrip=1;
    auto padInput=map();padInput.tick=1000;
    check(amalur::validMotionInput(padInput,1100),"valid complete Touch packet");
    check((padInput.buttons&0xf000)==0xf000&&padInput.abilities==.9f&&padInput.selectedWeapon==0,"four spell slots do not change selected weapon");
    check(!(padInput.buttons&XINPUT_GAMEPAD_LEFT_SHOULDER)&&padInput.supportGrip==1&&padInput.block==.8f,"left grip reserved for grabbing, Reckoning chord unchanged");
    touch={};map();touch.y=true;
    auto selected=map();check(selected.selectedWeapon==1&&selected.buttons==0,"Y selects secondary without attacking");
    check(map().selectedWeapon==1,"held Y selects only once");touch={};map();
    touch.rightTrigger=.7f;check(map().buttons==XINPUT_GAMEPAD_Y,"trigger attacks selected secondary");
    touch.y=true;auto hold=map();check(hold.selectedWeapon==0&&hold.buttons==XINPUT_GAMEPAD_Y,"selection change cannot reinterpret held attack");
    touch.y=false;touch.rightTrigger=.5f;check(map().buttons==XINPUT_GAMEPAD_Y,"trigger hysteresis retains attack owner");
    touch.rightTrigger=.4f;check(map().buttons==0,"trigger releases");
    touch.rightTrigger=.7f;check(map().buttons==XINPUT_GAMEPAD_X,"next attack uses new selection");
    touch.rightGrip=1;check(map().abilities==0&&map().buttons==XINPUT_GAMEPAD_X,"gripping during attack cannot turn it into spell");
    touch={};map();touch.rightGrip=1;touch.y=true;
    check(map().buttons==XINPUT_GAMEPAD_Y&&map().abilities==1,"ability Y remains native spell slot");
    touch.rightGrip=0;check(map().buttons==XINPUT_GAMEPAD_Y&&map().abilities==1,"release grip before spell button preserves ability context");
    touch={};check(map().abilities==0,"release complete action ends ability context");
    touch.rightGrip=1;touch.rightTrigger=1;auto spell=map();touch.rightGrip=0;
    check(spell.buttons==XINPUT_GAMEPAD_X&&map().abilities==1,"trigger spell keeps its context until trigger release");
    touch={};map();
    touch.rightX=1;check(map().turnYawDegrees==30,"right stick snaps thirty degrees");
    for(unsigned i=0;i<100;++i)check(map().turnYawDegrees==30,"held snap never repeats");
    touch.rightX=-1;check(map().turnYawDegrees==30,"crossing direction without neutral cannot snap");
    touch.rightX=.3f;map();touch.rightX=-1;check(map().turnYawDegrees==30,"partial release does not rearm");
    touch.rightX=0;map();touch.rightX=-1;check(map().turnYawDegrees==0,"neutral release rearms opposite snap");
    touch={};map();touch.leftX=1;touch.rightThumbrest=true;
    auto shifted=map();check(shifted.moveX==0&&shifted.buttons==XINPUT_GAMEPAD_DPAD_RIGHT,"thumbrest shifts left stick to D-pad");
    touch.rightThumbrest=false;check(map().moveX==0&&map().buttons==0,"release modifier cannot unexpectedly move player");
    touch.leftX=0;map();touch.leftX=1;check(map().moveX==1,"neutral left stick rearms movement");
    touch.leftClick=touch.rightClick=true;auto chord=map();
    check(chord.moveX==0&&chord.buttons==XINPUT_GAMEPAD_DPAD_RIGHT,"stick click fallback suppresses both shortcuts");
    touch.rightClick=false;check(!(map().buttons&XINPUT_GAMEPAD_BACK),"releasing fallback chord cannot accidentally open map");
    touch={};check(map().buttons==0,"full chord release does not fire shortcuts");
    touch.leftClick=true;check(map().buttons==0,"single left click waits for release");
    touch={};check(map().buttons==XINPUT_GAMEPAD_BACK,"single left click release opens map");map();
    touch.rightClick=true;check(map().buttons==0,"single right click waits for release");
    touch={};check(map().buttons==XINPUT_GAMEPAD_RIGHT_SHOULDER,"single right click release toggles stealth");map();
    // Realistic staggered sampling: no shortcut can escape while forming the chord.
    touch.leftClick=true;check(map().buttons==0,"first staggered click cannot open map");
    touch.rightClick=true;touch.leftY=1;check(map().buttons==XINPUT_GAMEPAD_DPAD_UP,"later second click activates shifted D-pad");
    touch.leftClick=false;check((map().buttons&(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_RIGHT_SHOULDER))==0,"partial staggered chord release consumed");
    touch={};check(map().buttons==0,"last staggered chord release consumed");
    amalur::TouchMapper wheelMapper;amalur::TouchInput wheel;
    wheelMapper.map(wheel,true,true,1000);wheel.leftClick=true;
    check(wheelMapper.map(wheel,true,true,1010).buttons==0,"wheel waits for hold");
    check(wheelMapper.map(wheel,true,true,1359).buttons==0,"short hold cannot open wheel");
    check(wheelMapper.map(wheel,true,true,1360).buttons==XINPUT_GAMEPAD_LEFT_SHOULDER,"350ms hold opens wheel");
    wheel.leftY=1;auto wheelMove=wheelMapper.map(wheel,true,true,1400);check(wheelMove.moveY==1,"wheel selection keeps analog stick");
    wheel={};check(wheelMapper.map(wheel,true,true,1410).buttons==0,"wheel release cannot also open map");
    wheel.leftGrip=1;auto grab=wheelMapper.map(wheel,true,true,1420);check(grab.supportGrip==1&&!grab.buttons,"grip has no native menu action");
    check(wheelMapper.map(wheel,false,true,1430).supportGrip==0,"focus loss releases grab");
    // A short XR pulse can be missed by a slower native poll. Keep exactly the
    // release action alive for 80ms, without repeating held-click actions.
    amalur::TouchMapper pulseMapper;amalur::TouchInput click;
    pulseMapper.map(click,true,true,1000);click.leftClick=true;
    check(pulseMapper.map(click,true,true,1001).buttons==0,"pulse mapper defers held click");
    click.leftClick=false;check(pulseMapper.map(click,true,true,1002).buttons==XINPUT_GAMEPAD_BACK,"release begins map pulse");
    check(pulseMapper.map(click,true,true,1081).buttons==XINPUT_GAMEPAD_BACK,"map pulse survives slow native poll");
    check(pulseMapper.map(click,true,true,1082).buttons==0,"map pulse expires after eighty milliseconds");
    click.rightClick=true;pulseMapper.map(click,true,true,1100);click.rightClick=false;
    check(pulseMapper.map(click,true,true,1101).buttons==XINPUT_GAMEPAD_RIGHT_SHOULDER,"stealth release begins bounded pulse");
    check(pulseMapper.map(click,false,true,1102).buttons==0,"focus loss cancels active shortcut pulse");
    check(pulseMapper.map(click,true,true,1103).buttons==0,"focus recovery does not replay shortcut");
    click.leftClick=true;pulseMapper.map(click,true,true,1200);click.leftClick=false;pulseMapper.map(click,true,true,1201);
    check(pulseMapper.map(click,true,false,1202).buttons==0,"menu context change cancels shortcut pulse");
    touch={};touch.menu=true;check(map().buttons==XINPUT_GAMEPAD_START,"left menu remains start");
    touch={};map();touch.y=true;map();touch={};map();
    const auto beforeMenu=map();
    touch.rightTrigger=1;check(map(false).buttons==0,"menu transition cancels held attack");
    check(map(false).buttons==0,"held transition input remains cancelled");
    touch={};map(false);touch.a=touch.b=touch.x=touch.y=true;touch.rightX=1;touch.leftY=.7f;
    auto menu=map(false);check((menu.buttons&0xf000)==0xf000&&menu.selectedWeapon==beforeMenu.selectedWeapon,"menu preserves native face buttons and selection");
    check((menu.buttons&XINPUT_GAMEPAD_DPAD_RIGHT)&&menu.turnYawDegrees==beforeMenu.turnYawDegrees&&menu.moveY>0,"menu right stick navigates without turning");
    check(map(true).buttons==0&&map(true).moveY==0,"return to gameplay requires neutral");
    touch={};map();touch.rightTrigger=1;map();
    auto inactive=mapper.map(touch,false);check(!inactive.active&&!inactive.buttons&&inactive.selectedWeapon==menu.selectedWeapon&&inactive.turnYawDegrees==menu.turnYawDegrees,"focus loss releases controls but retains selection and turn");
    check(map().buttons==0,"regaining focus with held trigger cannot attack");
    touch={};map();touch.rightTrigger=1;check(map().buttons!=0,"fresh trigger works after neutral");
    touch.rightTrigger=NAN;auto invalidTouch=map();check(!invalidTouch.active&&invalidTouch.buttons==0,"nonfinite hardware input cancels safely");
    XINPUT_GAMEPAD real{};real.wButtons=XINPUT_GAMEPAD_B;real.sThumbLX=1234;
    amalur::mergeMotion(real,padInput);check(real.sThumbLX==1234&&(real.wButtons&XINPUT_GAMEPAD_B),"physical pad preserved with neutral XR stick");
    padInput.buttons|=0x80000000;check(!amalur::validMotionInput(padInput,1100),"unknown buttons rejected");
    auto invalidTurn=padInput;invalidTurn.buttons=0;invalidTurn.turnYawDegrees=INFINITY;check(!amalur::validMotionInput(invalidTurn,1100),"nonfinite turn rejected");
    auto invalidSelection=padInput;invalidSelection.buttons=0;invalidSelection.selectedWeapon=2;check(!amalur::validMotionInput(invalidSelection,1100),"unknown selected weapon rejected");
    padInput.buttons=0;padInput.abilities=NAN;check(!amalur::validMotionInput(padInput,1100),"invalid analog trigger rejected");
    const float s=std::sqrt(.5f);
    for(auto forward:{mgs5vr::Vec3{0,1,0},mgs5vr::Vec3{-1,0,0}}){
        amalur::CameraPose rig{{10,20,170},{10+forward.x*200,20+forward.y*200,170},{0,0,1}};
        mgs5vr::Pose grip;check(amalur::gripInGame(rig,{{0,s,0,s},{.2f,-.3f,-.4f}},100,grip),"valid grip mapping");
        amalur::CameraPose camera;check(amalur::trackedCamera(rig,{{0,s,0,s},{.2f,-.3f,-.4f}},100,camera),"comparison camera");
        auto direction=mgs5vr::rotate(grip.orientation,{0,1,0});auto expected=camera.target-camera.eye;amalur::normalize(expected);
        check(closeEnough(direction.x,expected.x)&&closeEnough(direction.y,expected.y)&&closeEnough(direction.z,expected.z),"weapon and head share yaw axes");
        check(closeEnough(grip.position.x,camera.eye.x)&&closeEnough(grip.position.y,camera.eye.y)&&closeEnough(grip.position.z,camera.eye.z),"hand and head share positional origin");
        auto local=mgs5vr::compose(mgs5vr::inverse(grip),grip);check(closeEnough(local.position.x,0)&&closeEnough(local.orientation.w,1),"attachment inverse cancels world transform");
    }
    puts("PASS: Touch weapon ownership, spell context, snap rearm, shifted D-pad, focus/menu cancellation, packet validation and pose consistency");
}
