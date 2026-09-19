#include "arm_pose.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace mgs5vr;
void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
float distance(Vec3 a,Vec3 b){auto d=a-b;return std::sqrt(dot(d,d));}
int main(){
    Pose trim;
    check(amalur::gripAngleTrim(0,0,0,trim)&&distance(rotate(trim.orientation,{0,1,0}),{0,1,0})<.001f,"neutral grip trim is deterministic identity");
    check(amalur::gripAngleTrim(90,0,0,trim)&&distance(rotate(trim.orientation,{0,1,0}),{0,0,1})<.001f,"grip pitch rotates forward upward");
    check(amalur::gripAngleTrim(0,90,0,trim)&&distance(rotate(trim.orientation,{0,1,0}),{-1,0,0})<.001f,"grip yaw rotates in local horizontal plane");
    check(amalur::gripAngleTrim(0,0,90,trim)&&distance(rotate(trim.orientation,{0,0,1}),{1,0,0})<.001f,"grip roll rotates around forward axis");
    check(!amalur::gripAngleTrim(NAN,0,0,trim)&&!amalur::gripAngleTrim(0,181,0,trim),"invalid grip angles rejected");
    check(amalur::gripAngleTrim(30,-45,20,trim),"combined trim valid");
    Pose grip{{0,0,.70710678f,.70710678f},{12,34,56}};
    const auto adjusted=compose(grip,trim);
    check(distance(adjusted.position,grip.position)<.001f,"trim changes wrist angle without moving controller anchor");
    check(distance(rotate(adjusted.orientation,{0,1,0}),rotate(grip.orientation,rotate(trim.orientation,{0,1,0})))<.001f,"trim is controller-local independent of world heading");
    amalur::RigBone bones[6]{},out[6]{};
    int16_t parents[]{-1,0,1,2,3,0};uint32_t ids[]{1,0xf21468,0xd1f75e,0x88d0eb,2,3};
    bones[1].position={0,20,155};bones[2].position={0,30,125};bones[3].position={0,40,100};
    bones[4].position={2,40,96};bones[5].position={-10,-30,110};
    for(auto& b:bones){b.positionW=7;for(unsigned i=0;i<16;++i)b.opaque[i]=static_cast<unsigned char>(0x80+i);}
    amalur::RigBone original[6];std::memcpy(original,bones,sizeof(bones));
    const Pose target{{0,0,.38268343f,.92387953f},{35,40,140}};
    check(amalur::solveRightArm(bones,out,6,parents,ids,target,100),"reachable target");
    check(distance(out[3].position,target.position)<.001f,"wrist reaches controller");
    auto wristDirection=rotate(amalur::bonePose(out[3]).orientation,{1,0,0});
    auto targetDirection=rotate(target.orientation,{1,0,0});
    check(distance(wristDirection,targetDirection)<.001f,"solved wrist orientation survives native quaternion storage");
    check(std::abs(distance(out[1].position,out[2].position)-distance(bones[1].position,bones[2].position))<.001f,"upper arm length preserved");
    check(std::abs(distance(out[2].position,out[3].position)-distance(bones[2].position,bones[3].position))<.001f,"forearm length preserved");
    check(std::abs(distance(out[4].position,out[3].position)-distance(bones[4].position,bones[3].position))<.001f,"finger relative distance preserved");
    check(!std::memcmp(bones,original,sizeof(bones)),"native animation not mutated");
    check(!std::memcmp(out+5,bones+5,sizeof(bones[5])),"unrelated limb untouched");
    for(unsigned i=0;i<6;++i)check(out[i].positionW==7&&!std::memcmp(out[i].opaque,bones[i].opaque,16),"opaque bone bytes preserved");
    amalur::ArmReference reference;const Vec3 anchor{0,0,170};
    check(amalur::captureRightArmReference(bones,6,parents,ids,anchor,reference),"neutral arm reference captured");
    amalur::RigBone stable[6],attack[6],attackOriginal[6];
    check(amalur::solveRightArm(bones,stable,6,parents,ids,target,100,&reference,anchor),"reference arm solved");
    std::memcpy(attack,bones,sizeof(attack));
    // Model a spell animation which pulls the shoulder forward and sweeps the
    // whole arm upward. It must not change the controller-driven visual arm.
    for(unsigned i=1;i<=4;++i){attack[i].position=attack[i].position+Vec3{28,-15,20};attack[i].orientation={0,0,.70710678f,.70710678f};}
    attack[4].position=attack[4].position+Vec3{10,5,-8}; // Animated finger/socket moves independently.
    std::memcpy(attackOriginal,attack,sizeof(attack));
    check(amalur::solveRightArm(attack,out,6,parents,ids,target,100,&reference,anchor),"cast pose solved against neutral reference");
    for(unsigned i=1;i<=3;++i){
        check(distance(out[i].position,stable[i].position)<.001f,"cast animation cannot move controlled arm joints");
        check(distance(rotate(amalur::bonePose(out[i]).orientation,{1,0,0}),rotate(amalur::bonePose(stable[i]).orientation,{1,0,0}))<.001f,"cast animation cannot rotate controlled arm joints");
    }
    check(distance(out[4].position,stable[4].position)<.001f,"cast finger animation cannot move weapon socket");
    check(distance(rotate(amalur::bonePose(out[4]).orientation,{0,1,0}),rotate(amalur::bonePose(stable[4]).orientation,{0,1,0}))<.001f,"cast finger animation cannot rotate weapon socket");
    check(!std::memcmp(attack,attackOriginal,sizeof(attack)),"native attack pose remains untouched");
    check(!std::memcmp(out+5,attack+5,sizeof(attack[5])),"reference leaves other limbs animated");
    const Vec3 movedAnchor=anchor+Vec3{12,-23,7};auto movedTarget=target;movedTarget.position=movedTarget.position+(movedAnchor-anchor);
    check(amalur::solveRightArm(attack,out,6,parents,ids,movedTarget,100,&reference,movedAnchor),"reference follows body anchor");
    for(unsigned i=1;i<=3;++i)check(distance(out[i].position,stable[i].position+(movedAnchor-anchor))<.001f,"anchor shift translates solved arm coherently");
    amalur::ArmReference missing;
    check(!amalur::solveRightArm(bones,out,6,parents,ids,target,100,&missing,anchor),"uncaptured reference rejected");
    check(amalur::solveRightArm(bones,out,6,parents,ids,{{},{1000,40,150}},100),"distant target clamps");
    check(distance(out[1].position,out[3].position)<distance(bones[1].position,bones[2].position)+distance(bones[2].position,bones[3].position),"reach bounded");
    check(amalur::solveRightArm(bones,out,6,parents,ids,{{},bones[1].position},100),"coincident shoulder target stays finite");
    check(!amalur::solveRightArm(bones,out,6,parents,ids,target,0),"invalid scale rejected");
    auto invalid=target;invalid.position.x=std::numeric_limits<float>::quiet_NaN();
    check(!amalur::solveRightArm(bones,out,6,parents,ids,invalid,100),"NaN target rejected");
    parents[2]=2;check(!amalur::solveRightArm(bones,out,6,parents,ids,target,100),"cyclic hierarchy rejected");parents[2]=1;
    ids[4]=ids[3];check(!amalur::solveRightArm(bones,out,6,parents,ids,target,100),"ambiguous wrist ID rejected");
    // Two independent arm chains from the captured player skeleton IDs.
    amalur::RigBone both[9]{},rightOnly[9]{},dual[9]{},reverse[9]{},leftOnly[9]{};
    int16_t dualParents[]{-1,0,1,2,3,0,5,6,7};
    uint32_t dualIds[]{1,0xf21468,0xd1f75e,0x88d0eb,2,0xf1076a,0xd0ea60,0x87c3ed,3};
    for(unsigned i=1;i<=4;++i){both[i]=original[i];both[i+4]=original[i];both[i+4].position.y=-both[i].position.y;}
    amalur::ArmReference rightRef,leftRef;
    check(amalur::captureRightArmReference(both,9,dualParents,dualIds,anchor,rightRef)&&
        amalur::captureLeftArmReference(both,9,dualParents,dualIds,anchor,leftRef),"capture independent left and right references");
    Pose rightTarget{{},{35,40,140}},leftTarget{{},{35,-40,140}};
    check(amalur::solveRightArm(both,rightOnly,9,dualParents,dualIds,rightTarget,100,&rightRef,anchor)&&
        amalur::solveLeftArm(rightOnly,dual,9,dualParents,dualIds,leftTarget,100,&leftRef,anchor),"solve both hands sequentially");
    check(distance(dual[3].position,rightTarget.position)<.001f&&distance(dual[7].position,leftTarget.position)<.001f,"both wrists reach independent targets");
    check(!std::memcmp(dual+1,rightOnly+1,4*sizeof(both[0])),"left solve preserves previously solved right arm byte-for-byte");
    check(distance(dual[6].position,{dual[2].position.x,-dual[2].position.y,dual[2].position.z})<.001f,"left elbow pole mirrors right elbow across body");
    check(amalur::solveLeftArm(both,leftOnly,9,dualParents,dualIds,leftTarget,100,&leftRef,anchor)&&
        amalur::solveRightArm(leftOnly,reverse,9,dualParents,dualIds,rightTarget,100,&rightRef,anchor),"solve hands in reverse order");
    check(!std::memcmp(dual,reverse,sizeof(dual)),"arm solve order does not change either hand");
    check(!amalur::solveLeftArm(both,dual,9,dualParents,dualIds,leftTarget,100,&rightRef,anchor),"right calibration rejected for left hand");
    auto leftRotated=leftTarget;leftRotated.orientation={.38268343f,0,0,.92387953f};
    check(amalur::solveLeftArm(both,dual,9,dualParents,dualIds,leftRotated,100,&leftRef,anchor),"left wrist orientation follows controller");
    check(distance(rotate(amalur::bonePose(dual[7]).orientation,{0,1,0}),rotate(leftRotated.orientation,{0,1,0}))<.001f,"left native quaternion storage preserves controller orientation");
    check(distance(dual[8].position,compose(amalur::bonePose(dual[7]),leftRef.handRelative[8]).position)<.001f,"left finger attachment follows wrist");
    dualIds[8]=dualIds[7];check(!amalur::solveLeftArm(both,dual,9,dualParents,dualIds,leftTarget,100),"ambiguous left wrist rejected");
    std::puts("PASS: bilateral arm reach, independent mirrored elbow poles, native quaternion storage, animation-independent reference, moving body anchor, immutable source and input guards");
}
