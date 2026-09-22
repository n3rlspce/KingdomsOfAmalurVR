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
    int npcYaw{};
    check(dialogueNpcYaw({16000.9355f,-30515.3379f,5944.18f},{16129.2061f,-30556.1895f,6131.3f},npcYaw)&&npcYaw==342,"live off-axis NPC turns toward tracked eye, not a fixed seven-degree offset");
    check(dialogueNpcYaw({0,0,0},{0,10,800},npcYaw)&&npcYaw==90,"NPC body yaw ignores height");
    check(dialogueNpcYaw({0,0,0},{0,-10,0},npcYaw)&&npcYaw==270,"negative yaw wraps for native degree API");
    check(!dialogueNpcYaw(player,player,npcYaw),"coincident positions do not turn actor");
    check(!dialogueNpcYaw(player,{std::numeric_limits<float>::quiet_NaN(),0,0},npcYaw),"invalid position cannot enter native facing service");
    DialogueNpcFollow follow;uint32_t turn{};
    const auto turnDegrees=[](uint32_t bits){return double(static_cast<int32_t>(bits))*360.0/4294967296.0;};
    check(!follow.step(1,42,1000,0,{0,0,0},{0,10,0},turn),"tracking starts without snapping actor");
    check(follow.step(1,42,1010,0,{0,0,0},{0,10,0},turn)&&std::abs(turnDegrees(turn)-.6)<1e-5,"native fractional turn respects 60 degrees per second");
    check(!follow.step(1,42,1010,0,{0,0,0},{0,10,0},turn),"duplicate camera rebuild does not add turn");
    check(follow.step(1,42,5010,0,{0,0,0},{0,10,0},turn)&&std::abs(turnDegrees(turn)-3)<1e-5,"long stalls cannot cause large catch-up turns");
    check(follow.step(1,42,5020,359.5,{0,0,0},{10,0,0},turn)&&std::abs(turnDegrees(turn)-.5)<1e-5,"follows shortest direction across zero degrees");
    check(follow.step(1,42,5030,.5,{0,0,0},{10,0,0},turn)&&std::abs(turnDegrees(turn)+.5)<1e-5,"negative fine turns preserve binary angle sign");
    check(!follow.step(1,42,5040,.1,{0,0,0},{10,0,0},turn),"tiny pose noise does not jitter body");
    check(!follow.step(1,43,5050,90,{0,0,0},{10,0,0},turn),"NPC identity changes discard old timing");
    follow.reset();
    check(!follow.step(1,43,9000,90,{0,0,0},{10,0,0},turn),"tracking resumes without catch-up after loss or dialogue exit");
    check(!follow.step(1,43,9010,90,player,player,turn)&&!follow.active,"coincident positions disable follow");
    check(near(dialogueEntryFacing(player,{100,250,500},{1,0,0}),{0,1,0}),"off-axis NPC seeds entry heading, independent of NPC height");
    check(near(dialogueEntryFacing(player,player,{1,0,0}),{1,0,0}),"coincident NPC keeps valid player-facing fallback");
    check(near(dialogueEntryFacing(player,{std::numeric_limits<float>::quiet_NaN(),0,0},{1,0,0}),{1,0,0}),"invalid NPC position keeps fallback");
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
