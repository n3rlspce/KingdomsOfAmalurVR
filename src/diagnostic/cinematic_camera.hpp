#pragma once
#include "../tracking/cinematic_view.hpp"

namespace cinematic_camera {
struct State {uintptr_t scene{},core{},owner{};};
// Build 10619381: manager constructor RVA AF0EBF constructs CinematicMgr at
// +1080 and CinematicSceneMgr at +15A0. The latter's +D4 scene is destroyed by
// RVA 9E4876 and cleared by 9D8896. Require its RTTI and the active SceneWin
// camera, not merely paused time, a narrow FOV, or an arbitrary Camera object.
static bool sample(State& out){
    __try {
        const auto word=player_rig::word;
        const auto globals=word(gameBase+0x15fe9c4);if(!globals)return false;
        const auto scenes=globals+0x15a0;
        if(word(scenes)!=gameBase+0x13483cc)return false;
        const auto scene=word(scenes+0xd4);
        if(!scene||word(scene)!=gameBase+0x1348754)return false;
        const auto windows=globals+0x397c;
        if(word(windows)!=gameBase+0x134e60c||word(windows+8)!=1)return false;
        const auto entries=word(windows+4);if(!entries)return false;
        const auto game=word(entries+4);if(!game)return false;
        if(word(game)!=gameBase+0x1328914&&word(game)!=gameBase+0x132ae7c)return false;
        const auto sceneWindow=word(game+0x40c);
        if(!sceneWindow||word(sceneWindow)!=gameBase+0x1326e9c)return false;
        const auto camera=word(sceneWindow+0x410);
        if(!camera||word(camera)!=gameBase+0x1335d08||word(camera+8)!=1)return false;
        const auto player=reinterpret_cast<uintptr_t>(player_rig::player.load());
        if(!player||(word(player)!=gameBase+0x1359f14&&word(player)!=gameBase+0x1359e94))return false;
        // A preloaded scene cannot take over gameplay or a player-owned view.
        if(camera==word(player+0x108))return false;
        if(word(scenes+0xd4)!=scene||word(sceneWindow+0x410)!=camera)return false;
        out={scene,camera+8,sceneWindow};return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
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
    const bool tracked=packet.version==3&&packet.valid&&packet.gameMode==1&&packet.tick<=now&&now-packet.tick<250&&
        std::isfinite(packet.horizontalFov)&&packet.horizontalFov>50&&packet.horizontalFov<179&&
        view.apply(s.scene,s.core,recenterGeneration.load(),packet.recenter,native,head,packet.worldScale,adjusted);
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
