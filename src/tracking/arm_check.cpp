#include "arm_pose.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace mgs5vr;
void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
float distance(Vec3 a,Vec3 b){auto d=a-b;return std::sqrt(dot(d,d));}
int main(){
    amalur::RigBone bones[6]{},out[6]{};
    int16_t parents[]{-1,0,1,2,3,0};uint32_t ids[]{1,0xf21468,0xd1f75e,0x88d0eb,2,3};
    bones[1].position={0,20,155};bones[2].position={0,30,125};bones[3].position={0,40,100};
    bones[4].position={2,40,96};bones[5].position={-10,-30,110};
    for(auto& b:bones){b.positionW=7;for(unsigned i=0;i<16;++i)b.opaque[i]=static_cast<unsigned char>(0x80+i);}
    amalur::RigBone original[6];std::memcpy(original,bones,sizeof(bones));
    const Pose target{{0,0,.38268343f,.92387953f},{35,40,140}};
    check(amalur::solveRightArm(bones,out,6,parents,ids,target,100),"reachable target");
    check(distance(out[3].position,target.position)<.001f,"wrist reaches controller");
    check(std::abs(distance(out[1].position,out[2].position)-distance(bones[1].position,bones[2].position))<.001f,"upper arm length preserved");
    check(std::abs(distance(out[2].position,out[3].position)-distance(bones[2].position,bones[3].position))<.001f,"forearm length preserved");
    check(std::abs(distance(out[4].position,out[3].position)-distance(bones[4].position,bones[3].position))<.001f,"finger relative distance preserved");
    check(!std::memcmp(bones,original,sizeof(bones)),"native animation not mutated");
    check(!std::memcmp(out+5,bones+5,sizeof(bones[5])),"unrelated limb untouched");
    for(unsigned i=0;i<6;++i)check(out[i].positionW==7&&!std::memcmp(out[i].opaque,bones[i].opaque,16),"opaque bone bytes preserved");
    check(amalur::solveRightArm(bones,out,6,parents,ids,{{},{1000,40,150}},100),"distant target clamps");
    check(distance(out[1].position,out[3].position)<distance(bones[1].position,bones[2].position)+distance(bones[2].position,bones[3].position),"reach bounded");
    check(amalur::solveRightArm(bones,out,6,parents,ids,{{},bones[1].position},100),"coincident shoulder target stays finite");
    check(!amalur::solveRightArm(bones,out,6,parents,ids,target,0),"invalid scale rejected");
    auto invalid=target;invalid.position.x=std::numeric_limits<float>::quiet_NaN();
    check(!amalur::solveRightArm(bones,out,6,parents,ids,invalid,100),"NaN target rejected");
    parents[2]=2;check(!amalur::solveRightArm(bones,out,6,parents,ids,target,100),"cyclic hierarchy rejected");parents[2]=1;
    ids[4]=ids[3];check(!amalur::solveRightArm(bones,out,6,parents,ids,target,100),"ambiguous wrist ID rejected");
    std::puts("PASS: arm reach, bone lengths, descendant propagation, immutable source, opaque data and invalid input guards");
}
