#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <intrin.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include "MinHook.h"
#include "../tracking/pose_channel.hpp"
#include "../tracking/camera_pose.hpp"
#include "../tracking/camera_inputs.hpp"
#include "../tracking/stereo_frame.hpp"
using Microsoft::WRL::ComPtr;

// VR features default on but require fresh bridge poses; F9 is a desktop FOV probe.
// Hooks remain installed until process exit; hot unloading is unsupported.
static HMODULE selfModule;
static INIT_ONCE runtimeOnce=INIT_ONCE_STATIC_INIT, factoryOnce=INIT_ONCE_STATIC_INIT, chainOnce=INIT_ONCE_STATIC_INIT;
static SRWLOCK logLock=SRWLOCK_INIT;
static HANDLE logFile=INVALID_HANDLE_VALUE;
static decltype(&D3D11CreateDevice) realCreate;
static decltype(&D3D11CreateDeviceAndSwapChain) realCreateChain;
using CreateChain=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
using Present=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using Resize=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
static CreateChain realFactoryCreate;
static Present realPresent;
static Resize realResize;
static std::atomic<unsigned long> presents{0};
using CreateVS=HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,ID3D11VertexShader**);
static CreateVS realCreateVS;
static std::atomic<unsigned long> shaders{0};
static std::atomic<unsigned> reflectedSlots{0};
using Map=HRESULT(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,D3D11_MAP,UINT,D3D11_MAPPED_SUBRESOURCE*);
using Unmap=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT);
static Map realMap;
static Unmap realUnmap;
using Update=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,const D3D11_BOX*,const void*,UINT,UINT);
static Update realUpdate;
static std::atomic<unsigned> samples{0};
struct PendingMap { ID3D11DeviceContext* context{}; ID3D11Resource* resource{}; UINT subresource{}; void* data{}; };
static thread_local PendingMap pending;
using RebuildCamera=void(__thiscall*)(void*);
static RebuildCamera realRebuildCamera;
static std::atomic<bool> cameraProbe{false};
static std::atomic<void*> probeCamera{nullptr};
static ULONG_PTR gameBase{};
static std::atomic<unsigned> cameraLogs{0};
static std::atomic<bool> headTracking{true};
static std::atomic<bool> firstPerson{true};
static std::atomic<bool> coherentCamera{true};
static amalur::CameraInputs cameraInputs;
static void restoreCameraInputs(){
    __try {cameraInputs.restore();} __except(EXCEPTION_EXECUTE_HANDLER){cameraInputs.core=nullptr;}
}
static std::atomic<unsigned> recenterGeneration{0};
static amalur::PoseChannel poseChannel;
static amalur::PoseChannel frameChannel{true};
static std::atomic<int> stereoStatus{-1};
static ComPtr<ID3D11Device> captureDevice;
static amalur::PosePacket renderPose;
static bool sampledRenderPose=false,haveCameraForFrame=false;
static amalur::PosePacket cameraForFrame;
#include "camera_status.hpp"

