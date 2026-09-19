#include "body_pose.hpp"
#include <cstdio>
#include <cstdlib>
using namespace mgs5vr;
void check(bool pass,const char* message){if(!pass){std::printf("FAIL: %s\n",message);std::exit(1);}}
float length(Vec3 v){return std::sqrt(dot(v,v));}
int main(){
    amalur::RigBone native[7]{},out[7];int16_t parents[]{-1,0,1,2,3,1,2};
    uint32_t ids[]{1,2,0x688528,3,0x5a2e4c,4,5};
    native[1].position={0,0,100};native[2].position={2,0,112};native[3].position={12,0,140};
    native[4].position={30,4,166};native[5].position={0,10,5};native[6].position={8,20,144};
    for(auto& b:native){b.positionW=3;memset(b.opaque,0xab,16);}
    amalur::RigBone copy[7];memcpy(copy,native,sizeof(copy));
    for(int frame=0;frame<100;++frame){
        native[4].position.x=30*std::sin(frame*.07f);
        check(amalur::stabilizeBody(native,out,7,parents,ids,{15,0,170}),"valid whole-body pose");
        check(std::abs(out[4].position.x-15)<.001f&&std::abs(out[4].position.y)<.001f,"head horizontal position anchored across gait");
        for(unsigned i=0;i<7;++i){
            auto delta=out[i].position-native[i].position;
            auto expected=out[4].position-native[4].position;
            check(length(delta-expected)<.001f,"same offset for head torso pelvis and feet");
            check(out[i].position.z==native[i].position.z&&!memcmp(&out[i].orientation,&native[i].orientation,sizeof(Quat)),"vertical gait and rotations untouched");
        }
        for(unsigned i:{3u,4u,6u})check(std::abs(length(out[i].position-out[parents[i]].position)-length(native[i].position-native[parents[i]].position))<.001f,"upper-body segment lengths preserved");
        for(unsigned i=0;i<7;++i)check(out[i].positionW==3&&!memcmp(out[i].opaque,native[i].opaque,16),"opaque bytes preserved");
    }
    native[4]=copy[4];check(!memcmp(copy,native,sizeof(copy)),"source remains native");
    check(!amalur::stabilizeBody(native,out,7,parents,ids,{NAN,0,0}),"invalid anchor rejected");
    parents[3]=3;check(!amalur::stabilizeBody(native,out,7,parents,ids,{}),"invalid hierarchy rejected");
    puts("PASS: whole-body anchoring, intact gait, segment lengths, immutable source and invalid-state guards");
}
