#pragma once
#include "../tracking/cinematic_view.hpp"
#include "../tracking/cinematic_owner.hpp"
#include "../tracking/cinematic_mode.hpp"
#include "../tracking/finisher_view.hpp"
#include "native_finisher_state.hpp"

namespace cinematic_camera {
using State=amalur::CinematicOwner;
static bool sample(State& out){
    __try {return amalur::cinematicOwner(gameBase,player_rig::word,out);}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Mode84/Fate_Shift can own a finisher camera without the generic cinematic
// manager flag. Reuse its verified active scene graph, only for that sequence.
static bool finisherScene(State& out){
    __try {
        const auto word=player_rig::word;const auto g=word(gameBase+0x15fe9c4);if(!g)return false;
        const auto windows=g+0x397c;
        if(word(windows)!=gameBase+0x134e60c||word(windows+8)!=1)return false;
        const auto entries=word(windows+4),game=entries?word(entries+4):0;
        if(!game||(word(game)!=gameBase+0x1328914&&word(game)!=gameBase+0x132ae7c))return false;
        const auto scene=word(game+0x40c);if(!scene||word(scene)!=gameBase+0x1326e9c)return false;
        const auto camera=word(scene+0x410);
        if(!camera||word(camera)!=gameBase+0x1335d08||word(camera+8)!=1)return false;
        out={scene,camera+8,scene};return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool finisherEye(uint32_t owner,float scale,mgs5vr::Vec3& eye,mgs5vr::Vec3& forward,unsigned& anchorKind){
    bool fallback=false;anchorKind=0;
    __try {
        const auto entity=player_rig::resolve(owner),loc=player_rig::part(entity,6,owner,0x1355cdc);
        if(!loc||!(player_rig::word(loc+0x20)&1))return false;
        memcpy(&eye,reinterpret_cast<void*>(loc+0x24),12);
        const double yaw=uint32_t(player_rig::word(loc+0xb0))*(6.283185307179586/4294967296.0);
        forward={float(std::cos(yaw)),float(std::sin(yaw)),0};
        // Same first-person body-eye height as gameplay; remains first person
        // if the native head model is unavailable during an animation change.
        eye.z+=195.f+amalur::headHeight.get()*.01f*scale;
        if(!std::isfinite(eye.x)||!std::isfinite(eye.y)||!std::isfinite(eye.z))return false;
        fallback=true;anchorKind=1;
        const auto rendering=player_rig::part(entity,7,owner,0x13560e4);
        const auto root=rendering?weapon_control::fab(player_rig::word(rendering+0x9c)):0;
        if(!root||player_rig::word(root+0xf8)!=owner)return true;
        const uint32_t* ids{};const int16_t* parents{};unsigned count{};
        if(!body_visibility::skeleton(root,ids,parents,count))return true;
        const auto bones=player_rig::word(root+0x34);if(!bones)return true;
        unsigned head=count;
        for(unsigned i=0;i<count;++i)if(ids[i]==0x005a2e4c){if(head!=count)return true;head=i;}
        if(head==count)return true;
        amalur::RigBone world{},joint{};
        memcpy(&world,reinterpret_cast<void*>(root+0x124),48);
        memcpy(&joint,reinterpret_cast<void*>(bones+head*48),48);
        // The tail is opaque, not guaranteed initialized scale data. V26
        // rejected valid live model538 heads because those bytes contained NaN.
        mgs5vr::Vec3 body{},position{};memcpy(&body,reinterpret_cast<void*>(loc+0x24),12);
        if(!amalur::finisherHeadPosition(world,joint,body,position))return true;
        if(player_rig::word(root+0x34)!=bones||player_rig::word(root+0xf8)!=owner)return true;
        anchorKind=2;eye=position;eye.z+=amalur::headHeight.get()*.01f*scale;return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return fallback;}
}
static amalur::CinematicView view;
static amalur::FinisherView finisherView;
static amalur::CameraInputs inputs;
static State retained{};
static bool wasActive{};
static uint64_t lastOpen{};
static void traceFinisher(const native_finisher_state::Snapshot& f,const State& s,void* camera,
    const char* reason,const amalur::PosePacket* p=nullptr,unsigned anchor=0,
    mgs5vr::Vec3 eye={},mgs5vr::Vec3 forward={}){
    static bool observed=false;static uint64_t next[16]{};
    const auto now=GetTickCount64();
    if(!f.nativeSequence){if(observed){log("VR finisher camera tick=%llu reason=sequence-ended\n",now);observed=false;}return;}
    observed=true;unsigned code=anchor;
    for(const char* c=reason;*c;++c)code=code*33u+unsigned(*c);
    const auto slot=code%16;if(now<next[slot])return;next[slot]=now+500;
    log("VR finisher camera tick=%llu reason=%s owner=%08x first=%u head=%u interface=%u core=%08x selected=%08x scene=%08x anchor=%u poseValid=%u mode=%u age=%llu scale=%.3f eye=%.3f,%.3f,%.3f forward=%.3f,%.3f,%.3f\n",
        now,reason,f.owner,unsigned(firstPerson.load()),unsigned(headTracking.load()),unsigned(interfaceView.load()),
        unsigned(reinterpret_cast<uintptr_t>(camera)),unsigned(s.core),unsigned(s.scene),anchor,p?p->valid:0,p?p->gameMode:0,
        p&&p->tick<=now?now-p->tick:~uint64_t(0),p?p->worldScale:0.f,eye.x,eye.y,eye.z,forward.x,forward.y,forward.z);
}
static void restore(void* camera){
    auto core=static_cast<unsigned char*>(camera);
    State s;const auto finisher=native_finisher_state::read();
    const bool sequence=firstPerson.load()&&finisher.nativeSequence;
    const bool scene=sequence?finisherScene(s):sample(s);
    if(inputs.core==core){
        if(scene&&s.scene==retained.scene&&s.owner==retained.owner&&s.core==retained.core){
            __try {inputs.restore();} __except(EXCEPTION_EXECUTE_HANDLER){inputs.core=nullptr;}
        }else inputs.core=nullptr;
    }
}
static void leave(){
    if(wasActive){view.reset();inputs.core=nullptr;dialogue_camera::resumeGameplay=true;log("Immersive cinematic inactive\n");}
    wasActive=false;finisherView.reset();
}
static bool rebuild(void* camera){
    auto core=static_cast<unsigned char*>(camera);
    State s;const auto finisher=native_finisher_state::read();
    const bool sequence=firstPerson.load()&&finisher.nativeSequence;
    const bool scene=sequence?finisherScene(s):sample(s);
    const bool active=scene&&headTracking.load()&&!interfaceView.load();
    if(!active){
        traceFinisher(finisher,s,camera,scene?"tracking-or-interface-gate":"scene-unavailable");
        leave();return false;
    }
    if(!wasActive)log("Immersive cinematic scene=%p core=%p (native position, level head-look)\n",
        reinterpret_cast<void*>(s.scene),reinterpret_cast<void*>(s.core));
    wasActive=true;
    // Reuse the existing interaction suppression and cinematic subtitle layout.
    // Dialogue sampling resets this flag before each camera rebuild.
    motion_controls::dialogueActive.store(true);
    motion_controls::sampleMovementBasis({}, {},false,GetTickCount64());
    if(reinterpret_cast<uintptr_t>(camera)!=s.core){traceFinisher(finisher,s,camera,"non-selected-core");realRebuildCamera(camera);return true;}
    const auto now=GetTickCount64();
    if(now-lastOpen>1000){poseChannel.open(false);lastOpen=now;}
    amalur::PosePacket packet{};poseChannel.read(packet);
    amalur::CameraPose native{},adjusted{};
    memcpy(&native.eye,core+4,12);memcpy(&native.target,core+0x14,12);memcpy(&native.up,core+0x1c0,12);
    const float fov=*reinterpret_cast<float*>(core+0x2c);
    const mgs5vr::Pose head{{packet.orientation[0],packet.orientation[1],packet.orientation[2],packet.orientation[3]},
        {packet.position[0],packet.position[1],packet.position[2]}};
    const bool fullVR=sequence||amalur::cinematicMode.fullVR();
    if(!sequence)finisherView.reset();
    if(!fullVR)view.reset();
    const bool poseValid=fullVR&&packet.version==3&&packet.valid&&packet.gameMode==1&&packet.tick<=now&&now-packet.tick<250&&
        std::isfinite(packet.horizontalFov)&&packet.horizontalFov>50&&packet.horizontalFov<179;
    bool tracked=false;unsigned anchorKind=0;const char* resultReason="pose-rejected";
    if(poseValid&&sequence){
        resultReason="eye-unavailable";
        mgs5vr::Vec3 eye{},heading{};
        if(finisherEye(finisher.owner,packet.worldScale,eye,heading,anchorKind)){
            // Last gameplay view is already in the user's world-facing basis.
            // Require the matching recent locomotion owner before using it.
            AcquireSRWLockShared(&weapon_control::poseLock);
            const auto previous=weapon_control::locomotionFrame;
            ReleaseSRWLockShared(&weapon_control::poseLock);
            if(!finisherView.active&&previous.valid&&previous.owner==finisher.owner&&
               previous.tick<=now&&now-previous.tick<1000){
                auto prior=cameraInputs.applied.target-cameraInputs.applied.eye;prior.z=0;
                if(amalur::normalize(prior))heading=prior;
            }
            tracked=finisherView.apply(finisher.owner,recenterGeneration.load(),packet.recenter,
                eye,heading,head,packet.worldScale,adjusted);
            resultReason=tracked?"applied":"view-rejected";
        }
    }else if(poseValid)tracked=view.apply(s.scene,s.core,recenterGeneration.load(),packet.recenter,
        native,head,packet.worldScale,adjusted,now);
    traceFinisher(finisher,s,camera,sequence?resultReason:"ordinary-cinematic",&packet,anchorKind,
        tracked?adjusted.eye:native.eye,tracked?adjusted.target-adjusted.eye:native.target-native.eye);
    if(tracked){
        memcpy(core+4,&adjusted.eye,12);memcpy(core+0x14,&adjusted.target,12);memcpy(core+0x1c0,&adjusted.up,12);
        *reinterpret_cast<float*>(core+0x2c)=packet.horizontalFov;
    }
    core[0x35e]|=1;realRebuildCamera(camera);
    packet.valid=tracked?1:0;
    packet.projectionX=*reinterpret_cast<float*>(core+0xc4);packet.projectionY=*reinterpret_cast<float*>(core+0xd8);
    packet.stereoStatus=stereoStatus.load();
    if(frameChannel.open(true))frameChannel.publish(packet);
    render_pose::camera(core,packet);trackedCameraAvailable.store(tracked);
    camera_status::publish(packet,tracked,false,{},native,tracked?adjusted:native,core);
    if(tracked&&coherentCamera.load()){
        inputs={core,native,adjusted,fov,packet.horizontalFov};retained=s;
    }else{
        memcpy(core+4,&native.eye,12);memcpy(core+0x14,&native.target,12);memcpy(core+0x1c0,&native.up,12);
        *reinterpret_cast<float*>(core+0x2c)=fov;
    }
    return true;
}
}
