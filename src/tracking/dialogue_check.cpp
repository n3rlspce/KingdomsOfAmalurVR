#include "dialogue_view.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
static void check(bool ok,const char* msg){if(!ok){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}}
static bool near(mgs5vr::Vec3 a,mgs5vr::Vec3 b){auto d=a-b;return mgs5vr::dot(d,d)<1e-5f;}
int main(){
    using namespace amalur;
    DialogueView view;CameraPose a{},b{};Pose head{};
    const Vec3 player{100,200,300};
    check(view.apply(1,7,0,0,player,{1,0,0},head,100,a),"entry valid");
    check(near(a.eye,{115,200,485})&&near(a.target-a.eye,{200,0,0}),"eye belongs to player, not side camera");
    head.position.x=.1f;
    check(view.apply(1,7,0,0,player,{0,1,0},head,100,b),"camera cut valid");
    check(near(b.eye-a.eye,{0,10,0})&&near(b.target-b.eye,a.target-a.eye),"native cut/facing change does not rotate viewer; physical translation retained");
    check(view.apply(1,7,1,0,player,{0,1,0},head,100,b)&&near(b.eye,a.eye),"local recenter rebases position without native cut");
    head.position.x=.3f;
    check(view.apply(1,7,1,1,player,{0,1,0},head,100,b)&&near(b.eye,a.eye),"bridge recenter independently rebases origin");
    head.orientation={0,std::sin(.25f),0,std::cos(.25f)};
    check(view.apply(1,7,1,1,player,{1,0,0},head,100,b)&&!near(b.target-b.eye,a.target-a.eye),"head yaw remains live");
    check(view.apply(2,7,1,1,player,{0,1,0},head,100,b)&&near(b.target-b.eye,{0,200,0}),"new conversation rebases heading");
    check(view.apply(2,8,1,1,player,{-1,0,0},head,100,b)&&near(b.target-b.eye,{-200,0,0}),"new player owner cannot retain old heading");
    view.reset();
    check(view.apply(2,8,1,1,player,{1,0,0},head,100,b)&&near(b.target-b.eye,{200,0,0}),"exit/reentry resets even reused dialogue pointer");
    check(!view.apply(0,8,1,1,player,{1,0,0},head,100,b),"missing conversation rejected");
    check(!view.apply(2,8,1,1,player,{1,0,0},head,0,b),"invalid scale rejected");
    head.position.x=std::numeric_limits<float>::quiet_NaN();
    check(!view.apply(2,8,1,1,player,{1,0,0},head,100,b),"invalid pose rejected");
    SnapHeading snap;auto before=snap.apply({1,0,0},1,0);before=snap.apply({1,0,0},1,30);
    snap.rebase(1,120);
    check(near(snap.apply({1,0,0},1,120),before),"dialogue stick turns discarded on exit");
    check(!near(snap.apply({1,0,0},1,150),before),"new gameplay turns still apply");
    std::puts("PASS: dialogue eye anchor, native cuts, live head movement, recenter, owner/exit reset and snap-turn resume");
}
