#pragma once
#include "../tracking/cinematic_view.hpp"
#include "../tracking/cinematic_owner.hpp"
#include "../tracking/cinematic_mode.hpp"

namespace cinematic_camera {
using State=amalur::CinematicOwner;
static bool sample(State& out){
    __try {return amalur::cinematicOwner(gameBase,player_rig::word,out);}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static amalur::CinematicView view;
static amalur::CameraInputs inputs;
static State retained{};
static bool wasActive{};
static uint64_t lastOpen{};
static void restore(void* camera){
    auto core=static_cast<unsigned char*>(camera);
    State s;const bool scene=sample(s);
    if(inputs.core==core){
        if(scene&&s.scene==retained.scene&&s.owner==retained.owner&&s.core==retained.core){
            __try {inputs.restore();} __except(EXCEPTION_EXECUTE_HANDLER){inputs.core=nullptr;}
        }else inputs.core=nullptr;
    }
}
static void leave(){
    if(wasActive){view.reset();inputs.core=nullptr;dialogue_camera::resumeGameplay=true;log("Immersive cinematic inactive\n");}
    wasActive=false;
}
static bool rebuild(void* camera){
    auto core=static_cast<unsigned char*>(camera);
    State s;const bool scene=sample(s);
    const bool active=scene&&headTracking.load()&&!interfaceView.load();
    if(!active){
        leave();return false;
    }
    if(!wasActive)log("Immersive cinematic scene=%p core=%p (native position, level head-look)\n",
        reinterpret_cast<void*>(s.scene),reinterpret_cast<void*>(s.core));
    wasActive=true;
    // Reuse the existing interaction suppression and cinematic subtitle layout.
    // Dialogue sampling resets this flag before each camera rebuild.
    motion_controls::dialogueActive.store(true);
    motion_controls::sampleMovementBasis({}, {},false,GetTickCount64());
    if(reinterpret_cast<uintptr_t>(camera)!=s.core){realRebuildCamera(camera);return true;}
    const auto now=GetTickCount64();
    if(now-lastOpen>1000){poseChannel.open(false);lastOpen=now;}
    amalur::PosePacket packet{};poseChannel.read(packet);
    amalur::CameraPose native{},adjusted{};
    memcpy(&native.eye,core+4,12);memcpy(&native.target,core+0x14,12);memcpy(&native.up,core+0x1c0,12);
    const float fov=*reinterpret_cast<float*>(core+0x2c);
    const mgs5vr::Pose head{{packet.orientation[0],packet.orientation[1],packet.orientation[2],packet.orientation[3]},
        {packet.position[0],packet.position[1],packet.position[2]}};
    const bool fullVR=amalur::cinematicMode.fullVR();
    if(!fullVR)view.reset();
    const bool tracked=fullVR&&packet.version==3&&packet.valid&&packet.gameMode==1&&packet.tick<=now&&now-packet.tick<250&&
        std::isfinite(packet.horizontalFov)&&packet.horizontalFov>50&&packet.horizontalFov<179&&
        view.apply(s.scene,s.core,recenterGeneration.load(),packet.recenter,native,head,packet.worldScale,adjusted,now);
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
