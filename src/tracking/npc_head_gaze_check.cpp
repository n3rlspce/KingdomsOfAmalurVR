#include "npc_head_gaze.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
static void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    using namespace amalur;
    RigBone native[4]{},out[4]{},again[4]{};
    const int16_t parents[]{-1,0,1,0};
    const uint32_t ids[]{1,0x005a2e4c,3,4};
    native[0].position={0,0,0};native[1].position={0,0,160};
    native[2].position={10,0,160};native[3].position={0,0,100};
    for(auto& bone:native)bone.orientation.w=1;
    const Pose root{};
    check(npcHeadGaze(native,out,4,parents,ids,root,{0,200,160}),"valid target applies head gaze");
    const auto facing=mgs5vr::rotate(bonePose(out[1]).orientation,{1,0,0});
    check(facing.y>.96f&&facing.x>0,"head aims toward viewer within turn limit");
    check(out[1].position.x==native[1].position.x&&out[1].position.y==native[1].position.y,
        "head pivots without moving its root");
    check(out[2].position.y>9.f,"face descendant follows head turn");
    check(!memcmp(&out[0],&native[0],sizeof(RigBone))&&!memcmp(&out[3],&native[3],sizeof(RigBone)),
        "body and torso remain native");
    check((out[1].opaque[12]&0x1c)==0x1c,"head rotation is activated for native consumer");
    check(npcHeadGaze(native,again,4,parents,ids,root,{0,200,160})&&
        !memcmp(out,again,sizeof(out)),"fresh animation input gives deterministic gaze without accumulation");
    auto badIds=std::array<uint32_t,4>{1,2,3,4};
    check(!npcHeadGaze(native,out,4,parents,badIds.data(),root,{0,200,160}),"unverified skeleton stays native");
    RigBone large[89]{},largeOut[89]{};
    int16_t largeParents[89]{};uint32_t largeIds[89]{};
    for(unsigned i=0;i<89;++i){large[i].orientation.w=1;largeParents[i]=i?static_cast<int16_t>(i-1):-1;}
    large[24].position={0,0,160};largeIds[24]=0x005a2e4c;
    check(npcHeadGaze(large,largeOut,89,largeParents,largeIds,root,{0,200,160}),"89-bone face mesh follows viewer");
    check((largeOut[24].opaque[12]&0x1c)==0x1c&&largeOut[88].orientation.w<1.f,
        "large face mesh publishes head and descendant rotation");
    std::puts("PASS: NPC head-only gaze, descendant transform, bounded turn, native body and invalid skeleton");
}