static void log(const char* format,...) {
    char line[2048]; va_list args; va_start(args,format);
    int length=_vsnprintf_s(line,sizeof(line),_TRUNCATE,format,args); va_end(args);
    if(length<0) length=static_cast<int>(strlen(line));
    AcquireSRWLockExclusive(&logLock);
    if(logFile!=INVALID_HANDLE_VALUE){DWORD written;WriteFile(logFile,line,static_cast<DWORD>(length),&written,nullptr);FlushFileBuffers(logFile);}
    ReleaseSRWLockExclusive(&logLock);
}
static void location(const char* label,void* address) {
    MEMORY_BASIC_INFORMATION info{};char module[MAX_PATH]{};
    if(VirtualQuery(address,&info,sizeof(info))) {
        GetModuleFileNameA(static_cast<HMODULE>(info.AllocationBase),module,MAX_PATH);
        log("%s address=%p module=%s rva=0x%08lx\n",label,address,module,
            static_cast<unsigned long>(reinterpret_cast<ULONG_PTR>(address)-reinterpret_cast<ULONG_PTR>(info.AllocationBase)));
    }
}
static void stack() {
    void* addresses[12]{};USHORT n=CaptureStackBackTrace(1,12,addresses,nullptr);
    for(USHORT i=0;i<n;++i) location("stack",addresses[i]);
}
static HRESULT STDMETHODCALLTYPE onMap(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource,D3D11_MAP type,UINT flags,D3D11_MAPPED_SUBRESOURCE* mapped) {
    HRESULT result=realMap(context,resource,subresource,type,flags,mapped);
    if(SUCCEEDED(result)&&mapped&&mapped->pData&&samples.load()<8) {
        ComPtr<ID3D11Buffer> buffer;
        if(SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&buffer)))){
            D3D11_BUFFER_DESC desc{};buffer->GetDesc(&desc);
            if(desc.ByteWidth==256&&(desc.BindFlags&D3D11_BIND_CONSTANT_BUFFER)){
                pending={context,resource,subresource,mapped->pData};
                log("Map candidate dynamic buffer=%p type=%u\n",resource,type);stack();
            }
        }
    }
    return result;
}
static void STDMETHODCALLTYPE onUnmap(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource) {
    if(pending.context==context&&pending.resource==resource&&pending.subresource==subresource){
        if(samples.fetch_add(1)<8){
            // Copy only while the mapping is valid, before forwarding Unmap.
            float values[64];memcpy(values,pending.data,sizeof(values));
            log("Dynamic candidate buffer=%p\n",resource);
            for(unsigned row=0;row<4;++row)log("  world[%u]=%.6g %.6g %.6g %.6g  wvp[%u]=%.6g %.6g %.6g %.6g\n",row,values[row*4],values[row*4+1],values[row*4+2],values[row*4+3],row,values[32+row*4],values[33+row*4],values[34+row*4],values[35+row*4]);
            location("Unmap caller",_ReturnAddress());
        }
        pending={};
    }
    realUnmap(context,resource,subresource);
}
static void STDMETHODCALLTYPE onUpdate(ID3D11DeviceContext* context,ID3D11Resource* resource,UINT subresource,const D3D11_BOX* box,const void* data,UINT rowPitch,UINT depthPitch) {
    if(data&&!box&&samples.load()<8){
        ComPtr<ID3D11Buffer> buffer;
        if(SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&buffer)))){
            D3D11_BUFFER_DESC desc{};buffer->GetDesc(&desc);
            if(desc.ByteWidth==256&&(desc.BindFlags&D3D11_BIND_CONSTANT_BUFFER)&&samples.fetch_add(1)<8){
                float values[64];memcpy(values,data,sizeof(values));
                log("UpdateSubresource dynamic candidate=%p source=%p\n",resource,data);stack();
                for(unsigned row=0;row<4;++row)log("  world[%u]=%.6g %.6g %.6g %.6g  wvp[%u]=%.6g %.6g %.6g %.6g\n",row,values[row*4],values[row*4+1],values[row*4+2],values[row*4+3],row,values[32+row*4],values[33+row*4],values[34+row*4],values[35+row*4]);
            }
        }
    }
    realUpdate(context,resource,subresource,box,data,rowPitch,depthPitch);
}
static BOOL CALLBACK loadRuntime(PINIT_ONCE,PVOID,PVOID*) {
    wchar_t logfile[MAX_PATH]{};GetModuleFileNameW(selfModule,logfile,MAX_PATH);
    wchar_t* slash=wcsrchr(logfile,L'\\');if(slash) slash[1]=0;
    wcscat_s(logfile,L"amalur-diagnostic.log");
    logFile=CreateFileW(logfile,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    log("Amalur diagnostic v1 x86 pid=%lu exeBase=%p\n",GetCurrentProcessId(),GetModuleHandleW(nullptr));
    wchar_t system[MAX_PATH]{};GetSystemDirectoryW(system,MAX_PATH);wcscat_s(system,L"\\d3d11.dll");
    HMODULE runtime=LoadLibraryExW(system,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(runtime){realCreate=reinterpret_cast<decltype(realCreate)>(GetProcAddress(runtime,"D3D11CreateDevice"));realCreateChain=reinterpret_cast<decltype(realCreateChain)>(GetProcAddress(runtime,"D3D11CreateDeviceAndSwapChain"));}
    log("System runtime=%p create=%p createChain=%p\n",runtime,reinterpret_cast<void*>(realCreate),reinterpret_cast<void*>(realCreateChain));
    auto status=MH_Initialize();log("MinHook initialize=%s\n",MH_StatusToString(status));
    return TRUE;
}
static bool hook(void* target,void* detour,void** original,const char* name) {
    auto status=MH_CreateHook(target,detour,original);
    if(status==MH_OK) status=MH_EnableHook(target);
    log("Hook %s: %s\n",name,MH_StatusToString(status));location(name,target);
    return status==MH_OK;
}
#include "player_rig.hpp"
#include "motion_controls.hpp"
#include "weapon_control.hpp"
#include "body_visibility.hpp"
#include "rig_probe.hpp"
static bool isCameraCore(unsigned char* core) {
    // Core is embedded at BHG::Camera +8. Other embedded camera structures also
    // use the rebuild routine, so never infer ownership from its address alone.
    __try {
        return *reinterpret_cast<ULONG_PTR*>(core-8)==gameBase+0x01335d08
            && *reinterpret_cast<unsigned*>(core)==1
            && *reinterpret_cast<float*>(core+0x2c)>50.f
            && *reinterpret_cast<float*>(core+0x2c)<120.f;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static void __fastcall onRebuildCamera(void* camera,void*) {
    auto core=static_cast<unsigned char*>(camera);
    // Rebuild may be requested several times in one frame. Do not feed our
    // previous offset back into the engine rig on a subsequent request.
    if(cameraInputs.core==core)restoreCameraInputs();
    if(!isCameraCore(core)){realRebuildCamera(camera);return;}
    mgs5vr::Vec3 selectedPlayerPosition{};
    const bool selectedPlayerValid=firstPerson.load()&&player_rig::location(camera,selectedPlayerPosition);
    if(selectedPlayerValid)probeCamera.store(camera);
    void* expected=nullptr;
    probeCamera.compare_exchange_strong(expected,camera);
    if(probeCamera.load()!=camera){realRebuildCamera(camera);return;}
    const bool enabled=cameraProbe.load();
    static bool wasTracked=false;
    static mgs5vr::Pose origin{};
    static bool haveOrigin=false;
    static unsigned centeredGeneration=0,bridgeCenter=0;
    static amalur::HeadingAnchor headingAnchor;
    static amalur::SnapHeading snapHeading;
    static amalur::BodyHeading bodyHeading;
    static void* headingCamera{};
    static void* headingPlayer{};
    static ULONGLONG lastOpenAttempt=0,lastPoseTick=0;
    static amalur::CameraPose lastTrackedCamera{};
    static bool hadTrackedCamera=false;
    amalur::PosePacket packet;
    bool tracked=false;
    amalur::CameraPose originalCamera{},adjusted{};
    memcpy(&originalCamera.eye,core+4,sizeof(mgs5vr::Vec3));
    memcpy(&originalCamera.target,core+0x14,sizeof(mgs5vr::Vec3));
    memcpy(&originalCamera.up,core+0x1c0,sizeof(mgs5vr::Vec3));
    if(headTracking.load()) {
        auto now=GetTickCount64();
        if(now-lastOpenAttempt>1000){poseChannel.open(false);lastOpenAttempt=now;}
        if(!sampledRenderPose){renderPose={};poseChannel.read(renderPose);sampledRenderPose=true;}
        packet=renderPose;
        const bool desktopPose=packet.gameMode==2&&packet.valid&&packet.tick<=now&&now-packet.tick<250;
        weapon_control::desktopPose.store(desktopPose);
        const bool useFirstPerson=firstPerson.load()&&!desktopPose;
        if(packet.valid&&packet.tick<=now&&now-packet.tick<250){
            mgs5vr::Pose head{{packet.orientation[0],packet.orientation[1],packet.orientation[2],packet.orientation[3]},{packet.position[0],packet.position[1],packet.position[2]}};
            if(mgs5vr::valid(head)){
                unsigned generation=recenterGeneration.load();
                if(!haveOrigin||centeredGeneration!=generation||bridgeCenter!=packet.recenter){origin=amalur::levelOrigin(head);haveOrigin=true;centeredGeneration=generation;bridgeCenter=packet.recenter;headingAnchor.reset();snapHeading.reset();bodyHeading.reset();log("Head tracking recentered (position and heading; horizon level)\n");}
                auto relative=mgs5vr::compose(mgs5vr::inverse(origin),head);
                auto baseCamera=originalCamera;
                mgs5vr::Vec3 playerPosition{};
                if((firstPerson.load()||arm_rig::enabled.load())&&player_rig::location(camera,playerPosition)){
                    auto heading=originalCamera.target-originalCamera.eye;heading.z=0;
                    auto owner=player_rig::player.load();
                    if(headingCamera!=camera||headingPlayer!=owner){headingAnchor.reset();snapHeading.reset();bodyHeading.reset();headingCamera=camera;headingPlayer=owner;}
                    if(useFirstPerson){
                        headingAnchor.get(heading,heading);
                        const auto controls=motion_controls::viewControls();
                        heading=snapHeading.apply(heading,controls.session,controls.turnYawDegrees);
                    }else {headingAnchor.reset();snapHeading.reset();}
                    if(amalur::normalize(heading)){
                        auto handCamera=baseCamera;
                        handCamera.eye=playerPosition+mgs5vr::Vec3{0,0,185}+heading*15.f;
                        handCamera.target=handCamera.eye+heading*200.f;handCamera.up={0,0,1};
                        if(!bodyHeading.valid){mgs5vr::Vec3 seed;bodyHeading.get(heading,seed);}
                        weapon_control::sample(handCamera,origin,packet.worldScale,centeredGeneration+bridgeCenter);
                        if(useFirstPerson)baseCamera=handCamera;
                    }
                }
                tracked=!desktopPose&&amalur::trackedCamera(baseCamera,relative,packet.worldScale,adjusted);
            }
        }
    }else haveOrigin=false;
    mgs5vr::Vec3 bodyForward{};
    bool bodyHeadingValid=tracked&&bodyHeading.get(adjusted.target-adjusted.eye,bodyForward);
    // Keep the collar/shoulders behind the eyes without moving the camera with gait.
    auto anchor=adjusted.eye-bodyForward*18.f;
    arm_rig::sampleBody(anchor,bodyHeadingValid&&packet.gameMode?packet.tick:0);
    if(tracked){
        if(firstPerson.load()&&bodyHeadingValid&&packet.gameMode&&motion_controls::gameFocused())
            player_rig::face(camera,bodyForward);
        memcpy(core+4,&adjusted.eye,sizeof(mgs5vr::Vec3));
        memcpy(core+0x14,&adjusted.target,sizeof(mgs5vr::Vec3));
        memcpy(core+0x1c0,&adjusted.up,sizeof(mgs5vr::Vec3));
        // Locomotion can change the camera between two equal headset samples.
        // Do not pair a newly published pose with the previous cached matrices.
        if(packet.tick!=lastPoseTick||!hadTrackedCamera||amalur::cameraChanged(adjusted,lastTrackedCamera))core[0x35e]|=1;
        lastPoseTick=packet.tick;
        lastTrackedCamera=adjusted;hadTrackedCamera=true;
    }
    else hadTrackedCamera=false;
    if(tracked!=wasTracked){core[0x35e]|=1;log("Native head tracking %s (100 units/m provisional)\n",tracked?"ACTIVE":"INACTIVE");}
    // Apply on dirty rebuilds only, preserving the engine's matrix/frustum cache.
    // F9 is sampled here too, so toggling forces a refresh on the selected camera.
    static bool wasEnabled=false;
    if(enabled!=wasEnabled)core[0x35e]|=1;
    const bool dirty=(core[0x35e]&1)!=0;
    float original=*reinterpret_cast<float*>(core+0x2c);
    // A wide symmetric image is cropped to each runtime eye's asymmetric FOV.
    if(tracked&&packet.gameMode&&dirty)*reinterpret_cast<float*>(core+0x2c)=packet.horizontalFov;
    else if(enabled&&dirty)*reinterpret_cast<float*>(core+0x2c)=original*.85f;
    realRebuildCamera(camera);
    camera_status::publish(packet,tracked,selectedPlayerValid,selectedPlayerPosition,originalCamera,tracked?adjusted:originalCamera);
    haveCameraForFrame=false;
    if(tracked&&packet.gameMode){
        packet.projectionX=*reinterpret_cast<float*>(core+0xc4);
        packet.projectionY=*reinterpret_cast<float*>(core+0xd8);
        packet.stereoStatus=stereoStatus.load();
        if(frameChannel.open(true))frameChannel.publish(packet);
        cameraForFrame=packet;haveCameraForFrame=true;
    }
    const bool retainInputs=tracked&&packet.gameMode&&firstPerson.load()&&coherentCamera.load();
    if(retainInputs){
        cameraInputs={core,originalCamera,adjusted,original,*reinterpret_cast<float*>(core+0x2c)};
    }else{
        memcpy(core+4,&originalCamera.eye,sizeof(mgs5vr::Vec3));
        memcpy(core+0x14,&originalCamera.target,sizeof(mgs5vr::Vec3));
        memcpy(core+0x1c0,&originalCamera.up,sizeof(mgs5vr::Vec3));
        *reinterpret_cast<float*>(core+0x2c)=original;
    }
    wasTracked=tracked;
    if((dirty&&cameraLogs.fetch_add(1)<4)||enabled!=wasEnabled)
        log("Camera core=%p probe=%d inputFov=%.6g projectionXY=%.6g,%.6g\n",camera,enabled,original,*reinterpret_cast<float*>(core+0xc4),*reinterpret_cast<float*>(core+0xd8));
    wasEnabled=enabled;
}
static void installCameraProbe() {
    wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
    const wchar_t* name=wcsrchr(executable,L'\\');name=name?name+1:executable;
    if(_wcsicmp(name,L"koa.exe")!=0)return;
    gameBase=reinterpret_cast<ULONG_PTR>(GetModuleHandleW(nullptr));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(gameBase);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(gameBase+dos->e_lfanew);
    if(nt->OptionalHeader.SizeOfImage<0x01335d10)return;
    auto target=reinterpret_cast<unsigned char*>(gameBase+0x008e1c80);
    const unsigned char expected[]={0x83,0xec,0x40,0x53,0x8b,0xd9,0xf6,0x83,0x5e,0x03,0,0,1};
    if(memcmp(target,expected,sizeof(expected))!=0){log("Camera signature mismatch; native hook skipped\n");return;}
    player_rig::install();
    weapon_control::install();
    body_visibility::install();
    rig_probe::install();
    motion_controls::install();
    hook(target,reinterpret_cast<void*>(&onRebuildCamera),reinterpret_cast<void**>(&realRebuildCamera),"CameraRebuild (F9 FOV probe)");
}
static void members(ID3D11ShaderReflectionType* type,UINT offset,unsigned depth) {
    D3D11_SHADER_TYPE_DESC td{};if(depth>3||FAILED(type->GetDesc(&td)))return;
    for(UINT i=0;i<td.Members;++i){auto child=type->GetMemberTypeByIndex(i);D3D11_SHADER_TYPE_DESC cd{};if(FAILED(child->GetDesc(&cd)))continue;
        log("      member %s offset=%u class=%u rows=%u columns=%u elements=%u\n",type->GetMemberTypeName(i),offset+cd.Offset,cd.Class,cd.Rows,cd.Columns,cd.Elements);
        members(child,offset+cd.Offset,depth+1);
    }
}
#include "hud_trace.hpp"
static HRESULT STDMETHODCALLTYPE onCreateVS(ID3D11Device* device,const void* bytes,SIZE_T size,ID3D11ClassLinkage* linkage,ID3D11VertexShader** shader) {
    HRESULT result=realCreateVS(device,bytes,size,linkage,shader);
    if(gameBase&&SUCCEEDED(result)&&shader&&*shader)hud_trace::tag(*shader,bytes,size);
    auto number=++shaders;
    if(SUCCEEDED(result)&&bytes&&number<=256) {
        ComPtr<ID3D11ShaderReflection> reflection;
        if(SUCCEEDED(D3DReflect(bytes,size,IID_PPV_ARGS(&reflection)))) {
            D3D11_SHADER_DESC desc{};reflection->GetDesc(&desc);
            log("VS #%lu bytes=%zu constantBuffers=%u\n",number,size,desc.ConstantBuffers);
            for(UINT i=0;i<desc.ConstantBuffers;++i){
                auto buffer=reflection->GetConstantBufferByIndex(i);D3D11_SHADER_BUFFER_DESC bd{};
                if(FAILED(buffer->GetDesc(&bd)))continue;
                D3D11_SHADER_INPUT_BIND_DESC binding{};reflection->GetResourceBindingDescByName(bd.Name,&binding);
                log("  CB %s size=%u slot=%u variables=%u\n",bd.Name,bd.Size,binding.BindPoint,bd.Variables);
                unsigned bit=binding.BindPoint<32 ? 1u<<binding.BindPoint : 0;
                bool detail=bit && !(reflectedSlots.fetch_or(bit)&bit);
                for(UINT j=0;j<bd.Variables;++j){D3D11_SHADER_VARIABLE_DESC v{};auto variable=buffer->GetVariableByIndex(j);if(SUCCEEDED(variable->GetDesc(&v))){log("    %s offset=%u size=%u used=%u\n",v.Name,v.StartOffset,v.Size,(v.uFlags&D3D_SVF_USED)!=0);if(detail)members(variable->GetType(),v.StartOffset,0);}}
            }
        }else log("VS #%lu reflection unavailable bytes=%zu\n",number,size);
    }
    return result;
}
// Query IDs/signatures verified against NVIDIA's public nvapi_interface.h and
// nvapi_lite_stereo.h. Calls target only the already-loaded local geo-11 shim.
static void applyStereoSettings(IDXGISwapChain* chain){
    using Query=void*(__cdecl*)(unsigned);using Create=int(__cdecl*)(IUnknown*,void**);
    using Set=int(__cdecl*)(void*,float);static void* handle{};static Set setDepth{},setConvergence{};
    static ULONGLONG next{};static float oldDepth=-1,oldConvergence=-1;
    auto now=GetTickCount64();if(!gameBase||now<next)return;next=now+500;
    amalur::PosePacket p;if(!poseChannel.open(false)||!poseChannel.read(p)||!p.gameMode)return;
    if(!handle){auto module=GetModuleHandleW(L"nvapi.dll");if(!module)return;
        auto query=reinterpret_cast<Query>(GetProcAddress(module,"nvapi_QueryInterface"));if(!query){log("Stereo settings: query export missing\n");return;}
        auto create=reinterpret_cast<Create>(query(0xac7e37f4));setDepth=reinterpret_cast<Set>(query(0x5c069fa3));setConvergence=reinterpret_cast<Set>(query(0x3dd6b54b));
        ComPtr<ID3D11Device> device;if(!create||!setDepth||!setConvergence){static bool reported=false;if(!reported){log("Stereo settings: missing API create=%p depth=%p convergence=%p\n",reinterpret_cast<void*>(create),reinterpret_cast<void*>(setDepth),reinterpret_cast<void*>(setConvergence));reported=true;}return;}
        if(FAILED(chain->GetDevice(IID_PPV_ARGS(&device))))return;
        int result=create(device.Get(),&handle);stereoStatus.store(result);log("Stereo settings handle: status=%d handle=%p\n",result,handle);if(result!=0||!handle)return;
    }
    if(p.depth==oldDepth&&p.convergence==oldConvergence)return;
    if(!std::isfinite(p.depth)||p.depth<0||p.depth>100||!std::isfinite(p.convergence)||p.convergence<1||p.convergence>1000)return;
    int a=setDepth(handle,p.depth),b=setConvergence(handle,p.convergence);stereoStatus.store(a?a:b);
    log("Stereo settings depth=%.2f convergence=%.2f status=%d,%d\n",p.depth,p.convergence,a,b);
    if(!a&&!b){oldDepth=p.depth;oldConvergence=p.convergence;}
}
#include "source_resolution.hpp"
static void publishStereoFrame(){
    if(!captureDevice)return;
    static HANDLE mapping{};static const ULONG_PTR* sharedHandle{};
    static ULONG_PTR opened{};static ComPtr<ID3D11Texture2D> stereo;
    static amalur::StereoPublisher publisher;
    if(!mapping){mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,L"Local\\KatangaMappedFile");if(!mapping)return;sharedHandle=static_cast<const ULONG_PTR*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(ULONG_PTR)));}
    if(!sharedHandle||!*sharedHandle)return;
    if(opened!=*sharedHandle||!stereo){stereo.Reset();opened=*sharedHandle;if(FAILED(captureDevice->OpenSharedResource(reinterpret_cast<HANDLE>(opened),IID_PPV_ARGS(&stereo))))return;}
    ComPtr<ID3D11DeviceContext> context;captureDevice->GetImmediateContext(&context);
    // Executed after geo-11 Present has assembled the packed stereo image on
    // this immediate context, before the next game frame can overwrite it.
    amalur::PosePacket metadata;
    if(haveCameraForFrame)metadata=cameraForFrame;
    static unsigned published=0;
    if(publisher.publish(captureDevice.Get(),context.Get(),stereo.Get(),metadata)&&++published%300==1)
        log("Paired stereo frame published #%u tracked=%u poseTick=%llu\n",published,metadata.valid,metadata.tick);
}
static HRESULT STDMETHODCALLTYPE onPresent(IDXGISwapChain* chain,UINT sync,UINT flags) {
    hud_size::poll();
    source_resolution::apply(chain);
    applyStereoSettings(chain);
    static bool f8Down=false;bool down=(GetAsyncKeyState(VK_F8)&0x8000)!=0;
    if(down&&!f8Down){samples.store(0);hud_trace::rearm();log("F8: rearmed dynamic-buffer and HUD samples\n");}f8Down=down;
    static bool f9Down=false;bool f9=(GetAsyncKeyState(VK_F9)&0x8000)!=0;
    if(f9&&!f9Down){bool enabled=!cameraProbe.load();cameraProbe.store(enabled);log("F9: camera FOV probe %s\n",enabled?"ON":"OFF");}f9Down=f9;
    static bool f10Down=false,f7Down=false;bool f10=(GetAsyncKeyState(VK_F10)&0x8000)!=0,f7=(GetAsyncKeyState(VK_F7)&0x8000)!=0;
    if(f10&&!f10Down){bool enabled=!headTracking.load();headTracking.store(enabled);++recenterGeneration;log("F10: head tracking requested %s\n",enabled?"ON":"OFF");}f10Down=f10;
    if(f7&&!f7Down){++recenterGeneration;log("F7: recenter requested\n");}f7Down=f7;
    static bool f3Down=false;bool f3=(GetAsyncKeyState(VK_F3)&0x8000)!=0;
    if(f3&&!f3Down&&motion_controls::gameFocused()){bool enabled=!weapon_control::enabled.load();weapon_control::enabled.store(enabled);++recenterGeneration;log("F3: experimental weapon pose %s\n",enabled?"ON":"OFF");}f3Down=f3;
    static bool f5Down=false;bool f5=(GetAsyncKeyState(VK_F5)&0x8000)!=0;
    if(f5&&!f5Down&&motion_controls::gameFocused()){bool enabled=!firstPerson.load();firstPerson.store(enabled);if(enabled)headTracking.store(true);++recenterGeneration;log("F5: experimental first-person and Touch movement %s\n",enabled?"ON":"OFF");}f5Down=f5;
    static bool f2Down=false;bool f2=(GetAsyncKeyState(VK_F2)&0x8000)!=0;
    if(f2&&!f2Down&&motion_controls::gameFocused()){coherentCamera.store(!coherentCamera.load());log("F2: camera input consistency %s\n",coherentCamera.load()?"ON":"OFF");}f2Down=f2;
    static bool f1Down=false;bool f1=(GetAsyncKeyState(VK_F1)&0x8000)!=0;
    if(f1&&!f1Down&&motion_controls::gameFocused()){body_visibility::enabled.store(!body_visibility::enabled.load());log("F1: first-person body hiding %s\n",body_visibility::enabled.load()?"ON":"OFF");}f1Down=f1;
    body_visibility::update();
    static bool f4Down=false,bodyBeforeArm=true,weaponBeforeArm=false;bool f4=(GetAsyncKeyState(VK_F4)&0x8000)!=0;
    if(f4&&!f4Down&&motion_controls::gameFocused()){
        if(arm_rig::enabled.exchange(!arm_rig::enabled.load())){body_visibility::enabled.store(bodyBeforeArm);weapon_control::enabled.store(weaponBeforeArm);}
        else {rig_probe::disable();bodyBeforeArm=body_visibility::enabled.load();weaponBeforeArm=weapon_control::enabled.load();
            body_visibility::enabled.store(false);weapon_control::enabled.store(false);headTracking.store(true);arm_rig::resetCalibration();}
        log("F4: right arm IK %s (mesh updates=%u)\n",arm_rig::enabled.load()?"ON":"OFF",arm_rig::samples.load());
    }f4Down=f4;
    static bool f6Down=false,bodyHideBeforeProbe=true;bool f6=(GetAsyncKeyState(VK_F6)&0x8000)!=0;
    if(f6&&!f6Down&&motion_controls::gameFocused()&&!arm_rig::enabled.load()){
        if(rig_probe::enabled.load()){rig_probe::disable();body_visibility::enabled.store(bodyHideBeforeProbe);}
        else {bodyHideBeforeProbe=body_visibility::enabled.load();body_visibility::enabled.store(false);rig_probe::enabled.store(true);}
        log("F6: player wrist discovery displacement %s (samples=%u)\n",rig_probe::enabled.load()?"ON":"OFF",rig_probe::samples.load());
    }f6Down=f6;
    auto count=++presents;
    rig_status::publish();
    if(count<=3){log("Present #%lu chain=%p sync=%u flags=0x%x\n",count,chain,sync,flags);stack();}
    HRESULT result=realPresent(chain,sync,flags);
    restoreCameraInputs();
    if(SUCCEEDED(result)&&!(flags&DXGI_PRESENT_TEST))publishStereoFrame();
    // If the engine reuses its cached camera matrices next frame, their last
    // rendered pose remains the correct attribution until that camera rebuilds.
    sampledRenderPose=false;
    if(count%600==0 || (FAILED(result)&&count<=10))log("Present #%lu result=0x%08lx\n",count,static_cast<unsigned long>(result));
    return result;
}
static HRESULT STDMETHODCALLTYPE onResize(IDXGISwapChain* chain,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags) {
    log("ResizeBuffers chain=%p count=%u size=%ux%u format=%u flags=0x%x\n",chain,count,width,height,format,flags);
    HRESULT result=realResize(chain,count,width,height,format,flags);log("ResizeBuffers result=0x%08lx\n",static_cast<unsigned long>(result));return result;
}
static BOOL CALLBACK hookChain(PINIT_ONCE,PVOID parameter,PVOID*) {
    auto chain=static_cast<IDXGISwapChain*>(parameter);DXGI_SWAP_CHAIN_DESC d{};
    if(SUCCEEDED(chain->GetDesc(&d)))log("Swapchain size=%ux%u format=%u samples=%u buffers=%u windowed=%d effect=%u\n",d.BufferDesc.Width,d.BufferDesc.Height,d.BufferDesc.Format,d.SampleDesc.Count,d.BufferCount,d.Windowed,d.SwapEffect);
    void** vt=*reinterpret_cast<void***>(chain);
    hook(vt[8],reinterpret_cast<void*>(&onPresent),reinterpret_cast<void**>(&realPresent),"Present");
    hook(vt[13],reinterpret_cast<void*>(&onResize),reinterpret_cast<void**>(&realResize),"ResizeBuffers");
    return TRUE;
}
static HRESULT STDMETHODCALLTYPE onCreateChain(IDXGIFactory* factory,IUnknown* device,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** chain) {
    HRESULT result=realFactoryCreate(factory,device,desc,chain);
    log("CreateSwapChain result=0x%08lx\n",static_cast<unsigned long>(result));
    if(SUCCEEDED(result)&&chain&&*chain)InitOnceExecuteOnce(&chainOnce,hookChain,*chain,nullptr);
    return result;
}
static BOOL CALLBACK hookFactory(PINIT_ONCE,PVOID parameter,PVOID*) {
    installCameraProbe();
    auto device=static_cast<ID3D11Device*>(parameter);ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory> factory;
    captureDevice=device;
    void** deviceVt=*reinterpret_cast<void***>(device);
    hook(deviceVt[12],reinterpret_cast<void*>(&onCreateVS),reinterpret_cast<void**>(&realCreateVS),"CreateVertexShader");
    ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
    void** contextVt=*reinterpret_cast<void***>(context.Get());
    hook(contextVt[14],reinterpret_cast<void*>(&onMap),reinterpret_cast<void**>(&realMap),"Map");
    hook(contextVt[15],reinterpret_cast<void*>(&onUnmap),reinterpret_cast<void**>(&realUnmap),"Unmap");
    hook(contextVt[48],reinterpret_cast<void*>(&onUpdate),reinterpret_cast<void**>(&realUpdate),"UpdateSubresource");
    if(gameBase)hud_trace::install(contextVt);
    if(FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi)))||FAILED(dxgi->GetAdapter(&adapter))||FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))){log("Could not locate factory\n");return TRUE;}
    DXGI_ADAPTER_DESC d{};adapter->GetDesc(&d);log("Adapter VID=0x%x device=0x%x LUID=%08lx:%08lx feature=0x%x\n",d.VendorId,d.DeviceId,static_cast<unsigned long>(d.AdapterLuid.HighPart),d.AdapterLuid.LowPart,device->GetFeatureLevel());
    void** vt=*reinterpret_cast<void***>(factory.Get());
    hook(vt[10],reinterpret_cast<void*>(&onCreateChain),reinterpret_cast<void**>(&realFactoryCreate),"CreateSwapChain");return TRUE;
}
extern "C" HRESULT WINAPI ProxyCreateDevice(IDXGIAdapter* adapter,D3D_DRIVER_TYPE type,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,ID3D11Device** device,D3D_FEATURE_LEVEL* selected,ID3D11DeviceContext** context) {
    InitOnceExecuteOnce(&runtimeOnce,loadRuntime,nullptr,nullptr);if(!realCreate)return E_FAIL;
    location("D3D11CreateDevice caller",_ReturnAddress());
    HRESULT result=realCreate(adapter,type,software,flags,levels,count,sdk,device,selected,context);
    log("D3D11CreateDevice result=0x%08lx flags=0x%x\n",static_cast<unsigned long>(result),flags);
    if(SUCCEEDED(result)&&device&&*device)InitOnceExecuteOnce(&factoryOnce,hookFactory,*device,nullptr);
    return result;
}
extern "C" HRESULT WINAPI ProxyCreateDeviceAndSwapChain(IDXGIAdapter* adapter,D3D_DRIVER_TYPE type,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,const DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** chain,ID3D11Device** device,D3D_FEATURE_LEVEL* selected,ID3D11DeviceContext** context) {
    InitOnceExecuteOnce(&runtimeOnce,loadRuntime,nullptr,nullptr);if(!realCreateChain)return E_FAIL;
    HRESULT result=realCreateChain(adapter,type,software,flags,levels,count,sdk,desc,chain,device,selected,context);
    log("D3D11CreateDeviceAndSwapChain result=0x%08lx\n",static_cast<unsigned long>(result));
    if(SUCCEEDED(result)&&device&&*device)InitOnceExecuteOnce(&factoryOnce,hookFactory,*device,nullptr);
    if(SUCCEEDED(result)&&chain&&*chain)InitOnceExecuteOnce(&chainOnce,hookChain,*chain,nullptr);
    return result;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){selfModule=module;DisableThreadLibraryCalls(module);}return TRUE;
}
