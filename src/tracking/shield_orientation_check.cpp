#include "shield_orientation.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace amalur;
void check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
bool near(mgs5vr::Vec3 a,mgs5vr::Vec3 b){return std::abs(a.x-b.x)<.0001f&&std::abs(a.y-b.y)<.0001f&&std::abs(a.z-b.z)<.0001f;}
int main(){
    const uint32_t ids[]{6711025,17330092};const int16_t parents[]{-1,0};
    RigBone before[2]{};
    mgs5vr::Pose pose;check(gripAngleTrim(37,-29,61,pose),"rotated fixture");
    for(unsigned i=0;i<2;++i){before[i].position={11.f+i,23.f+i,41.f+i};before[i].positionW=4;
        before[i].orientation=nativeQuaternion(pose.orientation);memset(before[i].opaque,0xa3,16);}
    before[1].opaque[12]=0xa4;
    RigBone after[2];memcpy(after,before,sizeof(after));
    check(uprightShield(after,2,ids,parents),"accepted skeleton");
    check(!memcmp(after,before,sizeof(RigBone)),"root byte unchanged");
    const auto old=bonePose(before[1]),now=bonePose(after[1]);
    check(near(mgs5vr::rotate(old.orientation,{1,0,0}),mgs5vr::rotate(now.orientation,{1,0,0})),"local X preserved");
    check(near(mgs5vr::rotate(old.orientation,{0,1,0})*-1,mgs5vr::rotate(now.orientation,{0,1,0})),"local Y reversed");
    check(near(mgs5vr::rotate(old.orientation,{0,0,1})*-1,mgs5vr::rotate(now.orientation,{0,0,1})),"local Z reversed");
    check(!memcmp(&before[1].position,&after[1].position,sizeof(before[1].position))&&before[1].positionW==after[1].positionW,"anchor and W unchanged");
    check((after[1].opaque[12]&0x1c)==0x1c,"orientation flags activated");
    for(unsigned i=0;i<16;++i)check(i==12?((before[1].opaque[i]&~0x1c)==(after[1].opaque[i]&~0x1c)):(before[1].opaque[i]==after[1].opaque[i]),"scale and other opaque bits unchanged");
    RigBone current[2];memcpy(current,after,sizeof(current));
    check(restoreShieldOrientation(current[1],before[1],after[1])&&!memcmp(current,before,sizeof(current)),"exact restoration");
    check(uprightShield(current,2,ids,parents)&&!memcmp(current,after,sizeof(current)),"restore apply no accumulation");
    current[1].opaque[12]^=0x80;current[1].opaque[0]=9;current[1].position.x=101;
    const auto unrelated=current[1].opaque[12]&~0x1c;
    check(restoreShieldOrientation(current[1],before[1],after[1]),"unrelated native changes permit restore");
    check((current[1].opaque[12]&~0x1c)==unrelated&&current[1].opaque[0]==9&&current[1].position.x==101,"unrelated native changes preserved");
    memcpy(current,after,sizeof(current));current[1].orientation={0,0,0,1};RigBone saved=current[1];
    check(!restoreShieldOrientation(current[1],before[1],after[1])&&!memcmp(&current[1],&saved,sizeof(saved)),"native quaternion wins");
    memcpy(current,after,sizeof(current));current[1].opaque[12]^=0x04;saved=current[1];
    check(!restoreShieldOrientation(current[1],before[1],after[1])&&!memcmp(&current[1],&saved,sizeof(saved)),"native orientation flags win");
    memcpy(current,before,sizeof(current));current[1].position.x=std::numeric_limits<float>::quiet_NaN();RigBone invalid[2];memcpy(invalid,current,sizeof(invalid));
    check(!uprightShield(current,2,ids,parents)&&!memcmp(current,invalid,sizeof(current)),"invalid input no partial writes");
    memcpy(current,before,sizeof(current));uint32_t wrongIds[]{6711025,17330093};int16_t wrongParents[]{-1,-1};
    check(!uprightShield(current,2,wrongIds,parents)&&!uprightShield(current,2,ids,wrongParents)&&!uprightShield(current,1,ids,parents)&&!uprightShield(current,2,nullptr,parents)&&!memcmp(current,before,sizeof(current)),"skeleton guard");
    puts("PASS: shield local-X flip, fixed anchor/root, activation flags, selective restore and invalid-input atomicity");
}
