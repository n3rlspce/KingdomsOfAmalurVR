#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#define NOMINMAX
#include <windows.h>
#include "source_resolution_control.hpp"
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
#include "../bridge_tracking/tracking_snapshot.hpp"
#include "../bridge_tracking/pose_channel.hpp"
#include "../bridge_tracking/menu_view.hpp"
#include "../bridge_tracking/cinematic_resize.hpp"
#include "../bridge_tracking/motion_input.hpp"
#include "../bridge_tracking/heavy_charge_frame.hpp"
#include "../bridge_tracking/charged_feedback_channel.hpp"
#include "../bridge_tracking/impact_feedback_channel.hpp"
#include "../bridge_tracking/action_feedback_channel.hpp"
#include "../bridge_tracking/physical_crouch.hpp"
#include "../bridge_tracking/snap_pitch.hpp"
#include "../bridge_tracking/rig_status.hpp"
#include "../bridge_tracking/grip_settings.hpp"
#include "stereo_source.hpp"
#include "render_pose.hpp"
#include "menu_anchor.hpp"
#include "../bridge_tracking/menu_image_policy.hpp"
#include "game_lifetime.hpp"
#include "game_focus.hpp"
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
#include "cinematic_hint_panel.hpp"
#include "../bridge_tracking/cinematic_hint.hpp"
#include "map_panel.hpp"
#include "wrist_hud.hpp"
#include "../bridge_tracking/wrist_regions.hpp"
#include "../bridge_tracking/map_panel_settings.hpp"
#include "../bridge_tracking/hud_settings.hpp"
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
    if(argc==2&&std::string(argv[1])=="--wrist-hud-check"){
        WristHud hud;amalur::PosePacket hand;hand.valid=1;hand.orientation[2]=.5f;hand.orientation[3]=.8660254f;hand.position[1]=1.25f;hand.position[2]=-.35f;
        XrPosef head{{0,0,0,1},{0,1.6f,0}};bool good=true;
        for(uint64_t tick=1000;tick<=1300;tick+=20){hand.tick=tick;hud.update(hand,head,true,tick);}
        good=good&&hud.alpha>.99f;
        // Regression: this lowered position used to remain latched at alpha=1.
        hand.position[1]=1.0f;
        for(uint64_t tick=1320;tick<=1700;tick+=20){hand.tick=tick;hud.update(hand,head,true,tick);}
        good=good&&hud.alpha==0;
        hand.position[1]=1.25f;
        for(uint64_t tick=1720;tick<=2020;tick+=20){hand.tick=tick;hud.update(hand,head,true,tick);}
        good=good&&hud.alpha>.99f;
        hud.lastTick=1300;hud.hideSince=0;
        hand.orientation[2]=0;hand.orientation[3]=1;
        auto p=WristHud::pose(hand,.13f,.045f);
        good=good&&std::abs(p.position.y-1.295f)<.0001f&&std::abs(p.position.z+.22f)<.0001f;
        auto normal=XMVector3Rotate(XMVectorSet(0,0,1,0),XMVectorSet(p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w));
        good=good&&XMVectorGetY(normal)>.999f;
        const auto before=WristHud::pose(hand,.13f,.045f);head.orientation={0,.7071068f,0,.7071068f};
        const auto after=WristHud::pose(hand,.13f,.045f);good=good&&before.position.z==after.position.z&&before.orientation.w==after.orientation.w;
        hand.position[1]=.7f;
        for(uint64_t tick=1320;tick<=1620;tick+=20){hand.tick=tick;hud.update(hand,head,true,tick);}
        good=good&&hud.alpha==0&&!WristHud::valid(hand,1800);
        hand.tick=1800;hand.valid=0;good=good&&!WristHud::valid(hand,1800);
        hand.valid=1;hand.orientation[3]=0;good=good&&!WristHud::valid(hand,1800);
        hand.orientation[3]=1;hand.position[1]=1.25f;hud.alpha=1;good=good&&hud.update(hand,head,false,1800)==0;
        std::cout<<(good?"PASS":"FAIL")<<": wrist pose, facing/height fade, head-turn independence, tracking expiry and focus reset\n";return good?0:1;
    }
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
    if(argc==2&&std::string(argv[1])=="--map-panel-check"){
        const auto name=L"Local\\AmalurMapPanelTest"+std::to_wstring(GetCurrentProcessId()),mutex=name+L"Mutex";
        amalur::MapPanelSettings writer(name.c_str(),mutex.c_str()),reader(name.c_str(),mutex.c_str());
        bool good=!reader.read();writer.publish(true);good=good&&reader.read();writer.publish(false);good=good&&!reader.read();
        VrSettings settings;settings.developerVisible=true;settings.developerRow=VrSettings::mapPanelRow;good=good&&settings.mapPanelPrototype;
        settings.held[VK_RETURN]=true;settings.poll();good=good&&!settings.mapPanelPrototype;
        settings.held[VK_RETURN]=false;settings.poll();settings.held[VK_RETURN]=true;settings.poll();good=good&&settings.mapPanelPrototype;
        std::cout<<(good?"PASS":"FAIL")<<": map prototype IPC and developer toggle on/off\n";return good?0:1;
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
    CinematicHintPanel cinematicHintPanel;
    amalur::CinematicHint cinematicHint;
    DeveloperTools developer;
    if(gameMode)settings.captureInput();
    amalur::PoseChannel poses;
    amalur::TrackingSnapshotChannel trackingSnapshots;
    uint64_t trackingSequence=0;
    LARGE_INTEGER sessionCounter{};QueryPerformanceCounter(&sessionCounter);
    const uint64_t trackingSession=static_cast<uint64_t>(sessionCounter.QuadPart)^ (uint64_t(GetCurrentProcessId())<<32);
    amalur::MotionInputChannel motionInput;
    amalur::HeavyChargeChannel heavyInput;
    amalur::ChargedFeedbackChannel chargedFeedbackChannel;
    amalur::ChargedFeedbackPacket chargedFeedbackPacket;
    amalur::ChargedFeedbackReceiver chargedFeedback;
    amalur::ImpactFeedbackChannel impactChannel;amalur::ImpactFeedbackPacket impactPacket;
    amalur::ImpactFeedbackReceiver impactFeedback;
    amalur::ActionFeedbackChannel actionChannel;
    amalur::ActionFeedbackPacket actionPacket;
    amalur::ActionFeedbackReceiver actionFeedback;
    uint64_t actionPulseUntil[2]{};uint64_t chargedPulseUntil{};
    amalur::TouchMapper touchMapper;
    amalur::PhysicalCrouch physicalCrouch;
    amalur::SnapPitchChannel snapPitch; snapPitch.publish(0);
    amalur::RigStatusChannel rigStatus;amalur::RigStatus latestRig;
    amalur::PoseChannel leftHand(L"Local\\AmalurVRLeftHandV3",L"Local\\AmalurVRLeftHandMutexV3");
    amalur::PoseChannel rightHand(L"Local\\AmalurVRRightHandV3",L"Local\\AmalurVRRightHandMutexV3");
    StereoSource stereoSource;
    MenuAnchor menuAnchor;
    WristHud wristHud;MapPanel wristPanel;SourceResolutionControl sourceResolutionControl;
    StereoSource wristSource{L"Local\\UnusedWristLegacy",L"Local\\AmalurWristFrameV1",L"Local\\AmalurWristFrameMutexV1",true};
    amalur::MapPanelSettings wristSettings{L"Local\\AmalurWristHudV1",L"Local\\AmalurWristHudMutexV1"};
    MapPanel mapPanel;StereoSource uiSource{L"Local\\UnusedUiLegacy",L"Local\\AmalurUiFrameV1",L"Local\\AmalurUiFrameMutexV1",true};
    amalur::MapPanelSettings mapPanelSettings{L"Local\\AmalurUiLayerV2",L"Local\\AmalurUiLayerMutexV2"};
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
        if(trackingMode&&(!trackingSnapshots.open(true)||!poses.open(true)||!leftHand.open(true)||!rightHand.open(true)||!motionInput.open(true)))throw std::runtime_error("Cannot create tracking channels");
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
        if(gameMode){panel.initialize(r.session,static_cast<DXGI_FORMAT>(format));cinematicHintPanel.initialize(r.session,static_cast<DXGI_FORMAT>(format));}
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
        XrSessionState sessionState=XR_SESSION_STATE_UNKNOWN; uint64_t lastHealthTick{}; unsigned lastHealthBits=~0u;
        bool running=false,focused=false,done=false; unsigned frames=0; std::array<bool,2> pressed{}; const auto start=std::chrono::steady_clock::now();
        if(gameMode&&!stereoSource.initialize(device.Get()))throw std::runtime_error("Stereo presenter initialization failed");
        if(gameMode&&!uiSource.initialize(device.Get()))throw std::runtime_error("UI presenter initialization failed");
        if(gameMode&&!wristSource.initialize(device.Get()))throw std::runtime_error("Wrist presenter initialization failed");
        GameLifetime gameLifetime;
        GameFocusRestorer gameFocus;
        amalur::CinematicResize cinematicResize;
        // Observation only: sample raw stick before any UI/cinematic mutation,
        // then the exact mapped packet handed to the game. No per-frame flush.
        uint64_t movementLogTick{},movementLastMotion{};unsigned movementLogMode=~0u;
        auto movementTrace=[&](uint64_t now,float rawX,float rawY,bool rawAvailable,
            const amalur::MotionInputPacket& mapped,bool gameplay,bool panelCapture,bool resizeScreen){
            if(!gameMode)return;
            const auto rigAge=DWORD(GetTickCount()-latestRig.tick);
            const bool gameFocusNow=VrSettings::gameFocused();
            const bool moving=std::fabs(rawX)>.2f||std::fabs(rawY)>.2f||mapped.moveX!=0||mapped.moveY!=0;
            if(moving)movementLastMotion=now;
            const unsigned mode=unsigned(focused)|(unsigned(gameFocusNow)<<1)|(unsigned(mapped.active!=0)<<2)
                |(unsigned(gameplay)<<3)|(unsigned(panelCapture)<<4)|(unsigned(resizeScreen)<<5)
                |(unsigned(settings.panelOpen())<<6)|(unsigned(settings.interfaceView)<<7)
                |(unsigned(rigAge<1000&&latestRig.pid!=0)<<8)|(unsigned(latestRig.weaponRemaps!=0)<<9)
                |(unsigned(latestRig.paused==0)<<10)|(unsigned(rawAvailable)<<11)
                |((unsigned(latestRig.paused+1)&3u)<<12)|(unsigned(latestRig.focused!=0)<<14)|(unsigned(latestRig.tracked!=0)<<15);
            const bool tail=movementLastMotion&&now>=movementLastMotion&&now-movementLastMotion<=500;
            if((!moving&&!tail&&mode==movementLogMode)||(movementLogTick&&now-movementLogTick<50))return;
            movementLogTick=now;movementLogMode=mode;
            std::cout<<"VR movement bridge tick="<<now<<" raw="<<rawX<<','<<rawY<<" rawAvailable="<<rawAvailable
                <<" mapped="<<mapped.moveX<<','<<mapped.moveY<<" active="<<mapped.active<<" gameplay="<<gameplay
                <<" panelCapture="<<panelCapture<<" panel="<<settings.panelOpen()<<" resize="<<resizeScreen
                <<" interface="<<settings.interfaceView<<" rigAge="<<rigAge<<" paused="<<latestRig.paused
                <<" weaponRemaps="<<latestRig.weaponRemaps<<" pid="<<latestRig.pid<<" xrFocus="<<focused
                <<" gameFocus="<<gameFocusNow<<" rigFocus="<<latestRig.focused<<" rigTracked="<<latestRig.tracked
                <<" mode="<<mode<<" buttons="<<mapped.buttons<<"\n";
        };
        const int duration=trackingMode?1800:30;
        const int stopKey=trackingMode?VK_F12:VK_ESCAPE;
        if(gameMode)std::cout<<"Gameplay session: no time limit, F12 exits.\n";
        else std::cout<<"Session test: "<<duration<<" seconds, "<<(trackingMode?"F12":"ESC")<<" exits.\n";
        if(gameMode)std::cout<<"EXPERIMENTAL GAME STEREO: menu panel until F10 camera is active; waiting for geo-11 Katanga surface. Scale/eye convergence uncalibrated.\n";
        else if(trackingMode)std::cout<<"POSE BRIDGE: desktop camera diagnostic only. The headset still shows triangles, not Amalur.\n";
        // Gameplay lasts until explicit exit, runtime shutdown, or game process exit.
        while(!done && (gameMode || std::chrono::steady_clock::now()-start<std::chrono::seconds(duration)) && !(GetAsyncKeyState(stopKey)&0x8000)) {
            if(gameMode&&gameLifetime.gameExited()){
                std::cout<<"Game process exited; closing VR bridge.\n";
                break;
            }
            if(gameMode){settings.poll();mapPanelSettings.publish(true);DWORD developerPid=GetTickCount()-latestRig.tick<1000?latestRig.pid:0;developer.poll(developerPid,developerPid&&latestRig.paused==0&&latestRig.weaponRemaps>0);if(settings.menuRecovery.poll(developerPid)&&settings.menuRecovery.failed){settings.visible=true;settings.developerVisible=false;}if(settings.pauseNativeRequested){settings.pauseNativeRequested=false;settings.menuRecovery.submit(developerPid);if(settings.menuRecovery.failed){settings.visible=true;settings.developerVisible=false;}}if(settings.developerAction>=0){developer.submit(settings.developerAction,developerPid);settings.developerAction=-1;}if(gripSettings.open(true))gripSettings.publish(settings.gripPitch,settings.gripYaw,settings.gripRoll,settings.weaponX,settings.weaponY,settings.weaponZ);if(hudSettings.open(true))hudSettings.publish(settings.hudSize);if(menuSettings.open(true))menuSettings.publish(amalur::menuScale(settings.interfaceScale)*.8f);if(settings.renderScale!=activeRenderScale){createEyeChains();activeRenderScale=settings.renderScale;}}
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
            for(;;) {
                auto result=xrPollEvent(instance,&event); if(result==XR_EVENT_UNAVAILABLE) break; xrcheck(result,"xrPollEvent");
                if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                    auto state=reinterpret_cast<XrEventDataSessionStateChanged*>(&event)->state;sessionState=state;std::cout<<"Session state="<<state<<" tick="<<GetTickCount64()<<std::endl;focused=state==XR_SESSION_STATE_FOCUSED;
                    if(gameMode)gameFocus.onSessionFocus(focused);
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
            amalur::TrackingSnapshot snapshot;
            snapshot.sequence=++trackingSequence;snapshot.session=trackingSession;
            snapshot.predictedTime=fs.predictedDisplayTime;
            const uint64_t acquisitionTick=GetTickCount64();
            XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};
            if(trackingMode){
                XR(xrLocateSpace(r.view,r.local,fs.predictedDisplayTime,&head));
                amalur::PosePacket packet;packet.tick=acquisitionTick;packet.gameMode=gameMode?(settings.interfaceView?3u:1u):0u;
                packet.depth=settings.depth;packet.convergence=settings.convergence;packet.worldScale=settings.scale;packet.horizontalFov=settings.fov;packet.recenter=settings.recenter;
                constexpr XrSpaceLocationFlags required=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
                packet.valid=focused&&(head.locationFlags&required)==required;
                packet.orientation[0]=head.pose.orientation.x;packet.orientation[1]=head.pose.orientation.y;packet.orientation[2]=head.pose.orientation.z;packet.orientation[3]=head.pose.orientation.w;
                packet.position[0]=head.pose.position.x;packet.position[1]=head.pose.position.y;packet.position[2]=head.pose.position.z;
                snapshot.head=packet;poses.publish(packet);
            }
            bool render=fs.shouldRender&&count==2&&(state.viewStateFlags&XR_VIEW_STATE_POSITION_VALID_BIT)&&(state.viewStateFlags&XR_VIEW_STATE_ORIENTATION_VALID_BIT);
            amalur::PosePacket gameFrame;
            bool trackedGame=false;bool mapPanelFrame=false;bool isolatedMenuFrame=false;bool sourceReady=false;
            if(gameMode){
                sourceReady=stereoSource.acquirePaired(device.Get(),context.Get(),gameFrame);
                trackedGame=!settings.interfaceView&&sourceReady&&gameFrame.valid&&gameFrame.projectionX>0&&gameFrame.projectionY>0;
                render=render&&sourceReady;
                if(sourceReady)sourceResolutionControl.update(settings.sourceResolutionPercent,latestRig.pid,stereoSource.sourceWidth(),stereoSource.sourceHeight(),GetTickCount64());
                // The world source no longer contains captured UI. Never place
                // the native framebuffer on the overlay or cache old HUD behind it.
                amalur::PosePacket uiFrame;
                isolatedMenuFrame=sourceReady&&gameFrame.gameMode==7&&!settings.interfaceView;
                mapPanelFrame=isolatedMenuFrame
                    &&uiSource.acquirePaired(device.Get(),context.Get(),uiFrame)
                    &&amalur::menuImageVisible(isolatedMenuFrame,true,uiFrame.gameMode,uiFrame.tick,GetTickCount64());
            }
            if(focused) {
                XrActiveActionSet active{r.actions,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;XR(xrSyncActions(r.session,&sync));
                auto scalar=[&](XrAction a,int hand){XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};g.action=a;g.subactionPath=handPaths[hand];
                    XrActionStateFloat v{XR_TYPE_ACTION_STATE_FLOAT};XR(xrGetActionStateFloat(r.session,&g,&v));return v.isActive?v.currentState:0.f;};
                auto button=[&](XrAction a,int hand){XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};g.action=a;g.subactionPath=handPaths[hand];
                    XrActionStateBoolean v{XR_TYPE_ACTION_STATE_BOOLEAN};XR(xrGetActionStateBoolean(r.session,&g,&v));return v.isActive&&v.currentState;};
                amalur::TouchInput touch;bool rawLeftAvailable=false;
                for(int hand=0;hand<2;++hand){XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};g.action=move;g.subactionPath=handPaths[hand];
                    XrActionStateVector2f v{XR_TYPE_ACTION_STATE_VECTOR2F};XR(xrGetActionStateVector2f(r.session,&g,&v));
                    if(hand==0)rawLeftAvailable=v.isActive!=0;
                    if(v.isActive){if(hand==0){touch.leftX=v.currentState.x;touch.leftY=v.currentState.y;}else{touch.rightX=v.currentState.x;touch.rightY=v.currentState.y;}}}
                const float rawLeftX=touch.leftX,rawLeftY=touch.leftY;
                touch.leftTrigger=scalar(trigger,0);touch.rightTrigger=scalar(trigger,1);touch.leftGrip=scalar(squeeze,0);touch.rightGrip=scalar(squeeze,1);
                touch.x=button(primary,0);touch.y=button(secondary,0);touch.a=button(primary,1);touch.b=button(secondary,1);
                touch.leftClick=button(stickClick,0);touch.rightClick=button(stickClick,1);touch.menu=button(menuAction,0);
                touch.rightThumbrest=button(thumbrest,1);
                // Cache across nonblocking mutex misses; expiry still cancels gameplay.
                amalur::RigStatus freshRig;
                if(rigStatus.transfer(freshRig,false)&&freshRig.version==2)latestRig=freshRig;
                if(gameMode){
                    gameFocus.onMenuSample(touch.menu,latestRig.pid);
                    const auto focusResult=gameFocus.poll(latestRig.pid);
                    if(focusResult==GameFocusRestorer::Result::Restored)std::cout<<"Game focus restored after returning to VR.\n";
                    else if(focusResult==GameFocusRestorer::Result::TimedOut)std::cout<<"Game focus restoration timed out.\n";
                }
                const bool gameplay=!settings.interfaceView&&latestRig.pid&&GetTickCount()-latestRig.tick<1000
                    &&latestRig.weaponRemaps>0&&latestRig.paused==0&&!latestRig.dialogueActive;
                const bool resizeContext=gameMode&&render&&sourceReady&&gameFrame.gameMode==6
                    &&!trackedGame&&!mapPanelFrame&&!settings.panelOpen()&&VrSettings::gameFocused();
                const bool resizeScreen=amalur::CinematicResize::ownsInput(resizeContext,touch.leftGrip,touch.rightGrip);
                if(cinematicResize.update(settings.cinematicScale,touch.leftY,touch.rightY,resizeScreen,acquisitionTick))settings.saveCinematicScale();
                if(resizeScreen){touch.leftY=0;touch.rightY=0;touch.leftX=0;touch.rightX=0;touch.leftGrip=0;touch.rightGrip=0;}
                bool panelCapture=settings.pollDeveloperControllers(touch,gameMode&&VrSettings::gameFocused(),developer.busy());
                auto mapped=touchMapper.map(touch,gameMode&&!panelCapture&&!settings.visible&&!settings.developerVisible&&VrSettings::gameFocused(),gameplay,GetTickCount64(),settings.interfaceView);
                panelCapture=settings.applyPauseInput(touch,gameMode&&VrSettings::gameFocused(),mapped)||panelCapture;
                settings.selectedWeapon=mapped.selectedWeapon;
                const bool crouchActive=gameplay&&mapped.active&&!panelCapture&&!settings.panelOpen()
                    &&!touch.menu&&!(mapped.buttons&XINPUT_GAMEPAD_START)&&snapshot.head.gameMode==1;
                if(physicalCrouch.update(snapshot.head.position[1],snapshot.head.valid!=0,crouchActive,
                    settings.physicalCrouch&&!settings.seatedMode,settings.recenter,latestRig.pid,
                    (mapped.buttons&XINPUT_GAMEPAD_RIGHT_SHOULDER)!=0,acquisitionTick))
                    mapped.buttons|=XINPUT_GAMEPAD_RIGHT_SHOULDER;
                snapPitch.publish(touchMapper.pitchSteps());
                movementTrace(acquisitionTick,rawLeftX,rawLeftY,rawLeftAvailable,mapped,gameplay,panelCapture,resizeScreen);
                motionInput.publish(mapped);
                for(int i=0;i<2;++i){XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=trigger;get.subactionPath=handPaths[i];XrActionStateFloat value{XR_TYPE_ACTION_STATE_FLOAT};XR(xrGetActionStateFloat(r.session,&get,&value));
                    bool down=value.isActive&&value.currentState>.75f;
                    if(down&&!pressed[i]&&!gameMode){XrHapticActionInfo hi{XR_TYPE_HAPTIC_ACTION_INFO};hi.action=haptic;hi.subactionPath=handPaths[i];XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};vibration.duration=50000000;vibration.amplitude=.25f;vibration.frequency=XR_FREQUENCY_UNSPECIFIED;XR(xrApplyHapticFeedback(r.session,&hi,reinterpret_cast<XrHapticBaseHeader*>(&vibration)));std::cout<<"Trigger/haptic hand="<<i<<"\n";} pressed[i]=down;
                    XrSpaceLocation hand{XR_TYPE_SPACE_LOCATION};XR(xrLocateSpace(r.hands[i],r.local,fs.predictedDisplayTime,&hand));
                    if(trackingMode){
                        XrActionStateGetInfo gripInfo{XR_TYPE_ACTION_STATE_GET_INFO};gripInfo.action=pose;gripInfo.subactionPath=handPaths[i];
                        XrActionStatePose grip{XR_TYPE_ACTION_STATE_POSE};XR(xrGetActionStatePose(r.session,&gripInfo,&grip));
                        amalur::PosePacket packet;packet.tick=acquisitionTick;packet.gameMode=gameMode?(settings.interfaceView?3u:1u):0u;
                        constexpr XrSpaceLocationFlags required=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
                        packet.valid=!panelCapture&&!settings.developerVisible&&grip.isActive&&(hand.locationFlags&required)==required;
                        packet.orientation[0]=hand.pose.orientation.x;packet.orientation[1]=hand.pose.orientation.y;packet.orientation[2]=hand.pose.orientation.z;packet.orientation[3]=hand.pose.orientation.w;
                        packet.position[0]=hand.pose.position.x;packet.position[1]=hand.pose.position.y;packet.position[2]=hand.pose.position.z;
                        packet.worldScale=settings.scale;packet.recenter=settings.recenter;
                        (i==0?snapshot.left:snapshot.right)=packet;
                        (i==0?leftHand:rightHand).publish(packet);
                    }
                    if(frames%90==0)std::cout<<"Hand="<<i<<" flags="<<hand.locationFlags<<" xyz="<<hand.pose.position.x<<','<<hand.pose.position.y<<','<<hand.pose.position.z<<"\n";
                }
                // Publish only after BOTH current-frame hand poses have been acquired.
                auto heavy=amalur::heavyChargeFrame(settings.heavyChargeMode,GetCurrentProcessId(),
                    snapshot,touch,mapped,gameplay,panelCapture||settings.panelOpen());
                heavyInput.transfer(heavy,true);
                amalur::ChargedFeedbackPacket freshFeedback;
                if(chargedFeedbackChannel.transfer(freshFeedback,false))chargedFeedbackPacket=freshFeedback;
                const bool feedbackAllowed=heavy.active&&!heavy.spell&&VrSettings::gameFocused()
                    &&latestRig.firstPerson&&latestRig.focused&&latestRig.tracked&&!amalur::playMode.normal();
                const auto pulse=chargedFeedback.sample(chargedFeedbackPacket,GetTickCount64(),feedbackAllowed,
                    latestRig.pid,GetCurrentProcessId(),settings.recenter);
                if(pulse.kind){chargedPulseUntil=GetTickCount64()+pulse.milliseconds;
                    XrHapticActionInfo hi{XR_TYPE_HAPTIC_ACTION_INFO};hi.action=haptic;hi.subactionPath=handPaths[1];
                    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};vibration.duration=static_cast<XrDuration>(pulse.milliseconds)*1000000;
                    vibration.amplitude=pulse.amplitude;vibration.frequency=XR_FREQUENCY_UNSPECIFIED;
                    const auto applied=xrApplyHapticFeedback(r.session,&hi,reinterpret_cast<XrHapticBaseHeader*>(&vibration));
                    std::cout<<"Charged haptic kind="<<pulse.kind<<" durationMs="<<pulse.milliseconds<<" result="<<applied<<std::endl;
                }
                amalur::ImpactFeedbackPacket freshImpact;
                if(impactChannel.transfer(freshImpact,false))impactPacket=freshImpact;
                const auto impactNow=GetTickCount64();
                const auto impacts=impactFeedback.sample(impactPacket,impactNow,feedbackAllowed,
                    latestRig.pid,GetCurrentProcessId(),settings.recenter);
                amalur::ActionFeedbackPacket freshAction;
                if(actionChannel.transfer(freshAction,false))actionPacket=freshAction;
                const bool actionAllowed=gameplay&&!panelCapture&&!settings.panelOpen()&&VrSettings::gameFocused()
                    &&latestRig.firstPerson&&latestRig.focused&&latestRig.tracked&&!amalur::playMode.normal();
                const auto actions=actionFeedback.sample(actionPacket,impactNow,actionAllowed,
                    latestRig.pid,GetCurrentProcessId(),settings.recenter);
                for(unsigned side=0;side<2;++side){
                    const auto& action=actions.hand[side];
                    const amalur::ImpactPulse hit=action.milliseconds
                        ?amalur::ImpactPulse{action.milliseconds,action.amplitude}:impacts.hand[side];
                    const auto& trackedHand=side==0?snapshot.right:snapshot.left;
                    if(!hit.milliseconds||!trackedHand.valid||trackedHand.tick>impactNow||impactNow-trackedHand.tick>=150
                       ||(side==0&&impactNow<chargedPulseUntil)||(!action.milliseconds&&impactNow<actionPulseUntil[side]))continue;
                    if(action.milliseconds)actionPulseUntil[side]=impactNow+action.milliseconds;
                    // Game hand0 is right; OpenXR subaction index1 is right.
                    XrHapticActionInfo hi{XR_TYPE_HAPTIC_ACTION_INFO};hi.action=haptic;hi.subactionPath=handPaths[side==0?1:0];
                    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};vibration.duration=XrDuration(hit.milliseconds)*1000000;
                    vibration.amplitude=hit.amplitude;vibration.frequency=XR_FREQUENCY_UNSPECIFIED;
                    xrApplyHapticFeedback(r.session,&hi,reinterpret_cast<XrHapticBaseHeader*>(&vibration));
                }
            } else {if(cinematicResize.update(settings.cinematicScale,0,0,false,acquisitionTick))settings.saveCinematicScale();chargedFeedback.reset();impactFeedback.reset();actionFeedback.reset();actionPulseUntil[0]=actionPulseUntil[1]=0;pressed={};settings.pollDeveloperControllers({},false,developer.busy());const auto inactiveMapped=touchMapper.map({},false);movementTrace(acquisitionTick,0,0,false,inactiveMapped,false,false,false);motionInput.publish(inactiveMapped);if(trackingMode){amalur::PosePacket invalid;leftHand.publish(invalid);rightHand.publish(invalid);}}
            std::array<XrCompositionLayerProjectionView,2> projectionViews{};
            if(trackingMode)trackingSnapshots.publish(snapshot);
            const auto wristTick=GetTickCount64();
            const auto& wristHand=settings.wristHud==2?snapshot.right:snapshot.left;
            const bool wristActive=gameMode&&render&&focused&&settings.wristHud&&!settings.interfaceView
                &&!settings.panelOpen()&&trackedGame&&!isolatedMenuFrame&&gameFrame.gameMode!=6
                &&WristHud::valid(wristHand,wristTick);
            wristSettings.publish(wristActive);
            const float wristOpacity=wristHud.update(wristHand,head.pose,wristActive,wristTick,settings.wristHud==2);
            amalur::PosePacket wristFrame;
            const bool wristReady=wristActive&&gameFrame.gameMode==8
                &&wristSource.acquirePaired(device.Get(),context.Get(),wristFrame)
                &&wristFrame.gameMode==8&&wristFrame.tick<=wristTick&&wristTick-wristFrame.tick<=1000;
            static uint64_t lastWristLog=0;
            if(gameMode&&wristTick-lastWristLog>=3000){lastWristLog=wristTick;
                std::cout<<"Wrist HUD mode="<<gameFrame.gameMode<<" active="<<wristActive
                    <<" atlas="<<wristReady<<" opacity="<<wristOpacity<<" handValid="<<wristHand.valid
                    <<" focused="<<focused<<" tracked="<<trackedGame<<std::endl;
            }


            if(gameMode&&render){
                if(trackedGame&&!isolatedMenuFrame)menuAnchor.close();
                else if((head.locationFlags&(XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT))==(XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT))menuAnchor.update(head.pose,settings.recenter);
                else if(!menuAnchor.active)render=false;
            }
            // Bounded diagnostics distinguish runtime focus/tracking loss from a
            // missing game frame or flat-view fallback. Do not change input/pose policy.
            const unsigned healthBits=(focused?1u:0u)|(snapshot.head.valid?2u:0u)
                |(sourceReady?4u:0u)|(trackedGame?8u:0u)|(render?16u:0u)
                |(fs.shouldRender?32u:0u)|(settings.interfaceView?64u:0u)
                |(settings.developerVisible?128u:0u)|(settings.visible?256u:0u);
            const auto healthTick=GetTickCount64();
            if(healthTick-lastHealthTick>=5000 || (healthBits!=lastHealthBits&&healthTick-lastHealthTick>=250)){
                std::cout<<"VR health tick="<<healthTick<<" session="<<sessionState
                    <<" focused="<<focused<<" headFlags="<<head.locationFlags<<" headValid="<<snapshot.head.valid
                    <<" viewFlags="<<state.viewStateFlags<<" viewCount="<<count<<" shouldRender="<<fs.shouldRender
                    <<" sourceReady="<<sourceReady<<" trackedGame="<<trackedGame<<" render="<<render
                    <<" frameValid="<<gameFrame.valid<<" frameTick="<<gameFrame.tick
                    <<" frameAge="<<(gameFrame.tick&&healthTick>=gameFrame.tick?healthTick-gameFrame.tick:0)
                    <<" interface="<<settings.interfaceView<<" settings="<<settings.visible
                    <<" developer="<<settings.developerVisible<<std::endl;
                lastHealthTick=healthTick;lastHealthBits=healthBits;
            }
            if(render)for(int i=0;i<2;++i){
                uint32_t index{};XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};XR(xrAcquireSwapchainImage(r.chains[i],&acquire,&index));XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=XR_INFINITE_DURATION;XR(xrWaitSwapchainImage(r.chains[i],&wi));
                auto texture=images[i][index].texture;D3D11_TEXTURE2D_DESC td{};texture->GetDesc(&td);ComPtr<ID3D11RenderTargetView> target;D3D11_RENDER_TARGET_VIEW_DESC rtv{};rtv.Format=static_cast<DXGI_FORMAT>(format);rtv.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;hrcheck(device->CreateRenderTargetView(texture,&rtv,&target));
                // A menu without a rendered world uses black, not the diagnostic gray-blue clear.
                float color[]={0,0,0,1};context->ClearRenderTargetView(target.Get(),color);auto rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);
                D3D11_VIEWPORT viewport{0,0,float(td.Width),float(td.Height),0,1};context->RSSetViewports(1,&viewport);context->RSSetState(raster.Get());
                if(gameMode){
                    auto f=views[i].fov;
                    int sourceEye=settings.interfaceView?0:(settings.swap?1-i:i);
                    float bias=settings.alignment*(settings.depth/20.f);
                    if(trackedGame)stereoSource.draw(context.Get(),sourceEye,std::tan(f.angleLeft),std::tan(f.angleRight),std::tan(f.angleDown),std::tan(f.angleUp),gameFrame.projectionX,gameFrame.projectionY,sourceEye==0?bias:-bias,settings.sharpness);
                    else if(!isolatedMenuFrame)stereoSource.draw(context.Get(),sourceEye,-1,1,-1,1,1,1,0,settings.sharpness);
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
                    const auto& attributed=gameFrame;
                    XrPosef rendered{{attributed.orientation[0],attributed.orientation[1],attributed.orientation[2],attributed.orientation[3]},
                        {attributed.position[0],attributed.position[1],attributed.position[2]}};
                    pv.pose=renderedEyePose(head.pose,views[i].pose,rendered);
                }
            }
            XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=r.local;layer.viewCount=2;layer.views=projectionViews.data();const XrCompositionLayerBaseHeader* layers[]={reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer)};
            std::array<XrCompositionLayerQuad,2> menu{};
            const XrCompositionLayerBaseHeader* menuLayers[2]{};
            if(gameMode&&!trackedGame&&!isolatedMenuFrame&&render)for(int i=0;i<2;++i){
                menu[i].type=XR_TYPE_COMPOSITION_LAYER_QUAD;menu[i].space=r.local;
                menu[i].eyeVisibility=i==0?XR_EYE_VISIBILITY_LEFT:XR_EYE_VISIBILITY_RIGHT;
                menu[i].subImage=projectionViews[i].subImage;menu[i].pose=menuAnchor.pose;
                const float interfaceScale=amalur::menuScale(settings.interfaceScale)*(sourceReady&&gameFrame.gameMode==6?settings.cinematicScale:1.f);
                menu[i].size={2.f*interfaceScale,1.125f*interfaceScale};menuLayers[i]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&menu[i]);
            }
            XrCompositionLayerQuad mapLayer{};
            if(render&&mapPanelFrame)mapLayer=mapPanel.draw(r.session,r.local,static_cast<DXGI_FORMAT>(format),device.Get(),context.Get(),uiSource,menuAnchor.pose,amalur::menuScale(settings.interfaceScale),settings.sharpness);
            std::vector<const XrCompositionLayerBaseHeader*> submitted;
            if(render){if(isolatedMenuFrame){submitted.push_back(layers[0]);if(mapPanelFrame)submitted.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&mapLayer));}else if(gameMode&&!trackedGame){submitted.push_back(menuLayers[0]);submitted.push_back(menuLayers[1]);}else submitted.push_back(layers[0]);}
            std::array<XrCompositionLayerQuad,2> wristLayers{};
            if(render&&wristReady&&wristOpacity>0){
                const float scale=settings.wristHudScale*settings.hudSize/.8f;
                auto atlas=wristPanel.draw(r.session,r.local,static_cast<DXGI_FORMAT>(format),device.Get(),context.Get(),wristSource,
                    WristHud::pose(wristHand,.13f,.045f),1.f,0.f,wristOpacity);
                const int w=atlas.subImage.imageRect.extent.width,h=atlas.subImage.imageRect.extent.height;
                const int ch=int(h*amalur::WristRegions::topHeight);
                for(int i=0;i<2;++i){
                    auto& layer=wristLayers[i];layer=atlas;
                    const int left=i?int(w*amalur::WristRegions::rightStart):0;
                    const int cw=i?w-left:int(w*amalur::WristRegions::leftWidth);
                    layer.subImage.imageRect.offset={left,0};layer.subImage.imageRect.extent={cw,ch};
                    layer.pose=WristHud::pose(wristHand,i?-.13f:.13f,i?.085f:.045f);
                    // Preserve original pixel size and aspect, expanding to fit the complete art.
                    const float width=(i?.40f:.30f)*scale*(float(cw)/w)/.30f;
                    layer.size={width,width*ch/cw};
                    submitted.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer));
                }
            }
            XrCompositionLayerQuad hintLayer{};
            const bool cinematicScreen=gameMode&&sourceReady&&gameFrame.gameMode==6&&!trackedGame&&!mapPanelFrame;
            const float hintAlpha=cinematicHint.update(cinematicScreen,render&&focused&&!settings.panelOpen(),GetTickCount64());
            if(hintAlpha>0){
                hintLayer=cinematicHintPanel.draw(context.Get(),r.local,menuAnchor.pose,menu[0].size.height,hintAlpha);
                submitted.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&hintLayer));
            }
            XrCompositionLayerQuad settingsLayer{};
            if(gameMode&&(settings.visible||settings.developerVisible)&&fs.shouldRender){
                auto a=views[0].pose.position,b=views[1].pose.position;
                float ipd=count==2?1000.f*std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)):0.f;
                settingsLayer=panel.draw(context.Get(),r.view,settings,ipd,outputWidth,outputHeight,stereoSource.sourceWidth(),stereoSource.sourceHeight(),trackedGame,gameFrame.stereoStatus,&developer);
                submitted.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&settingsLayer));
            }
            XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=fs.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;end.layerCount=static_cast<uint32_t>(submitted.size());end.layers=submitted.data();XR(xrEndFrame(r.session,&end));if(render)++frames;
        }
        if(cinematicResize.update(settings.cinematicScale,0,0,false,GetTickCount64()))settings.saveCinematicScale();
        std::cout<<"VR loop exit tick="<<GetTickCount64()<<" state="<<sessionState<<" done="<<done<<" stopKey="<<bool(GetAsyncKeyState(stopKey)&0x8000)<<std::endl;
        std::cout<<"Rendered pairs="<<frames<<"; visual comfort, pose accuracy and haptic perception require human verification.\n";
        return frames?0:2;
    }catch(const std::exception& e){std::cerr<<"FAIL tick="<<GetTickCount64()<<": "<<e.what()<<"\n";return 1;}
}
