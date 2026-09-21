#include "body_pose.hpp"
#include "visual_root.hpp"
#include <cstdio>
#include <cstdlib>
#include <array>
using namespace mgs5vr;
void check(bool pass,const char* message){if(!pass){std::printf("FAIL: %s\n",message);std::exit(1);}}
float length(Vec3 v){return std::sqrt(dot(v,v));}
int main(){
    {
        amalur::RigBone source[2]{},modified[2];
        for(auto& b:source){b.position={1,2,3};b.positionW=7;b.orientation={0,0,0,1};memset(b.opaque,0xa5,16);b.opaque[12]=0x40;}
        for(unsigned mode=0;mode<4;++mode){
            memcpy(modified,source,sizeof(source));
            for(auto& b:modified){b.position={9,8,7};b.positionW=1;b.orientation={0,0,.70710678f,.70710678f};b.opaque[12]=0x5e;}
            amalur::restoreNativeRigChannels(source,modified,2,(mode&1)!=0,(mode&2)!=0);
            for(unsigned i=0;i<2;++i){
                check(modified[i].position.x==((mode&1)?1:9),"position ablation selects native position independently");
                check(modified[i].positionW==((mode&1)?7:1),"position channel includes W");
                check(modified[i].orientation.z==((mode&2)?0:.70710678f),"rotation ablation selects native rotation independently");
                const unsigned flags=0x40|((mode&1)?0:2)|((mode&2)?0:0x1c);
                check(modified[i].opaque[12]==flags,"native activation bits follow restored channel");
                for(unsigned j=0;j<16;++j)if(j!=12)check(modified[i].opaque[j]==source[i].opaque[j],"scale and unrelated metadata preserved");
            }
            if(mode==3)check(!memcmp(source,modified,sizeof(source)),"both channels restore byte-identical native input");
        }
        puts("PASS: independent position/rotation publication and exact native restoration");
    }
    {
        amalur::RigBone original[3]{},published[3];
        for(auto& bone:original){bone.positionW=7;memset(bone.opaque,0xa0,16);bone.opaque[12]=0x40;}
        memcpy(published,original,sizeof(original));
        published[0].position={2,3,4};published[1].orientation={0,0,.70710678f,.70710678f};
        auto effectivePosition=[](const amalur::RigBone& b){return (b.opaque[12]&2)?b.position:Vec3{};};
        auto effectiveRotation=[](const amalur::RigBone& b){return (b.opaque[12]&0x1c)?b.orientation:Quat{};};
        check(length(effectivePosition(published[0]))==0,"native getter ignores unflagged changed translation");
        check(effectiveRotation(published[1]).w==1,"native getter ignores unflagged changed rotation");
        amalur::publishRigOverrides(original,published,3);
        check(length(effectivePosition(published[0])-Vec3{2,3,4})<.001f,"published translation visible to native getter");
        check(effectiveRotation(published[1]).z>.7f,"published quaternion visible to native getter");
        check(published[0].positionW==1&&published[1].positionW==7,"native position setter semantics apply only to changed positions");
        check(published[0].opaque[12]==0x42&&published[1].opaque[12]==0x5c,"preserve scale flag and activate only changed channels");
        check(!memcmp(&published[2],&original[2],sizeof(amalur::RigBone)),"unchanged native bones remain byte identical");
        for(unsigned i=0;i<2;++i)for(unsigned byte=0;byte<16;++byte)if(byte!=12)check(published[i].opaque[byte]==original[i].opaque[byte],"scale and unrelated metadata unchanged");
        puts("PASS: native transform flags publish modified position/rotation without changing scale or untouched bones");
    }

    {
        amalur::RigBone animated[8]{},stable[8]{};
        int16_t parents[]={-1,0,1,2,3,1,5,6};
        uint32_t ids[]={1,0x688528,0xf21468,0xd1f75e,0x88d0eb,0xf1076a,0xd0ea60,0x87c3ed};
        for(unsigned frame=0;frame<20;++frame){
            for(unsigned i=0;i<8;++i){animated[i].position={float(i),float(frame+i),10.f};animated[i].orientation={0,0,0,1};}
            memcpy(stable,animated,sizeof(stable));
            stable[1].position={30,20,10};stable[1].orientation=amalur::nativeQuaternion({0,0,.70710678f,.70710678f});
            auto correction=compose(amalur::bonePose(stable[1]),inverse(amalur::bonePose(animated[1])));
            check(amalur::restoreNativeArmAnimation(animated,stable,8,parents,ids),"native arm animation on stabilized torso accepted");
            for(unsigned i:{2u,3u,4u,5u,6u,7u}){
                auto expected=compose(correction,amalur::bonePose(animated[i]));
                check(length(stable[i].position-expected.position)<.001f,"both native arm chains animate relative to frozen chest");
            }
            check(stable[1].position.x==30,"native arms cannot unfreeze chest");
        }
        puts("PASS: native arm animation independent from frozen torso for both wrists");
    }

    {
        amalur::VisualRootRotation smooth;
        Pose root{{},{100,200,0}};
        smooth.sample(root,1,0,0,1.0);
        root.orientation={0,0,.5f,.8660254f}; // A measured-style 60 degree native step.
        auto visual=smooth.sample(root,1,0,1,1.01);
        check(std::abs(rotate(visual.orientation,{1,0,0}).y)<.053f,"60 degree native step limited to 3 visual degrees in 10ms");
        auto repeat=smooth.sample(root,1,0,1,1.015);
        check(!memcmp(&visual.orientation,&repeat.orientation,sizeof(Quat)),"armor and weapon callbacks share visual rotation");
        const Pose target{{},{130,240,150}};
        amalur::RigBone wrist{};auto local=compose(inverse(visual),target);
        wrist.position=local.position;wrist.orientation=amalur::nativeQuaternion(local.orientation);
        amalur::rebaseVisualRig(&wrist,1,visual,root);
        auto rendered=compose(root,amalur::bonePose(wrist));
        check(length(rendered.position-target.position)<.001f,"root compensation preserves exact world wrist target");
        check(length(rotate(rendered.orientation,{1,0,0})-Vec3{1,0,0})<.001f,"root compensation preserves wrist orientation");
        for(unsigned f=2;f<=101;++f)visual=smooth.sample(root,1,0,f,1.0+f*.01);
        check(length(rotate(visual.orientation,{1,0,0})-rotate(root.orientation,{1,0,0}))<.001f,"body still follows native facing after turn");
        root.orientation={};visual=smooth.sample(root,1,1,102,2.02);
        check(visual.orientation.w==1,"recenter resets visual root without trailing old heading");
        puts("PASS: abrupt body rotation bounded, same-frame consistency, exact wrist world pose and continued body follow");
    }

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
    check(amalur::stabilizeTrackedBody(native,out,7,parents,ids,fixedAnchor,stale),"changed valid layout recaptures body pose");
    check(stale.revision==reference.revision+1&&stale.ids[2]==ids[2],"layout recovery commits one revision");
    {
        amalur::BodyReferenceKey key{1,2,3,4,5};
        auto recovered=reference;
        check(amalur::stabilizeTrackedBody(native,out,7,parents,ids,fixedAnchor,recovered,key),"asset lifetime accepted");
        auto old=recovered;amalur::RigBone saved[7];memcpy(saved,out,sizeof(saved));
        auto changedKey=key;changedKey.asset=9;
        auto broken=native[3];native[3].orientation.w=NAN;
        check(!amalur::stabilizeTrackedBody(native,out,7,parents,ids,fixedAnchor,recovered,changedKey),"invalid new asset rejected");
        check(!memcmp(&old,&recovered,sizeof(old))&&!memcmp(saved,out,sizeof(saved)),"invalid transition keeps reference and output intact");
        native[3]=broken;
        for(unsigned field=0;field<5;++field){
            auto nextKey=key;
            if(field==0)++nextKey.root;if(field==1)++nextKey.owner;if(field==2)++nextKey.asset;
            if(field==3)++nextKey.blob;if(field==4)++nextKey.center;
            const auto revision=recovered.revision;
            check(amalur::stabilizeTrackedBody(native,out,7,parents,ids,fixedAnchor,recovered,nextKey),"validated lifetime transition recovers");
            check(recovered.revision==revision+1,"lifetime changes produce new reference revision");
            check(amalur::stabilizeTrackedBody(native,out,7,parents,ids,fixedAnchor,recovered,nextKey)&&recovered.revision==revision+1,"same layout does not repeatedly calibrate");
        }
        auto changedIds=std::array<uint32_t,7>{};memcpy(changedIds.data(),ids,sizeof(ids));changedIds[6]=77;
        const auto revision=recovered.revision;
        check(amalur::stabilizeTrackedBody(native,out,7,parents,changedIds.data(),fixedAnchor,recovered,key),"valid ID layout change recovers");
        check(recovered.ids[6]==77&&recovered.revision==revision+1,"new layout retained");
        int16_t changedParents[7];memcpy(changedParents,parents,sizeof(parents));changedParents[6]=3;
        check(amalur::stabilizeTrackedBody(native,out,7,changedParents,changedIds.data(),fixedAnchor,recovered,key),"valid parent layout change recovers");
        check(amalur::stabilizeTrackedBody(native,out,6,parents,ids,fixedAnchor,recovered,key)&&recovered.count==6,"valid count change recovers");
        old=recovered;changedParents[3]=3;
        check(!amalur::stabilizeTrackedBody(native,out,7,changedParents,changedIds.data(),fixedAnchor,recovered,key)&&!memcmp(&old,&recovered,sizeof(old)),"invalid hierarchy preserves last reference");
        puts("PASS: asset/layout/owner/recenter recovery and transactional rejection of invalid transitions");
    }
    check(!amalur::stabilizeTrackedBody(native,out,7,parents,ids,{0,0,NAN},reference),"nonfinite height rejected");
    puts("PASS: first-person torso/shoulder gait isolation, independent leg stride, physical lean, immutable native pose and skeleton guards");
    check(!amalur::stabilizeBody(native,out,7,parents,ids,{NAN,0,0}),"invalid anchor rejected");
    parents[3]=3;check(!amalur::stabilizeBody(native,out,7,parents,ids,{}),"invalid hierarchy rejected");
    puts("PASS: whole-body anchoring, intact gait, segment lengths, immutable source and invalid-state guards");
}
