#include "cinematic_view.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
static void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static bool near(amalur::Vec3 a,amalur::Vec3 b){const auto d=a-b;return mgs5vr::dot(d,d)<1e-5f;}
int main(){
    using namespace amalur;
    CinematicView view;Pose head{};CameraPose a{},b{};
    CameraPose authored{{10,20,30},{110,20,80},{0,1,0}};
    check(view.apply(1,2,0,0,authored,head,100,a),"scene entry");
    check(near(a.eye,authored.eye)&&near(a.target-a.eye,{200,0,0})&&near(a.up,{0,0,1}),"native location with level horizon");
    authored.eye={20,30,40};authored.target={20,130,-60};
    check(view.apply(1,2,0,0,authored,head,100,b),"animated scene camera");
    check(near(b.eye,authored.eye)&&near(b.target-b.eye,a.target-a.eye),"native travel retained without forced head rotation");
    head.position.x=.1f;
    check(view.apply(1,2,0,0,authored,head,100,b)&&near(b.eye,authored.eye+Vec3{0,10,0}),"physical head translation");
    head.orientation={0,std::sin(.2f),0,std::cos(.2f)};
    check(view.apply(1,2,0,0,authored,head,100,b)&&!near(b.target-b.eye,a.target-a.eye),"head rotation remains free");
    check(view.apply(1,2,1,0,authored,head,100,b)&&near(b.eye,authored.eye)&&near(b.target-b.eye,{200,0,0}),"local recenter");
    head.position.x=.3f;
    check(view.apply(1,2,1,1,authored,head,100,b)&&near(b.eye,authored.eye),"runtime recenter");
    check(view.apply(3,2,1,1,authored,head,100,b)&&near(b.target-b.eye,{0,200,0}),"new scene gets its own heading");
    authored.target={-80,30,40};
    check(view.apply(3,4,1,1,authored,head,100,b)&&near(b.target-b.eye,{-200,0,0}),"new owned camera resets");
    view.reset();authored.target={120,30,40};
    check(view.apply(3,4,1,1,authored,head,100,b)&&near(b.target-b.eye,{200,0,0}),"exit and pointer reuse");
    check(!view.apply(0,4,1,1,authored,head,100,b),"missing scene rejected");
    check(!view.apply(3,0,1,1,authored,head,100,b),"missing camera rejected");
    check(!view.apply(3,4,1,1,authored,head,0,b),"invalid scale rejected");
    authored.eye.x=std::numeric_limits<float>::quiet_NaN();
    check(!view.apply(3,4,1,1,authored,head,100,b),"invalid native camera rejected");
    std::puts("PASS: cinematic head-look, horizon, native travel, physical translation, recenter and scene/camera lifetimes");
}
