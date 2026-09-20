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
    for(float angle:{0.f,1.5707963268f,3.1415926536f,-1.5707963268f}){
        // Independent native formula for a Z-axis quaternion: clockwise rotation.
        Pose stored{{0,0,std::sin(angle*.5f),std::cos(angle*.5f)},{200,300,40}};
        Vec3 worldTarget{220,310,210};
        auto local=compose(inverse(amalur::nativePose(stored)),Pose{{},worldTarget});
        check(amalur::stabilizeBody(native,out,7,parents,ids,local.position),"body anchor at cardinal heading");
        auto p=out[4].position;
        Vec3 actual{200+std::cos(angle)*p.x+std::sin(angle)*p.y,300-std::sin(angle)*p.x+std::cos(angle)*p.y,40+p.z};
        check(std::abs(actual.x-worldTarget.x)<.001f&&std::abs(actual.y-worldTarget.y)<.001f,"native rendered head stays under world anchor after turns");
    }
    amalur::BodyReference reference;amalur::RigBone baseline[7],gait[7],unchanged[7];
    Vec3 fixedAnchor{15,0,170};
    check(amalur::stabilizeTrackedBody(native,baseline,7,parents,ids,fixedAnchor,reference),"first-person body reference captured");
    for(int frame=0;frame<100;++frame){
        memcpy(gait,native,sizeof(gait));
        const float sway=std::sin(frame*.2f);
        // Reproduce a jogging animation translating and twisting the torso,
        // with the legs advancing independently.
        for(unsigned i:{2u,3u,4u,6u}){
            gait[i].position=gait[i].position+Vec3{8*sway,4*sway,5*sway};
            gait[i].orientation={0,0,std::sin(sway*.15f),std::cos(sway*.15f)};
        }
        gait[5].position.x+=12*sway;memcpy(unchanged,gait,sizeof(gait));
        Vec3 lean{3*sway,2*sway,4*sway};
        check(amalur::stabilizeTrackedBody(gait,out,7,parents,ids,fixedAnchor+lean,reference),"jog pose with tracked head movement accepted");
        for(unsigned i:{2u,3u,4u,6u}){
            check(length(out[i].position-baseline[i].position-lean)<.001f,"torso and shoulders follow physical anchor without native gait shake");
            check(!memcmp(&out[i].orientation,&baseline[i].orientation,sizeof(Quat)),"native torso twist cannot shake tracked upper body");
        }
        check(length(out[5].position-baseline[5].position-lean-Vec3{12*sway,0,0})<.001f,"native leg stride remains animated");
        check(!memcmp(gait,unchanged,sizeof(gait)),"body stabilization does not mutate native source");
        for(unsigned i=0;i<7;++i)check(out[i].positionW==3&&!memcmp(out[i].opaque,gait[i].opaque,16),"tracked body preserves opaque bone data");
    }
    auto stale=reference;stale.ids[2]=123;
    check(!amalur::stabilizeTrackedBody(native,out,7,parents,ids,fixedAnchor,stale),"changed skeleton rejects cached body pose");
    check(!amalur::stabilizeTrackedBody(native,out,7,parents,ids,{0,0,NAN},reference),"nonfinite height rejected");
    puts("PASS: first-person torso/shoulder gait isolation, independent leg stride, physical lean, immutable native pose and skeleton guards");
    check(!amalur::stabilizeBody(native,out,7,parents,ids,{NAN,0,0}),"invalid anchor rejected");
    parents[3]=3;check(!amalur::stabilizeBody(native,out,7,parents,ids,{}),"invalid hierarchy rejected");
    puts("PASS: whole-body anchoring, intact gait, segment lengths, immutable source and invalid-state guards");
}
