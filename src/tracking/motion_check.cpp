#include "motion_input.hpp"
#include "weapon_pose.hpp"
#include "camera_inputs.hpp"
#include <cstdio>
#include <cstdlib>
static void check(bool pass,const char* label){if(!pass){printf("FAIL: %s\n",label);std::exit(1);}}
static bool closeEnough(float a,float b){return std::abs(a-b)<.002f;}
int main(){
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
    touch.a=touch.b=touch.x=touch.y=true;touch.leftTrigger=.8f;touch.rightGrip=.9f;touch.leftGrip=1;
    auto padInput=mapper.map(touch,true);padInput.tick=1000;
    check(amalur::validMotionInput(padInput,1100),"valid complete Touch packet");
    check((padInput.buttons&0xf000)==0xf000&&padInput.abilities==.9f,"four spell face buttons preserved under modifier");
    check((padInput.buttons&XINPUT_GAMEPAD_LEFT_SHOULDER)&&padInput.block==.8f,"bank switch and Reckoning chord available");
    touch={};touch.rightTrigger=.7f;check(mapper.map(touch,true).buttons&XINPUT_GAMEPAD_X,"right trigger attacks");
    touch.rightTrigger=.5f;check(mapper.map(touch,true).buttons&XINPUT_GAMEPAD_X,"trigger hysteresis holds");
    touch.rightTrigger=.4f;check(!(mapper.map(touch,true).buttons&XINPUT_GAMEPAD_X),"trigger releases");
    touch.menu=touch.rightClick=touch.leftClick=true;touch.rightX=-1;touch.rightY=1;
    auto shortcuts=mapper.map(touch,true);
    check(shortcuts.buttons==(XINPUT_GAMEPAD_START|XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_DPAD_UP),"menu map stealth and D-pad mapping");
    check(!mapper.map(touch,false).active&&mapper.map(touch,false).buttons==0,"overlay/focus loss releases all controls");
    XINPUT_GAMEPAD real{};real.wButtons=XINPUT_GAMEPAD_B;real.sThumbLX=1234;
    amalur::mergeMotion(real,padInput);check(real.sThumbLX==1234&&(real.wButtons&XINPUT_GAMEPAD_B),"physical pad preserved with neutral XR stick");
    padInput.buttons|=0x80000000;check(!amalur::validMotionInput(padInput,1100),"unknown buttons rejected");
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
    puts("PASS: camera input restoration, analog dead zone, input expiry/focus, and weapon/head coordinate consistency");
}
