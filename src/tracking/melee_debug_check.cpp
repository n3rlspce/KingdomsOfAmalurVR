#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <atomic>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "weapon_pose.hpp"
using Microsoft::WRL::ComPtr;
static void log(const char* text,...){std::printf("%s",text);}
static std::atomic<bool> firstPerson{true},interfaceView{false};
namespace arm_rig {static std::atomic<bool> enabled{true};}
namespace motion_controls {inline bool gameFocused(){return false;}struct Controls {int selectedWeapon{};};inline Controls viewControls(){return {};}}
namespace weapon_control {static SRWLOCK poseLock=SRWLOCK_INIT;static mgs5vr::Pose bladeWorld[2];static uint64_t bladeTick,tick,leftTick;}
#include "../diagnostic/melee_debug.hpp"
static void check(bool b,const char* message){if(!b){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(){
    auto window=CreateWindowExW(0,L"STATIC",L"overlay-check",WS_OVERLAPPEDWINDOW,0,0,256,256,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    DXGI_SWAP_CHAIN_DESC sd{};sd.BufferDesc.Width=256;sd.BufferDesc.Height=256;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=1;sd.OutputWindow=window;sd.Windowed=TRUE;
    ComPtr<ID3D11Device> d;ComPtr<ID3D11DeviceContext> c;ComPtr<IDXGISwapChain> chain;D3D_FEATURE_LEVEL level;
    check(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&chain,&d,&level,&c)),"WARP swapchain");
    ComPtr<ID3D11Texture2D> back;check(SUCCEEDED(chain->GetBuffer(0,IID_PPV_ARGS(&back))),"backbuffer");
    ComPtr<ID3D11RenderTargetView> rt;check(SUCCEEDED(d->CreateRenderTargetView(back.Get(),nullptr,&rt)),"target");float black[4]{};c->ClearRenderTargetView(rt.Get(),black);
    auto r=rt.Get();c->OMSetRenderTargets(1,&r,nullptr);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    weapon_control::bladeWorld[0].position={-12,0,0};weapon_control::bladeWorld[1].position={12,0,0};
    weapon_control::bladeTick=weapon_control::tick=weapon_control::leftTick=GetTickCount64();
    float vp[16]={.025f,0,0,0,0,.025f,0,0,0,0,.01f,0,0,0,.2f,1};
    melee_debug::record(0,0,{-20,0,0},{-12,0,0});melee_debug::draw(chain.Get(),vp,true);
    check(melee_debug::vs&&melee_debug::state,"shader compile and context state supported");
    D3D11_PRIMITIVE_TOPOLOGY topology;c->IAGetPrimitiveTopology(&topology);check(topology==D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,"restore topology");
    ComPtr<ID3D11RenderTargetView> restored;c->OMGetRenderTargets(1,&restored,nullptr);check(restored.Get()==rt.Get(),"restore render target");
    ComPtr<ID3D11VertexShader> shader;c->VSGetShader(&shader,nullptr,nullptr);check(!shader,"restore vertex shader");
    D3D11_TEXTURE2D_DESC td;back->GetDesc(&td);td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> staging;
    check(SUCCEEDED(d->CreateTexture2D(&td,nullptr,&staging)),"readback texture");c->CopyResource(staging.Get(),back.Get());D3D11_MAPPED_SUBRESOURCE map;
    check(SUCCEEDED(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)),"readback");unsigned cyan=0,magenta=0;
    for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x){auto p=static_cast<unsigned char*>(map.pData)+y*map.RowPitch+x*4;if(p[1]>100&&p[2]>100&&p[0]<60)++cyan;if(p[0]>100&&p[2]>100&&p[1]<60)++magenta;}
    c->Unmap(staging.Get(),0);check(cyan>20&&magenta>20,"both hand wireframes visible in rendered pixels");
    restored.Reset();rt.Reset();back.Reset();c->OMSetRenderTargets(0,nullptr,nullptr);
    check(SUCCEEDED(chain->ResizeBuffers(1,128,128,DXGI_FORMAT_UNKNOWN,0)),"overlay does not retain backbuffer after drawing");
    DestroyWindow(window);std::printf("PASS: overlay pixels cyan=%u magenta=%u; pipeline restored; resize succeeds\n",cyan,magenta);
}
