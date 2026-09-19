#include "camera_pose.hpp"
#include "pose_channel.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
static void expect(bool value,const char* label){if(!value){printf("FAIL: %s\n",label);std::exit(1);}}
static bool closeEnough(float a,float b){return std::abs(a-b)<.001f;}
int main(int argc,char** argv){
    if(argc==2&&std::strcmp(argv[1],"--read")==0){
        amalur::PoseChannel channel;amalur::PosePacket p;
        if(!channel.open(false)||!channel.read(p)){puts("No fresh tracked pose");return 2;}
        printf("Fresh live pose xyz=%.4f,%.4f,%.4f q=%.4f,%.4f,%.4f,%.4f\n",p.position[0],p.position[1],p.position[2],p.orientation[0],p.orientation[1],p.orientation[2],p.orientation[3]);return 0;
    }
    amalur::CameraPose base{{10,20,30},{10,220,30},{0,0,1}},out{};
    expect(amalur::trackedCamera(base,{},100,out),"identity accepted");
    expect(closeEnough(out.eye.x,10)&&closeEnough(out.target.y,220)&&closeEnough(out.up.z,1),"identity preserves rig");
    expect(amalur::trackedCamera(base,{{},{.1f,.2f,-.3f}},100,out),"translation accepted");
    expect(closeEnough(out.eye.x,0)&&closeEnough(out.eye.y,50)&&closeEnough(out.eye.z,50),"native right/up/forward translation axes");
    const float s=std::sqrt(.5f);
    expect(amalur::trackedCamera(base,{{0,s,0,s},{}},100,out),"yaw accepted");
    expect(closeEnough(out.target.x,210)&&closeEnough(out.target.y,20),"90 degree left yaw in native camera basis");
    expect(mgs5vr::dot(out.target-out.eye,amalur::Vec3{-1,0,0})<0,"left head yaw points toward native screen-left");
    expect(amalur::trackedCamera(base,{{0,0,s,s},{}},100,out),"roll accepted");
    expect(closeEnough(out.up.x,1)&&closeEnough(out.up.z,0)&&closeEnough(out.target.y,220),"roll changes up without changing forward");
    mgs5vr::Pose origin{{0,s,0,s},{2,3,4}};
    auto relative=mgs5vr::compose(mgs5vr::inverse(origin),origin);
    expect(amalur::trackedCamera(base,relative,100,out)&&closeEnough(out.eye.x,10)&&closeEnough(out.target.y,220),"recenter cancels pose");
    expect(!amalur::trackedCamera(base,{{0,0,0,0},{}},100,out),"invalid quaternion rejected");
    auto level=amalur::levelOrigin({{0,0,s,s},{2,3,4}});
    auto upright=mgs5vr::compose(mgs5vr::inverse(level),mgs5vr::Pose{{},{2,3,4}});
    expect(amalur::trackedCamera(base,upright,100,out)&&closeEnough(out.up.z,1)&&closeEnough(out.up.x,0),"recenter while tilted does not retain roll when upright");
    auto heading=amalur::levelOrigin(origin);
    relative=mgs5vr::compose(mgs5vr::inverse(heading),origin);
    expect(amalur::trackedCamera(base,relative,100,out)&&closeEnough(out.target.y,220),"level recenter cancels heading");
    auto translated=base;translated.eye.x+=1;translated.target.x+=1;
    expect(amalur::cameraChanged(base,translated)&&!amalur::cameraChanged(base,base),"locomotion invalidates cached camera even with unchanged headset sample");
    auto body=amalur::bodyAnchor(base,10);
    expect(closeEnough(body.x,base.eye.x)&&closeEnough(body.y,base.eye.y-10)&&closeEnough(body.z,base.eye.z),"body clearance follows horizontal viewing direction");
    auto pitched=base;pitched.target.z+=100;
    auto pitchedBody=amalur::bodyAnchor(pitched,10);
    expect(closeEnough(pitchedBody.y,body.y)&&closeEnough(pitchedBody.z,body.z),"looking up does not lift body anchor");
    amalur::HeadingAnchor anchor; mgs5vr::Vec3 fixed;
    expect(anchor.get({0,200,50},fixed)&&closeEnough(fixed.y,1)&&closeEnough(fixed.z,0),"anchor uses horizontal heading");
    for(int frame=0;frame<500;++frame){
        float angle=frame*.02f;expect(anchor.get({std::sin(angle),std::cos(angle),0},fixed)&&closeEnough(fixed.x,0)&&closeEnough(fixed.y,1),"native body/camera turns cannot accumulate in VR heading");
    }
    anchor.reset();expect(anchor.get({1,0,0},fixed)&&closeEnough(fixed.x,1),"recenter accepts new world heading");
    if(argc==2&&std::strcmp(argv[1],"--math")==0){puts("PASS: camera axes, yaw, roll, level recenter, stable heading and invalid pose");return 0;}
    const wchar_t* map=L"Local\\AmalurVRTestPose";const wchar_t* mutex=L"Local\\AmalurVRTestMutex";
    amalur::PoseChannel writer(map,mutex),reader(map,mutex);expect(writer.open(true)&&reader.open(false),"pose channel opens");
    amalur::PosePacket p;p.valid=1;p.tick=GetTickCount64();p.position[0]=.125f;writer.publish(p);
    amalur::PosePacket got;expect(reader.read(got)&&closeEnough(got.position[0],.125f),"whole packet transfer");
    HANDLE locked=CreateEventW(nullptr,TRUE,FALSE,nullptr),release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    std::thread contender([&]{HANDLE h=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,mutex);WaitForSingleObject(h,INFINITE);SetEvent(locked);WaitForSingleObject(release,INFINITE);ReleaseMutex(h);CloseHandle(h);});
    WaitForSingleObject(locked,INFINITE);
    expect(reader.read(got)&&closeEnough(got.position[0],.125f),"contention preserves complete fresh pose");
    Sleep(260);expect(!reader.read(got),"cached pose still expires under contention");
    SetEvent(release);contender.join();CloseHandle(locked);CloseHandle(release);
    p.tick-=1000;writer.publish(p);expect(!reader.read(got),"stale pose rejected");
    p.tick=GetTickCount64();p.valid=0;writer.publish(p);expect(!reader.read(got),"tracking loss rejected");
    puts("PASS: camera axes, yaw, roll, recenter, invalid pose and channel freshness");return 0;
}

