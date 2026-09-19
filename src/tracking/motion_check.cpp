#include "motion_input.hpp"
#include "weapon_pose.hpp"
#include <cstdio>
#include <cstdlib>
static void check(bool pass,const char* label){if(!pass){printf("FAIL: %s\n",label);std::exit(1);}}
static bool closeEnough(float a,float b){return std::abs(a-b)<.002f;}
int main(){
    float x=.1f,y=.1f;amalur::deadzone(x,y);check(x==0&&y==0,"stick drift suppressed");
    x=1;y=1;amalur::deadzone(x,y);check(closeEnough(x*x+y*y,1),"diagonal normalized");
    x=.6f;y=0;amalur::deadzone(x,y);check(closeEnough(x,.5f)&&y==0,"analog range after dead zone");
    amalur::MotionInputPacket input;input.active=1;input.tick=1000;input.moveX=.5f;
    check(amalur::validMotionInput(input,1100),"fresh input accepted");
    check(!amalur::validMotionInput(input,1300)&&!amalur::validMotionInput(input,900),"stale and future input rejected");
    input.active=0;check(!amalur::validMotionInput(input,1100),"focus loss neutral");
    input.active=1;input.moveX=NAN;check(!amalur::validMotionInput(input,1100),"NaN rejected");
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
    puts("PASS: analog dead zone, input expiry/focus, and weapon/head coordinate consistency");
}
