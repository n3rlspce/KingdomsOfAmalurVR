#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define AMALUR_MELEE_DEBUG_MAPPING L"Local\\AmalurMeleeDebugIsolatedRendererCheck"
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <atomic>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "weapon_pose.hpp"
#include "melee_swing_event.hpp"
using Microsoft::WRL::ComPtr;
static void log(const char* text,...){std::printf("%s",text);}
static std::atomic<bool> firstPerson{true},interfaceView{false};
namespace arm_rig {static std::atomic<bool> enabled{true};}
namespace motion_controls {inline bool focused=true;inline bool gameFocused(){return focused;}struct Controls {int selectedWeapon{};};inline Controls viewControls(){return {};}}
namespace weapon_control {static SRWLOCK poseLock=SRWLOCK_INIT;static mgs5vr::Pose visualPoses[2];static uint64_t visualTick,tick,leftTick;static uint32_t visualAsset=1520,visualWeapon=123,visualSelection=0;static bool visualDual=true;inline float longswordCharge{};inline bool longswordReady{};inline unsigned generation{};inline amalur::MeleeSwingEvent swingEvents[2];inline std::atomic<uint32_t> physicalActor{0};}
#include "../diagnostic/melee_debug.hpp"
static void check(bool b,const char* message){if(!b){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    if constexpr(!amalur::meleeDebugAvailable){
        amalur::MeleeDebugSettings settings;
        check(!settings.enabled()&&!settings.toggle(),"quarantine cannot be enabled through panel");
        melee_debug::draw(nullptr,nullptr,true);
        check(!melee_debug::device,"disabled overlay never enters D3D path");
        std::puts("PASS: overlay quarantined; panel cannot enable; no graphics access");return 0;
    }
    amalur::MeleeDebugSettings toggle;check(!toggle.enabled(),"overlay defaults off");check(toggle.toggle(),"explicit enable");
    for(const auto model:{2478u,1250u,1323u,1689u,1520u}){
        weapon_control::visualAsset=model;
        amalur::MeleeDebugSettings panel;
        check(panel.enabled()&&toggle.enabled(),"weapon changes and panel reconnection preserve ON");
    }
    auto window=CreateWindowExW(0,L"STATIC",L"overlay-check",WS_OVERLAPPEDWINDOW,0,0,256,256,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    DXGI_SWAP_CHAIN_DESC sd{};sd.BufferDesc.Width=256;sd.BufferDesc.Height=256;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=1;sd.OutputWindow=window;sd.Windowed=TRUE;
    ComPtr<ID3D11Device> d;ComPtr<ID3D11DeviceContext> c;ComPtr<IDXGISwapChain> chain;D3D_FEATURE_LEVEL level;
    check(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&chain,&d,&level,&c)),"WARP swapchain");
    ComPtr<ID3D11Device> wrapped;check(SUCCEEDED(chain->GetDevice(IID_PPV_ARGS(&wrapped))),"swapchain device");d=wrapped;c.Reset();d->GetImmediateContext(&c);
    ComPtr<ID3D11Texture2D> back;check(SUCCEEDED(chain->GetBuffer(0,IID_PPV_ARGS(&back))),"backbuffer");
    ComPtr<ID3D11RenderTargetView> rt;check(SUCCEEDED(d->CreateRenderTargetView(back.Get(),nullptr,&rt)),"target");float black[4]{};c->ClearRenderTargetView(rt.Get(),black);
    auto r=rt.Get();c->OMSetRenderTargets(1,&r,nullptr);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    weapon_control::visualPoses[0].position={-12,0,0};weapon_control::visualPoses[1].position={12,0,0};
    weapon_control::visualTick=weapon_control::tick=weapon_control::leftTick=GetTickCount64();
    float vp[16]={.025f,0,0,0,0,.025f,0,0,0,0,.01f,0,0,0,.2f,1};
    check(melee_debug::initialize(d.Get()),"initialize overlay resources");
    auto originalShader=melee_debug::vs;
    c->VSSetShader(melee_debug::vs.Get(),nullptr,0);c->PSSetShader(nullptr,nullptr,0);
    melee_debug::record(0,0,{-20,0,0},{-12,0,0});melee_debug::draw(chain.Get(),vp,true);
    check(melee_debug::vs!=nullptr,"shader compile and context state supported");
    D3D11_PRIMITIVE_TOPOLOGY topology;c->IAGetPrimitiveTopology(&topology);check(topology==D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,"restore topology");
    ComPtr<ID3D11RenderTargetView> restored;c->OMGetRenderTargets(1,&restored,nullptr);check(restored.Get()==rt.Get(),"restore render target");
    ComPtr<ID3D11VertexShader> shader;c->VSGetShader(&shader,nullptr,nullptr);check(shader.Get()==originalShader.Get(),"restore non-null vertex shader");
    ComPtr<ID3D11PixelShader> pixelShader;c->PSGetShader(&pixelShader,nullptr,nullptr);check(!pixelShader,"restore null pixel shader through wrapper");
    D3D11_TEXTURE2D_DESC td;back->GetDesc(&td);td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> staging;
    check(SUCCEEDED(d->CreateTexture2D(&td,nullptr,&staging)),"readback texture");c->CopyResource(staging.Get(),back.Get());D3D11_MAPPED_SUBRESOURCE map;
    check(SUCCEEDED(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)),"readback");unsigned cyan=0,magenta=0,edgePixels=0;
    for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x){auto p=static_cast<unsigned char*>(map.pData)+y*map.RowPitch+x*4;if((x<8||x>=248)&&(p[0]>100||p[1]>100||p[2]>100))++edgePixels;if(p[1]>100&&p[2]>100&&p[0]<60)++cyan;if(p[0]>100&&p[2]>100&&p[1]<60)++magenta;}
    c->Unmap(staging.Get(),0);check(edgePixels==0,"in-view rings cannot streak to screen edges");check(cyan>20&&magenta>20,"both hand wireframes visible in rendered pixels");
    vp[10]=.004f;
    for(const auto model:{2478u,1250u,1323u,1689u,1520u}){
        weapon_control::visualAsset=model;
        weapon_control::visualDual=model==1689||model==1520;
        weapon_control::visualTick=weapon_control::tick=weapon_control::leftTick=GetTickCount64();
        melee_debug::draw(chain.Get(),vp,true);
        check(toggle.enabled(),"rendering weapon switches never disables collision toggle");
        c->IAGetPrimitiveTopology(&topology);check(topology==D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,"each profile restores pipeline");
    }
    check(toggle.toggle()&&!toggle.enabled(),"explicit disable");
    weapon_control::visualAsset=2478;weapon_control::visualDual=false;
    weapon_control::longswordCharge=1;weapon_control::longswordReady=true;
    weapon_control::visualTick=weapon_control::tick=GetTickCount64();
    c->ClearRenderTargetView(rt.Get(),black);melee_debug::draw(chain.Get(),vp,true);
    c->CopyResource(staging.Get(),back.Get());check(SUCCEEDED(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)),"charge readback");
    unsigned green=0;for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x){auto p=static_cast<unsigned char*>(map.pData)+y*map.RowPitch+x*4;if(p[1]>150&&p[0]<100&&p[2]<120)++green;}
    c->Unmap(staging.Get(),0);check(green>10,"ready indicator visible with collision debug OFF");
    motion_controls::focused=false;c->ClearRenderTargetView(rt.Get(),black);melee_debug::draw(chain.Get(),vp,true);
    c->CopyResource(staging.Get(),back.Get());check(SUCCEEDED(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)),"cancel readback");
    unsigned lit=0;for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x){auto p=static_cast<unsigned char*>(map.pData)+y*map.RowPitch+x*4;if(p[0]||p[1]||p[2])++lit;}
    c->Unmap(staging.Get(),0);check(!lit,"focus loss hides charge and trail");
    restored.Reset();rt.Reset();back.Reset();c->OMSetRenderTargets(0,nullptr,nullptr);
    check(SUCCEEDED(chain->ResizeBuffers(1,128,128,DXGI_FORMAT_UNKNOWN,0)),"overlay does not retain backbuffer after drawing");
    DestroyWindow(window);std::printf("PASS: overlay pixels cyan=%u magenta=%u; pipeline restored; resize succeeds\n",cyan,magenta);
}
