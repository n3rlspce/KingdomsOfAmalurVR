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
    check(npcHeadGaze(native,out,4,parents,ids,root,{200,-200,160}),"valid target applies head gaze");
    const auto facing=mgs5vr::rotate(bonePose(out[1]).orientation,{0,-1,0});
    check(facing.x>.70f&&facing.y<-.70f,"face axis aims toward viewer within turn limit");
    check(out[1].position.x==native[1].position.x&&out[1].position.y==native[1].position.y,
        "head pivots without moving its root");
    check(out[2].position.y>6.f,"face descendant follows head turn");
    check(!memcmp(&out[0],&native[0],sizeof(RigBone))&&!memcmp(&out[3],&native[3],sizeof(RigBone)),
        "body and torso remain native");
    check((out[1].opaque[12]&0x1c)==0x1c,"head rotation is activated for native consumer");
    check(npcHeadGaze(native,again,4,parents,ids,root,{200,-200,160})&&
        !memcmp(out,again,sizeof(out)),"fresh animation input gives deterministic gaze without accumulation");
    auto badIds=std::array<uint32_t,4>{1,2,3,4};
    check(!npcHeadGaze(native,out,4,parents,badIds.data(),root,{0,200,160}),"unverified skeleton stays native");
    RigBone large[89]{},largeOut[89]{};
    int16_t largeParents[89]{};uint32_t largeIds[89]{};
    for(unsigned i=0;i<89;++i){large[i].orientation.w=1;largeParents[i]=i?static_cast<int16_t>(i-1):-1;}
    large[24].position={0,0,160};largeIds[24]=0x005a2e4c;
    check(npcHeadGaze(large,largeOut,89,largeParents,largeIds,root,{200,-200,160}),"89-bone face mesh follows viewer");
    check((largeOut[24].opaque[12]&0x1c)==0x1c&&largeOut[88].orientation.w<1.f,
        "large face mesh publishes head and descendant rotation");
    // Read-only live NPC capture: local +X points almost straight up, while
    // local -Y already points near the player. The old +X solve looked down.
    RigBone live[2]{},liveOut[2]{};const int16_t liveParents[]{-1,0};
    const uint32_t liveIds[]{1,0x005a2e4c};
    for(auto& bone:live)bone.orientation.w=1;
    live[1].position={-3.322099f,-.103051f,173.082779f};
    live[1].orientation={-.492085f,.480347f,-.507581f,.519115f};
    Pose liveRoot{nativeQuaternion({0,0,-.989049f,.147588f}),{4423.7207f,-64040.6797f,5189.5239f}};
    const Vec3 liveEye{4293.2461f,-64012.2969f,5366.5220f};
    check(npcHeadGaze(live,liveOut,2,liveParents,liveIds,liveRoot,liveEye),"live bind pose can correct eye contact");
    const auto worldFace=mgs5vr::compose(liveRoot,bonePose(liveOut[1]));
    auto eyeDirection=liveEye-worldFace.position;check(normalize(eyeDirection),"live eye direction valid");
    const auto correctedFace=mgs5vr::rotate(worldFace.orientation,{0,-1,0});
    check(mgs5vr::dot(correctedFace,eyeDirection)>.99f&&std::abs(correctedFace.z)<.1f,
        "live NPC face looks level toward the player, not into the floor");
    std::puts("PASS: NPC head-only gaze, descendant transform, bounded turn, native body and invalid skeleton");
}
