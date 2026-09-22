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
    // Same scene AND camera address are reused by authored shot cuts.
    CinematicView cuts;head={};authored={{0,0,0},{100,0,0},{0,0,1}};
    check(cuts.apply(1,2,0,0,authored,head,100,a,1000),"shot entry");
    head.orientation={0,std::sin(.4f),0,std::cos(.4f)};
    check(cuts.apply(1,2,0,0,authored,head,100,b,1016)&&!near(b.target-b.eye,{200,0,0}),"head motion does not cut");
    authored.target={0,100,0};
    check(cuts.apply(1,2,0,0,authored,head,100,b,1032)&&near(b.target-b.eye,{0,200,0}),"same-camera reverse shot rebases current head yaw");
    check(cuts.apply(1,2,0,0,authored,head,100,b,1048)&&near(b.target-b.eye,{0,200,0}),"cut is applied only once");
    head.orientation={0,std::sin(.5f),0,std::cos(.5f)};
    check(cuts.apply(1,2,0,0,authored,head,100,b,1064)&&!near(b.target-b.eye,{0,200,0}),"free look after cut");
    // Gradually sweep 90 degrees: never rebase on accumulated pan motion.
    for(unsigned i=1;i<=90;++i){float angle=(90.f+i)*.01745329252f;authored.eye.x+=1;authored.target=authored.eye+Vec3{100*std::cos(angle),100*std::sin(angle),0};
        check(cuts.apply(1,2,0,0,authored,head,100,b,1064+i*16),"continuous pan");
        check(near(cuts.heading,{0,1,0}),"continuous pan never snaps head");}
    authored.eye.x+=200;authored.target.x+=200;
    check(cuts.apply(1,2,0,0,authored,head,100,b,2520)&&near(b.target-b.eye,{-200,0,0}),"position cut gets new intended heading");
    const auto saved=cuts.heading;authored.target=authored.eye+Vec3{100,0,0};
    check(cuts.apply(1,2,0,0,authored,head,100,b,4000)&&near(cuts.heading,saved),"long timing gap alone is not a cut");
    std::puts("PASS: cinematic head-look, horizon, native travel, physical translation, recenter and scene/camera lifetimes");
}
