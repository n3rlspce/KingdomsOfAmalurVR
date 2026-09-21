#include "src/tracking/locomotion_frame.hpp"
#include <cstdio>
#include <cstdlib>
using namespace mgs5vr;
void check(bool b,const char* s){if(!b){printf("FAIL %s\n",s);exit(1);}}
float error(Vec3 a,Vec3 b){auto d=a-b;return std::sqrt(dot(d,d));}
int main(){
    amalur::LocomotionFrame source{{{},{16000,-30000,6100}},99,2,1000,true};
    auto drawn=source;drawn.tick+=16;drawn.pose.position=drawn.pose.position+Vec3{5,-6,0};
    const Pose relativeHand{{},{20,30,-40}};auto oldHand=compose(source.pose,relativeHand);
    amalur::RigBone root{};root.position={16000,-30000,5900};root.orientation=amalur::nativeQuaternion({});root.opaque[0]=91;
    auto original=root;auto localHand=compose(inverse(amalur::bonePose(root)),oldHand);
    check(amalur::advanceLocomotion(root,source,drawn),"moving frame accepted");
    check(error(compose(amalur::bonePose(root),localHand).position,compose(drawn.pose,relativeHand).position)<.005f,"walking compensation maintains camera-relative hand");
    check(root.opaque[0]==91,"native scale metadata preserved");
    // Native root can turn independently of the captured locomotion basis.
    // A snap turn changes the basis; rotate the old solved world pose once.
    drawn.pose.orientation={0,0,.70710678f,.70710678f};root=original;
    check(amalur::advanceLocomotion(root,source,drawn),"snap turn accepted");
    check(error(compose(amalur::bonePose(root),localHand).position,compose(drawn.pose,relativeHand).position)<.015f,"rotation and translation rebase together");
    root=original;auto noMotion=source;noMotion.tick+=16;
    check(amalur::advanceLocomotion(root,source,noMotion)&&error(root.position,original.position)<.005f,"head motion alone does not move locomotion basis");
    root=original;auto bad=drawn;++bad.center;
    check(!amalur::advanceLocomotion(root,source,bad)&&!memcmp(&root,&original,48),"recenter rejects transactionally");
    bad=drawn;++bad.owner;check(!amalur::advanceLocomotion(root,source,bad),"owner change rejected");
    bad=drawn;bad.tick=999;check(!amalur::advanceLocomotion(root,source,bad),"older camera rejected");
    bad=drawn;bad.tick=1250;check(!amalur::advanceLocomotion(root,source,bad),"stale pairing rejected");
    bad=drawn;bad.valid=false;check(!amalur::advanceLocomotion(root,source,bad),"missing camera provenance rejected");
    puts("PASS: walking, snap turn, stationary basis, metadata preservation and provenance guards");
}
