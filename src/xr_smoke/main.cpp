#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../tracking/pose_channel.hpp"
#include "../tracking/menu_view.hpp"
#include "../tracking/motion_input.hpp"
#include "../tracking/rig_status.hpp"
#include "../tracking/grip_settings.hpp"
#include "stereo_source.hpp"
#include "render_pose.hpp"
#include "menu_anchor.hpp"
#include "game_lifetime.hpp"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
static XrInstance instance{};
static void xrcheck(XrResult r, const char* operation) {
    if (XR_FAILED(r)) {
        char name[XR_MAX_RESULT_STRING_SIZE]{};
        if(instance) xrResultToString(instance,r,name);
        throw std::runtime_error(std::string(operation)+": "+name+" ("+std::to_string(r)+")");
    }
}
#define XR(call) xrcheck((call),#call)
static void hrcheck(HRESULT r) { if(FAILED(r)) throw std::runtime_error("D3D failure: "+std::to_string(r)); }
static XrPath path(const char* s) { XrPath p{}; XR(xrStringToPath(instance,s,&p)); return p; }
#include "settings_panel.hpp"
#include "../tracking/hud_settings.hpp"
struct Resources {
    XrSession session{}; XrSpace local{},view{}; XrActionSet actions{};
    std::array<XrSpace,2> hands{};
    std::array<XrSwapchain,2> chains{};
    ~Resources() {
        for(auto s:chains) if(s) xrDestroySwapchain(s);
        for(auto s:hands) if(s) xrDestroySpace(s);
        if(local) xrDestroySpace(local);
        if(view) xrDestroySpace(view);
        if(session) xrDestroySession(session);
        if(actions) xrDestroyActionSet(actions);
        if(instance) { xrDestroyInstance(instance); instance={}; }
    }
};
int main(int argc,char** argv) {
    if(argc==2&&std::string(argv[1])=="--pose-math"){
        const float s=std::sqrt(.5f);
        XrPosef head{{0,s,0,s},{1,2,3}},eye=head,rendered{{0,0,0,1},{5,6,7}};
        auto cant=XMQuaternionRotationAxis(XMVectorSet(0,0,1,0),.3f);
        XMFLOAT4 q,c;XMStoreFloat4(&c,cant);XMStoreFloat4(&q,XMQuaternionMultiply(cant,XMVectorSet(0,s,0,s)));
        eye.orientation={q.x,q.y,q.z,q.w};eye.position.z-=.032f;
        auto result=renderedEyePose(head,eye,rendered);
        float dot=result.orientation.x*c.x+result.orientation.y*c.y+result.orientation.z*c.z+result.orientation.w*c.w;
        bool good=std::abs(std::abs(dot)-1)<.0001f&&std::abs(result.position.x-5.032f)<.0001f&&std::abs(result.position.y-6)<.0001f&&std::abs(result.position.z-7)<.0001f;
        std::cout<<(good?"PASS":"FAIL")<<": rendered pose preserves eye cant and offset across 90-degree head rotation\n";return good?0:1;
    }
    if(argc==2&&std::string(argv[1])=="--menu-anchor"){
        MenuAnchor anchor;XrPosef first{{0,0,0,1},{1,2,3}};
        anchor.update(first,0);auto initial=anchor.pose;
        const float v=std::sqrt(.5f);XrPosef turned{{0,v,0,v},{4,5,6}};
        anchor.update(turned,0);
        bool good=anchor.pose.position.x==initial.position.x&&anchor.pose.position.z==initial.position.z&&anchor.pose.orientation.w==1;
        anchor.update(turned,1);
        good=good&&std::abs(anchor.pose.position.x-2)<.0001f&&std::abs(anchor.pose.position.z-6)<.0001f;
        anchor.close();anchor.update(first,1);
        good=good&&anchor.pose.position.x==1&&anchor.pose.position.y==2&&anchor.pose.position.z==1;
        for(float pitch:{-.6f,0.f,.7f})for(float roll:{-.4f,0.f,.5f}){
            XMFLOAT4 tilt;XMStoreFloat4(&tilt,XMQuaternionRotationRollPitchYaw(pitch,.8f,roll));
            XrPosef tilted{{tilt.x,tilt.y,tilt.z,tilt.w},{1,2,3}};
            anchor.close();anchor.update(tilted,2);
            good=good&&std::abs(anchor.pose.orientation.x)<1e-5f&&std::abs(anchor.pose.orientation.z)<1e-5f
                &&std::abs(anchor.pose.orientation.y-std::sin(.4f))<1e-5f&&std::abs(anchor.pose.position.y-2)<1e-5f;
        }
        XMFLOAT4 vertical;XMStoreFloat4(&vertical,XMQuaternionRotationRollPitchYaw(XM_PIDIV2,0,0));
        anchor.update({{vertical.x,vertical.y,vertical.z,vertical.w},{1,2,3}},3);
        good=good&&std::isfinite(anchor.pose.position.x)&&std::abs(anchor.pose.orientation.y-std::sin(.4f))<1e-5f;
        std::cout<<(good?"PASS":"FAIL")<<": menu stays fixed, upright and eye-level across pitch/roll/yaw; recenter/reopening and vertical-look fallback pass\n";return good?0:1;
    }
    bool live=false,trackingMode=false,gameMode=false;
    // Measured at geo-11 separation=20: distant geometry has ~53 px disparity
    // across 2560 px eyes, with near disparity of the opposite depth sign.
    // Remove each source eye's infinity offset and reverse the source order.
    // This is provisional screen alignment, not physical IPD calibration.
    for(int i=1;i<argc;++i) {
        if(std::string(argv[i])=="--session") live=true;
        else if(std::string(argv[i])=="--track") {live=true;trackingMode=true;}
        else if(std::string(argv[i])=="--game") {live=true;trackingMode=true;gameMode=true;}
        else if(std::string(argv[i])!="--probe") { std::cerr<<"Usage: amalur-xr-smoke [--probe|--session|--track|--game]\n"; return 1; }
    }
    std::cout.setf(std::ios::unitbuf);
    std::cout<<"Amalur XR smoke v1, process bits="<<sizeof(void*)*8<<", mode="<<(live?"session":"probe")<<"\n";
    Resources r;
    VrSettings settings;
    amalur::HudSettingsChannel hudSettings;
    amalur::HudSettingsChannel menuSettings{L"Local\\AmalurMenuSettingsV1",L"Local\\AmalurMenuSettingsMutexV1"};
    amalur::GripSettingsChannel gripSettings;
    SettingsPanel panel;
    DeveloperTools developer;
    if(gameMode)settings.captureInput();
    amalur::PoseChannel poses;
    amalur::MotionInputChannel motionInput;
    amalur::TouchMapper touchMapper;
    amalur::RigStatusChannel rigStatus;amalur::RigStatus latestRig;
    amalur::PoseChannel leftHand(L"Local\\AmalurVRLeftHandV3",L"Local\\AmalurVRLeftHandMutexV3");
    amalur::PoseChannel rightHand(L"Local\\AmalurVRRightHandV3",L"Local\\AmalurVRRightHandMutexV3");
    StereoSource stereoSource;
    MenuAnchor menuAnchor;
    try {
        uint32_t n{}; XR(xrEnumerateInstanceExtensionProperties(nullptr,0,&n,nullptr));
        std::vector<XrExtensionProperties> ext(n,{XR_TYPE_EXTENSION_PROPERTIES});
        XR(xrEnumerateInstanceExtensionProperties(nullptr,n,&n,ext.data()));
        if(std::none_of(ext.begin(),ext.end(),[](auto& e){return std::strcmp(e.extensionName,XR_KHR_D3D11_ENABLE_EXTENSION_NAME)==0;}))
            throw std::runtime_error("Runtime does not expose XR_KHR_D3D11_enable");
        const char* extension=XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
        XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
        strcpy_s(ci.applicationInfo.applicationName,"AmalurXRSmoke"); ci.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
        ci.enabledExtensionCount=1; ci.enabledExtensionNames=&extension;
        XR(xrCreateInstance(&ci,&instance));
        XrInstanceProperties ip{XR_TYPE_INSTANCE_PROPERTIES}; XR(xrGetInstanceProperties(instance,&ip));
        std::cout<<"Runtime="<<ip.runtimeName<<" version="<<XR_VERSION_MAJOR(ip.runtimeVersion)<<'.'<<XR_VERSION_MINOR(ip.runtimeVersion)<<'.'<<XR_VERSION_PATCH(ip.runtimeVersion)<<"\n";
        XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO}; si.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrSystemId system{}; auto systemResult=xrGetSystem(instance,&si,&system);
        if(systemResult==XR_ERROR_FORM_FACTOR_UNAVAILABLE) { std::cout<<"BLOCKED: runtime available, headset system unavailable. Connect Quest through Virtual Desktop.\n"; return 2; }
        xrcheck(systemResult,"xrGetSystem");
        XrSystemProperties sp{XR_TYPE_SYSTEM_PROPERTIES}; XR(xrGetSystemProperties(instance,system,&sp));
        std::cout<<"System="<<sp.systemName<<" orientation="<<sp.trackingProperties.orientationTracking<<" position="<<sp.trackingProperties.positionTracking<<"\n";
        PFN_xrGetD3D11GraphicsRequirementsKHR requirements{};
        XR(xrGetInstanceProcAddr(instance,"xrGetD3D11GraphicsRequirementsKHR",reinterpret_cast<PFN_xrVoidFunction*>(&requirements)));
        XrGraphicsRequirementsD3D11KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR}; XR(requirements(instance,system,&req));
        ComPtr<IDXGIFactory1> factory; hrcheck(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
        ComPtr<IDXGIAdapter1> adapter;
        for(UINT i=0;;++i) {
            ComPtr<IDXGIAdapter1> a; if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC1 d{}; hrcheck(a->GetDesc1(&d));
            if(d.AdapterLuid.HighPart==req.adapterLuid.HighPart && d.AdapterLuid.LowPart==req.adapterLuid.LowPart) {
                adapter=a; std::wcout<<L"Adapter="<<d.Description<<L"\n"; break;
            }
        }
        if(!adapter) throw std::runtime_error("Required runtime adapter not found");
        const D3D_FEATURE_LEVEL candidates[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};
        std::vector<D3D_FEATURE_LEVEL> levels; for(auto f:candidates) if(f>=req.minFeatureLevel) levels.push_back(f);
        if(levels.empty()) throw std::runtime_error("Required feature level exceeds this test's supported levels");
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context; D3D_FEATURE_LEVEL level{};
        hrcheck(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels.data(),static_cast<UINT>(levels.size()),D3D11_SDK_VERSION,&device,&level,&context));
        std::cout<<"D3D11 feature level="<<std::hex<<level<<std::dec<<"\n";
        if(!live) { std::cout<<"PASS: x86 runtime/system and matching D3D11 device. Session/render/input not tested.\n"; return 0; }
        XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR}; binding.device=device.Get();
        XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO}; sci.next=&binding; sci.systemId=system; XR(xrCreateSession(instance,&sci,&r.session));
        XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO}; spaceInfo.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL; spaceInfo.poseInReferenceSpace.orientation.w=1;
        XR(xrCreateReferenceSpace(r.session,&spaceInfo,&r.local));
        spaceInfo.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;
        XR(xrCreateReferenceSpace(r.session,&spaceInfo,&r.view));
        if(trackingMode&&(!poses.open(true)||!leftHand.open(true)||!rightHand.open(true)||!motionInput.open(true)))throw std::runtime_error("Cannot create tracking channels");
        XrActionSetCreateInfo asi{XR_TYPE_ACTION_SET_CREATE_INFO}; strcpy_s(asi.actionSetName,"smoke"); strcpy_s(asi.localizedActionSetName,"Smoke test"); XR(xrCreateActionSet(instance,&asi,&r.actions));
        std::array<XrPath,2> handPaths{path("/user/hand/left"),path("/user/hand/right")};
        auto action=[&](const char* name,XrActionType type) { XrActionCreateInfo ai{XR_TYPE_ACTION_CREATE_INFO}; ai.actionType=type; ai.countSubactionPaths=2; ai.subactionPaths=handPaths.data(); strcpy_s(ai.actionName,name); strcpy_s(ai.localizedActionName,name); XrAction a{}; XR(xrCreateAction(r.actions,&ai,&a)); return a; };
        XrAction pose=action("grip_pose",XR_ACTION_TYPE_POSE_INPUT),trigger=action("trigger",XR_ACTION_TYPE_FLOAT_INPUT),haptic=action("haptic",XR_ACTION_TYPE_VIBRATION_OUTPUT);
        XrAction move=action("move",XR_ACTION_TYPE_VECTOR2F_INPUT);
        XrAction squeeze=action("squeeze",XR_ACTION_TYPE_FLOAT_INPUT);
        XrAction primary=action("primary_button",XR_ACTION_TYPE_BOOLEAN_INPUT),secondary=action("secondary_button",XR_ACTION_TYPE_BOOLEAN_INPUT);
        XrAction stickClick=action("thumbstick_click",XR_ACTION_TYPE_BOOLEAN_INPUT),menuAction=action("menu",XR_ACTION_TYPE_BOOLEAN_INPUT);
        XrAction thumbrest=action("thumbrest_touch",XR_ACTION_TYPE_BOOLEAN_INPUT);
        std::vector<XrActionSuggestedBinding> bindings;
        bindings.push_back({thumbrest,path("/user/hand/right/input/thumbrest/touch")});
        bindings.push_back({menuAction,path("/user/hand/left/input/menu/click")});
        for(auto side:{"left","right"}) {
            std::string base=std::string("/user/hand/")+side;
            bindings.push_back({move,path((base+"/input/thumbstick").c_str())});
            bindings.push_back({stickClick,path((base+"/input/thumbstick/click").c_str())});
            bindings.push_back({squeeze,path((base+"/input/squeeze/value").c_str())});
            bindings.push_back({primary,path((base+(std::string(side)=="left"?"/input/x/click":"/input/a/click")).c_str())});
            bindings.push_back({secondary,path((base+(std::string(side)=="left"?"/input/y/click":"/input/b/click")).c_str())});
            bindings.push_back({pose,path((base+"/input/grip/pose").c_str())});
            bindings.push_back({trigger,path((base+"/input/trigger/value").c_str())});
            bindings.push_back({haptic,path((base+"/output/haptic").c_str())});
        }
        XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING}; suggested.interactionProfile=path("/interaction_profiles/oculus/touch_controller"); suggested.countSuggestedBindings=static_cast<uint32_t>(bindings.size()); suggested.suggestedBindings=bindings.data(); XR(xrSuggestInteractionProfileBindings(instance,&suggested));
        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO}; attach.countActionSets=1; attach.actionSets=&r.actions; XR(xrAttachSessionActionSets(r.session,&attach));
        for(int i=0;i<2;++i) { XrActionSpaceCreateInfo a{XR_TYPE_ACTION_SPACE_CREATE_INFO}; a.action=pose; a.subactionPath=handPaths[i]; a.poseInActionSpace.orientation.w=1; XR(xrCreateActionSpace(r.session,&a,&r.hands[i])); }
        XR(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&n,nullptr));
        if(n!=2) throw std::runtime_error("Expected exactly two stereo views");
        std::vector<XrViewConfigurationView> configs(n,{XR_TYPE_VIEW_CONFIGURATION_VIEW}); XR(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,n,&n,configs.data()));
        XR(xrEnumerateSwapchainFormats(r.session,0,&n,nullptr)); std::vector<int64_t> formats(n); XR(xrEnumerateSwapchainFormats(r.session,n,&n,formats.data()));
        int64_t format=0; for(auto f:{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM}) if(std::find(formats.begin(),formats.end(),f)!=formats.end()){format=f;break;}
        if(!format) throw std::runtime_error("No supported color format");
        std::array<std::vector<XrSwapchainImageD3D11KHR>,2> images;
        float activeRenderScale=settings.renderScale;
        unsigned outputWidth=0,outputHeight=0;
        auto createEyeChains=[&](){for(int i=0;i<2;++i) {
            images[i].clear();if(r.chains[i]){XR(xrDestroySwapchain(r.chains[i]));r.chains[i]=XR_NULL_HANDLE;}
            XrSwapchainCreateInfo c{XR_TYPE_SWAPCHAIN_CREATE_INFO}; c.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT; c.format=format; c.sampleCount=1;
            c.width=gameMode?std::min(configs[i].maxImageRectWidth,static_cast<unsigned>(configs[i].recommendedImageRectWidth*settings.renderScale)):std::min(1024u,configs[i].recommendedImageRectWidth);
            c.height=gameMode?std::min(configs[i].maxImageRectHeight,static_cast<unsigned>(configs[i].recommendedImageRectHeight*settings.renderScale)):std::min(1024u,configs[i].recommendedImageRectHeight);
            outputWidth=c.width;outputHeight=c.height;
            c.faceCount=1;c.arraySize=1;c.mipCount=1;
            std::cout<<"Eye "<<i<<" output="<<c.width<<"x"<<c.height<<" format="<<format<<"\n";
            XR(xrCreateSwapchain(r.session,&c,&r.chains[i])); XR(xrEnumerateSwapchainImages(r.chains[i],0,&n,nullptr)); images[i].resize(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR}); XR(xrEnumerateSwapchainImages(r.chains[i],n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(images[i].data())));
        }};createEyeChains();
        if(gameMode)panel.initialize(r.session,static_cast<DXGI_FORMAT>(format));
        const char* shader="cbuffer C:register(b0){float4x4 m;} struct O{float4 p:SV_POSITION;float3 c:COLOR;}; O vs(float3 p:POSITION,float3 c:COLOR){O o;o.p=mul(float4(p,1),m);o.c=c;return o;} float4 ps(O i):SV_TARGET{return float4(i.c,1);}";
        ComPtr<ID3DBlob> vs,ps,error; hrcheck(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"vs","vs_4_0",0,0,&vs,&error)); hrcheck(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"ps","ps_4_0",0,0,&ps,&error));
        ComPtr<ID3D11VertexShader> vertex; ComPtr<ID3D11PixelShader> pixel; ComPtr<ID3D11InputLayout> layout;
        hrcheck(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex)); hrcheck(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel));
        D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0}};
        hrcheck(device->CreateInputLayout(elements,2,vs->GetBufferPointer(),vs->GetBufferSize(),&layout));
        // Three separated triangles at different depths to expose real parallax.
        const float vertices[]={-1.2f,-.4f,-3, .2f,.8f,1, -.4f,-.4f,-3,.2f,.8f,1, -.8f,.4f,-3,.2f,.8f,1,
            .4f,-.4f,-3,1,.6f,.1f, 1.2f,-.4f,-3,1,.6f,.1f, .8f,.4f,-3,1,.6f,.1f,
            -.18f,-.18f,-1.2f,.3f,1,.4f, .18f,-.18f,-1.2f,.3f,1,.4f, 0,.18f,-1.2f,.3f,1,.4f};
        D3D11_BUFFER_DESC bd{}; bd.ByteWidth=sizeof(vertices);bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER; D3D11_SUBRESOURCE_DATA initial{vertices}; ComPtr<ID3D11Buffer> vb,cb; hrcheck(device->CreateBuffer(&bd,&initial,&vb));
        bd.ByteWidth=64;bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;hrcheck(device->CreateBuffer(&bd,nullptr,&cb));
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;ComPtr<ID3D11RasterizerState> raster;hrcheck(device->CreateRasterizerState(&rd,&raster));
        bool running=false,focused=false,done=false; unsigned frames=0; std::array<bool,2> pressed{}; const auto start=std::chrono::steady_clock::now();
        if(gameMode&&!stereoSource.initialize(device.Get()))throw std::runtime_error("Stereo presenter initialization failed");
        GameLifetime gameLifetime;
        const int duration=trackingMode?1800:30;
        const int stopKey=trackingMode?VK_F12:VK_ESCAPE;
        std::cout<<"Session test: "<<duration<<" seconds, "<<(trackingMode?"F12":"ESC")<<" exits.\n";
        if(gameMode)std::cout<<"EXPERIMENTAL GAME STEREO: menu panel until F10 camera is active; waiting for geo-11 Katanga surface. Scale/eye convergence uncalibrated.\n";
        else if(trackingMode)std::cout<<"POSE BRIDGE: desktop camera diagnostic only. The headset still shows triangles, not Amalur.\n";
        while(!done && std::chrono::steady_clock::now()-start<std::chrono::seconds(duration) && !(GetAsyncKeyState(stopKey)&0x8000)) {
            if(gameMode&&gameLifetime.gameExited()){
                std::cout<<"Game process exited; closing VR bridge.\n";
                break;
            }
            if(gameMode){settings.poll();DWORD developerPid=GetTickCount()-latestRig.tick<1000?latestRig.pid:0;developer.poll(developerPid);if(settings.developerAction>=0){developer.submit(settings.developerAction,developerPid);settings.developerAction=-1;}if(gripSettings.open(true))gripSettings.publish(settings.gripPitch,settings.gripYaw,settings.gripRoll,settings.weaponX,settings.weaponY,settings.weaponZ);if(hudSettings.open(true))hudSettings.publish(settings.hudSize);if(menuSettings.open(true))menuSettings.publish(amalur::menuScale(settings.interfaceScale)*.8f);if(settings.renderScale!=activeRenderScale){createEyeChains();activeRenderScale=settings.renderScale;}}
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
            for(;;) {
                auto result=xrPollEvent(instance,&event); if(result==XR_EVENT_UNAVAILABLE) break; xrcheck(result,"xrPollEvent");
                if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                    auto state=reinterpret_cast<XrEventDataSessionStateChanged*>(&event)->state;std::cout<<"Session state="<<state<<"\n";focused=state==XR_SESSION_STATE_FOCUSED;
                    if(state==XR_SESSION_STATE_READY){XrSessionBeginInfo b{XR_TYPE_SESSION_BEGIN_INFO};b.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;XR(xrBeginSession(r.session,&b));running=true;}
                    if(state==XR_SESSION_STATE_STOPPING){XR(xrEndSession(r.session));running=false;done=true;}
                    if(state==XR_SESSION_STATE_EXITING||state==XR_SESSION_STATE_LOSS_PENDING) done=true;
                } else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) done=true;
                event={XR_TYPE_EVENT_DATA_BUFFER};
            }
            if(done) break; if(!running){Sleep(10);continue;}
            XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState fs{XR_TYPE_FRAME_STATE};XR(xrWaitFrame(r.session,&wait,&fs));XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};XR(xrBeginFrame(r.session,&begin));
            XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;locate.displayTime=fs.predictedDisplayTime;locate.space=r.local;
            XrViewState state{XR_TYPE_VIEW_STATE};std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};uint32_t count{};XR(xrLocateViews(r.session,&locate,&state,2,&count,views.data()));
            XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};
            if(trackingMode){
                XR(xrLocateSpace(r.view,r.local,fs.predictedDisplayTime,&head));
                amalur::PosePacket packet;packet.tick=GetTickCount64();packet.gameMode=gameMode?(settings.interfaceView?3u:1u):0u;
                packet.depth=settings.depth;packet.convergence=settings.convergence;packet.worldScale=settings.scale;packet.horizontalFov=settings.fov;packet.recenter=settings.recenter;
                constexpr XrSpaceLocationFlags required=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
                packet.valid=focused&&(head.locationFlags&required)==required;
                packet.orientation[0]=head.pose.orientation.x;packet.orientation[1]=head.pose.orientation.y;packet.orientation[2]=head.pose.orientation.z;packet.orientation[3]=head.pose.orientation.w;
                packet.position[0]=head.pose.position.x;packet.position[1]=head.pose.position.y;packet.position[2]=head.pose.position.z;
                poses.publish(packet);
            }
            if(focused) {
                XrActiveActionSet active{r.actions,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;XR(xrSyncActions(r.session,&sync));
                auto scalar=[&](XrAction a,int hand){XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};g.action=a;g.subactionPath=handPaths[hand];
                    XrActionStateFloat v{XR_TYPE_ACTION_STATE_FLOAT};XR(xrGetActionStateFloat(r.session,&g,&v));return v.isActive?v.currentState:0.f;};
                auto button=[&](XrAction a,int hand){XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};g.action=a;g.subactionPath=handPaths[hand];
                    XrActionStateBoolean v{XR_TYPE_ACTION_STATE_BOOLEAN};XR(xrGetActionStateBoolean(r.session,&g,&v));return v.isActive&&v.currentState;};
                amalur::TouchInput touch;
                for(int hand=0;hand<2;++hand){XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};g.action=move;g.subactionPath=handPaths[hand];
                    XrActionStateVector2f v{XR_TYPE_ACTION_STATE_VECTOR2F};XR(xrGetActionStateVector2f(r.session,&g,&v));
                    if(v.isActive){if(hand==0){touch.leftX=v.currentState.x;touch.leftY=v.currentState.y;}else{touch.rightX=v.currentState.x;touch.rightY=v.currentState.y;}}}
                touch.leftTrigger=scalar(trigger,0);touch.rightTrigger=scalar(trigger,1);touch.leftGrip=scalar(squeeze,0);touch.rightGrip=scalar(squeeze,1);
                touch.x=button(primary,0);touch.y=button(secondary,0);touch.a=button(primary,1);touch.b=button(secondary,1);
                touch.leftClick=button(stickClick,0);touch.rightClick=button(stickClick,1);touch.menu=button(menuAction,0);
                touch.rightThumbrest=button(thumbrest,1);
                // Cache across nonblocking mutex misses; expiry still cancels gameplay.
                amalur::RigStatus freshRig;
                if(rigStatus.transfer(freshRig,false)&&freshRig.version==1)latestRig=freshRig;
                const bool gameplay=!settings.interfaceView&&latestRig.pid&&GetTickCount()-latestRig.tick<1000
                    &&latestRig.weaponRemaps>0&&latestRig.paused==0;
                auto mapped=touchMapper.map(touch,gameMode&&!settings.visible&&!settings.developerVisible&&VrSettings::gameFocused(),gameplay);
                settings.selectedWeapon=mapped.selectedWeapon;
                motionInput.publish(mapped);
                for(int i=0;i<2;++i){XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=trigger;get.subactionPath=handPaths[i];XrActionStateFloat value{XR_TYPE_ACTION_STATE_FLOAT};XR(xrGetActionStateFloat(r.session,&get,&value));
                    bool down=value.isActive&&value.currentState>.75f;
                    if(down&&!pressed[i]&&!gameMode){XrHapticActionInfo hi{XR_TYPE_HAPTIC_ACTION_INFO};hi.action=haptic;hi.subactionPath=handPaths[i];XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};vibration.duration=50000000;vibration.amplitude=.25f;vibration.frequency=XR_FREQUENCY_UNSPECIFIED;XR(xrApplyHapticFeedback(r.session,&hi,reinterpret_cast<XrHapticBaseHeader*>(&vibration)));std::cout<<"Trigger/haptic hand="<<i<<"\n";} pressed[i]=down;
                    XrSpaceLocation hand{XR_TYPE_SPACE_LOCATION};XR(xrLocateSpace(r.hands[i],r.local,fs.predictedDisplayTime,&hand));
                    if(trackingMode){
                        XrActionStateGetInfo gripInfo{XR_TYPE_ACTION_STATE_GET_INFO};gripInfo.action=pose;gripInfo.subactionPath=handPaths[i];
                        XrActionStatePose grip{XR_TYPE_ACTION_STATE_POSE};XR(xrGetActionStatePose(r.session,&gripInfo,&grip));
                        amalur::PosePacket packet;packet.tick=GetTickCount64();packet.gameMode=gameMode?(settings.interfaceView?3u:1u):0u;
                        constexpr XrSpaceLocationFlags required=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
                        packet.valid=grip.isActive&&(hand.locationFlags&required)==required;
                        packet.orientation[0]=hand.pose.orientation.x;packet.orientation[1]=hand.pose.orientation.y;packet.orientation[2]=hand.pose.orientation.z;packet.orientation[3]=hand.pose.orientation.w;
                        packet.position[0]=hand.pose.position.x;packet.position[1]=hand.pose.position.y;packet.position[2]=hand.pose.position.z;
                        packet.worldScale=settings.scale;packet.recenter=settings.recenter;
                        (i==0?leftHand:rightHand).publish(packet);
                    }
                    if(frames%90==0)std::cout<<"Hand="<<i<<" flags="<<hand.locationFlags<<" xyz="<<hand.pose.position.x<<','<<hand.pose.position.y<<','<<hand.pose.position.z<<"\n";
                }
            } else {pressed={};motionInput.publish(touchMapper.map({},false));if(trackingMode){amalur::PosePacket invalid;leftHand.publish(invalid);rightHand.publish(invalid);}}
            std::array<XrCompositionLayerProjectionView,2> projectionViews{};
            bool render=fs.shouldRender&&count==2&&(state.viewStateFlags&XR_VIEW_STATE_POSITION_VALID_BIT)&&(state.viewStateFlags&XR_VIEW_STATE_ORIENTATION_VALID_BIT);
            amalur::PosePacket gameFrame;
            bool trackedGame=false;
            if(gameMode){
                bool sourceReady=stereoSource.acquirePaired(device.Get(),context.Get(),gameFrame);
                trackedGame=!settings.interfaceView&&sourceReady&&gameFrame.valid&&gameFrame.projectionX>0&&gameFrame.projectionY>0;
                render=render&&sourceReady;
            }
            if(gameMode&&render){
                if(trackedGame)menuAnchor.close();
                else if((head.locationFlags&(XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT))==(XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT))menuAnchor.update(head.pose,settings.recenter);
                else if(!menuAnchor.active)render=false;
            }
            if(render)for(int i=0;i<2;++i){
                uint32_t index{};XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};XR(xrAcquireSwapchainImage(r.chains[i],&acquire,&index));XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;XR(xrWaitSwapchainImage(r.chains[i],&wi));
                auto texture=images[i][index].texture;D3D11_TEXTURE2D_DESC td{};texture->GetDesc(&td);ComPtr<ID3D11RenderTargetView> target;D3D11_RENDER_TARGET_VIEW_DESC rtv{};rtv.Format=static_cast<DXGI_FORMAT>(format);rtv.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;hrcheck(device->CreateRenderTargetView(texture,&rtv,&target));
                float color[]={.015f,.02f,.03f,1};context->ClearRenderTargetView(target.Get(),color);auto rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);
                D3D11_VIEWPORT viewport{0,0,float(td.Width),float(td.Height),0,1};context->RSSetViewports(1,&viewport);context->RSSetState(raster.Get());
                if(gameMode){
                    auto f=views[i].fov;
                    int sourceEye=settings.interfaceView?0:(settings.swap?1-i:i);
                    float bias=settings.alignment*(settings.depth/20.f);
                    if(trackedGame)stereoSource.draw(context.Get(),sourceEye,std::tan(f.angleLeft),std::tan(f.angleRight),std::tan(f.angleDown),std::tan(f.angleUp),gameFrame.projectionX,gameFrame.projectionY,sourceEye==0?bias:-bias,settings.sharpness);
                    else stereoSource.draw(context.Get(),sourceEye,-1,1,-1,1,1,1,0,settings.sharpness);
                }else{
                auto p=views[i].pose;auto f=views[i].fov;auto world=XMMatrixRotationQuaternion(XMVectorSet(p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w))*XMMatrixTranslation(p.position.x,p.position.y,p.position.z);
                auto view=XMMatrixInverse(nullptr,world);auto proj=XMMatrixPerspectiveOffCenterRH(std::tan(f.angleLeft)*.05f,std::tan(f.angleRight)*.05f,std::tan(f.angleDown)*.05f,std::tan(f.angleUp)*.05f,.05f,100.f);
                XMFLOAT4X4 matrix;XMStoreFloat4x4(&matrix,XMMatrixTranspose(view*proj));context->UpdateSubresource(cb.Get(),0,nullptr,&matrix,0,0);
                UINT stride=24,offset=0;auto v=vb.Get();auto c=cb.Get();context->IASetVertexBuffers(0,1,&v,&stride,&offset);context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(vertex.Get(),nullptr,0);context->VSSetConstantBuffers(0,1,&c);context->PSSetShader(pixel.Get(),nullptr,0);context->Draw(9,0);context->OMSetRenderTargets(0,nullptr,nullptr);
                }
                context->OMSetRenderTargets(0,nullptr,nullptr);
                XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};XR(xrReleaseSwapchainImage(r.chains[i],&release));
                auto& pv=projectionViews[i];pv.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;pv.pose=views[i].pose;pv.fov=views[i].fov;pv.subImage.swapchain=r.chains[i];pv.subImage.imageRect.extent={static_cast<int32_t>(td.Width),static_cast<int32_t>(td.Height)};
                if(gameMode&&trackedGame){
                    // Attribute the image to the pose used by the game camera,
                    // not the newer predicted pose. Preserve eye-to-head offsets.
                    // The producer publishes metadata with a GPU-owned image.
                    XrPosef rendered{{gameFrame.orientation[0],gameFrame.orientation[1],gameFrame.orientation[2],gameFrame.orientation[3]},
                        {gameFrame.position[0],gameFrame.position[1],gameFrame.position[2]}};
                    pv.pose=renderedEyePose(head.pose,views[i].pose,rendered);
                }
            }
            XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=r.local;layer.viewCount=2;layer.views=projectionViews.data();const XrCompositionLayerBaseHeader* layers[]={reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer)};
            std::array<XrCompositionLayerQuad,2> menu{};
            const XrCompositionLayerBaseHeader* menuLayers[2]{};
            if(gameMode&&!trackedGame&&render)for(int i=0;i<2;++i){
                menu[i].type=XR_TYPE_COMPOSITION_LAYER_QUAD;menu[i].space=r.local;
                menu[i].eyeVisibility=i==0?XR_EYE_VISIBILITY_LEFT:XR_EYE_VISIBILITY_RIGHT;
                menu[i].subImage=projectionViews[i].subImage;menu[i].pose=menuAnchor.pose;
                const float interfaceScale=amalur::menuScale(settings.interfaceScale);
                menu[i].size={2.f*interfaceScale,1.125f*interfaceScale};menuLayers[i]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&menu[i]);
            }
            std::vector<const XrCompositionLayerBaseHeader*> submitted;
            if(render){if(gameMode&&!trackedGame){submitted.push_back(menuLayers[0]);submitted.push_back(menuLayers[1]);}else submitted.push_back(layers[0]);}
            XrCompositionLayerQuad settingsLayer{};
            if(gameMode&&(settings.visible||settings.developerVisible)&&fs.shouldRender){
                auto a=views[0].pose.position,b=views[1].pose.position;
                float ipd=count==2?1000.f*std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)):0.f;
                settingsLayer=panel.draw(context.Get(),r.view,settings,ipd,outputWidth,outputHeight,stereoSource.sourceWidth(),stereoSource.sourceHeight(),trackedGame,gameFrame.stereoStatus,&developer);
                submitted.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&settingsLayer));
            }
            XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=fs.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;end.layerCount=static_cast<uint32_t>(submitted.size());end.layers=submitted.data();XR(xrEndFrame(r.session,&end));if(render)++frames;
        }
        std::cout<<"Rendered pairs="<<frames<<"; visual comfort, pose accuracy and haptic perception require human verification.\n";
        return frames?0:2;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}

