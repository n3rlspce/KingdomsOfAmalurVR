#pragma once
// Optional, nonblocking diagnostics. No timing, input or game-state changes.
namespace camera_status {
struct Snapshot {
    uint32_t version{2},pid{},frame{},rebuild{};
    uint64_t sampled{},poseTick{};
    uint32_t tracked{},playerValid{};
    float headPosition[3]{},headOrientation[4]{};
    float playerPosition[3]{},nativeEye[3]{},renderEye[3]{},renderForward[3]{};
    float nativeTarget[3]{},cachedRight[3]{},cachedUp[3]{},cachedForward[3]{};
    float projection[4]{};
};
static_assert(sizeof(Snapshot)==184,"Camera telemetry V2 layout must match capture-camera.py");
inline void publish(const amalur::PosePacket& pose,bool tracked,bool playerValid,
    mgs5vr::Vec3 player,const amalur::CameraPose& native,const amalur::CameraPose& rendered,const unsigned char* core){
    static HANDLE mapping{},mutex{};static Snapshot* memory{};static uint32_t serial{};
    if(!memory){
        if(!mapping)mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Snapshot),L"Local\\AmalurCameraStatusV2");
        if(!mutex)mutex=CreateMutexW(nullptr,FALSE,L"Local\\AmalurCameraStatusMutexV2");
        if(mapping&&mutex)memory=static_cast<Snapshot*>(MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,sizeof(Snapshot)));
        if(!memory)return;
    }
    Snapshot s;s.pid=GetCurrentProcessId();s.frame=presents.load();s.rebuild=++serial;
    s.sampled=GetTickCount64();s.poseTick=pose.tick;s.tracked=tracked;s.playerValid=playerValid;
    memcpy(s.headPosition,pose.position,12);memcpy(s.headOrientation,pose.orientation,16);
    memcpy(s.playerPosition,&player,12);memcpy(s.nativeEye,&native.eye,12);memcpy(s.renderEye,&rendered.eye,12);
    const auto forward=rendered.target-rendered.eye;memcpy(s.renderForward,&forward,12);
    memcpy(s.nativeTarget,&native.target,12);
    memcpy(s.cachedRight,core+0x19c,12);memcpy(s.cachedUp,core+0x1a8,12);memcpy(s.cachedForward,core+0x1b4,12);
    memcpy(s.projection,core+0xc4,4);memcpy(s.projection+1,core+0xd8,4);
    memcpy(s.projection+2,core+0xe4,4);memcpy(s.projection+3,core+0xe8,4);
    auto wait=WaitForSingleObject(mutex,0);if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED)return;
    memcpy(memory,&s,sizeof(s));ReleaseMutex(mutex);
}
}
