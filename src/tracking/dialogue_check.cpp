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
    check(near(dialogueEntryFacing(player,{100,250,500},{1,0,0}),{0,1,0}),"off-axis NPC seeds entry heading, independent of NPC height");
    check(near(dialogueEntryFacing(player,player,{1,0,0}),{1,0,0}),"coincident NPC keeps valid player-facing fallback");
    check(near(dialogueEntryFacing(player,{std::numeric_limits<float>::quiet_NaN(),0,0},{1,0,0}),{1,0,0}),"invalid NPC position keeps fallback");
    check(view.apply(1,7,55,0,0,player,{1,0,0},head,100,a),"entry valid");
    check(near(a.eye,{115,200,485})&&near(a.target-a.eye,{200,0,0}),"eye belongs to player, not side camera");
    check(near(dialogueBodyAnchor(a.eye,view.heading),{107,200,495}),
        "dialogue visual body starts at gameplay body reference");
    head.position.x=.1f;
    check(view.apply(1,7,55,0,0,player,{0,1,0},head,100,b),"camera cut valid");
    check(near(b.eye-a.eye,{0,10,0})&&near(b.target-b.eye,a.target-a.eye),"native cut/facing change does not rotate viewer; physical translation retained");
    check(near(dialogueBodyAnchor(b.eye,view.heading)-dialogueBodyAnchor(a.eye,view.heading),{0,10,0}),
        "physical headset movement carries the visual body during dialogue");
    CameraPose recordChange{};check(view.apply(88,7,55,0,0,player,{0,1,0},head,100,recordChange)&&near(recordChange.eye,b.eye),"new dialogue record with same actors preserves origin and heading");
    DialogueView resetView;Pose initial{},lean{},resetPose{};CameraPose beforeReset{},afterReset{};
    check(resetView.apply(9,7,55,0,0,player,{1,0,0},initial,100,beforeReset,1000),"reference-space fixture entry");
    lean.position={.1f,0,.13f};lean.orientation={0,std::sin(-.052f),0,std::cos(-.052f)};
    check(resetView.apply(9,7,55,0,0,player,{1,0,0},lean,100,beforeReset,2000),"ordinary slow head motion retained");
    check(resetView.apply(9,7,55,0,0,player,{1,0,0},resetPose,100,afterReset,2015)&&
        resetView.continuityAdjusted&&near(beforeReset.eye,afterReset.eye)&&
        near(beforeReset.target-beforeReset.eye,afterReset.target-afterReset.eye),
        "unannounced 16 cm tracking origin reset preserves eye and heading");
    resetPose.position.x=.01f;
    check(resetView.apply(9,7,55,0,0,player,{1,0,0},resetPose,100,afterReset,2115)&&
        !resetView.continuityAdjusted&&!near(beforeReset.eye,afterReset.eye),
        "physical head motion remains live after tracking origin reset");
    DialogueView moved;CameraPose beforeMove{},afterMove{};Pose stationary{};
    check(moved.apply(9,7,55,0,0,player,{1,0,0},stationary,100,beforeMove),"moving actor entry valid");
    check(moved.apply(9,7,55,0,0,{2100,200,400},{0,1,0},stationary,100,afterMove)&&
        near(afterMove.eye-beforeMove.eye,{2000,0,100}),"dialogue eye follows actor teleport and height change");
    check(view.apply(1,7,55,1,0,player,{0,1,0},head,100,b)&&near(b.eye,a.eye),"local recenter rebases position without native cut");
    head.position.x=.3f;
    check(view.apply(1,7,55,1,1,player,{0,1,0},head,100,b)&&near(b.eye,a.eye),"bridge recenter independently rebases origin");
    head.orientation={0,std::sin(.25f),0,std::cos(.25f)};
    check(view.apply(1,7,55,1,1,player,{1,0,0},head,100,b)&&!near(b.target-b.eye,a.target-a.eye),"head yaw remains live");
    check(view.apply(2,7,56,1,1,player,{0,1,0},head,100,b)&&near(b.target-b.eye,{0,200,0}),"new participant rebases heading");
    check(view.apply(2,8,55,1,1,player,{-1,0,0},head,100,b)&&near(b.target-b.eye,{-200,0,0}),"new player owner cannot retain old heading");
    view.reset();
    check(view.apply(2,8,55,1,1,player,{1,0,0},head,100,b)&&near(b.target-b.eye,{200,0,0}),"exit/reentry resets even reused dialogue pointer");
    check(!view.apply(0,8,55,1,1,player,{1,0,0},head,100,b),"missing conversation rejected");
    check(!view.apply(2,8,55,1,1,player,{1,0,0},head,0,b),"invalid scale rejected");
    head.position.x=std::numeric_limits<float>::quiet_NaN();
    check(!view.apply(2,8,55,1,1,player,{1,0,0},head,100,b),"invalid pose rejected");
    SnapHeading snap;auto before=snap.apply({1,0,0},1,0);before=snap.apply({1,0,0},1,30);
    snap.rebase(1,120);
    check(near(snap.apply({1,0,0},1,120),before),"dialogue stick turns discarded on exit");
    check(!near(snap.apply({1,0,0},1,150),before),"new gameplay turns still apply");
    std::puts("PASS: dialogue eye anchor, native cuts, live head movement, recenter, owner/exit reset and snap-turn resume");
}
