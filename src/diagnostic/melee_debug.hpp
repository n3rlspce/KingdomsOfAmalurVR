#pragma once
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include "../tracking/melee_debug_settings.hpp"
namespace melee_debug {
inline constexpr float radius=4.f;
inline mgs5vr::Vec3 offset(unsigned sample){return {0,0,float(sample)*10.f};}
struct Sweep {mgs5vr::Vec3 from{},to{};uint64_t tick{};};
inline SRWLOCK lock=SRWLOCK_INIT;
inline Sweep sweeps[2][4]{};
inline void record(unsigned hand,unsigned sample,mgs5vr::Vec3 from,mgs5vr::Vec3 to){
    AcquireSRWLockExclusive(&lock);sweeps[hand][sample]={from,to,GetTickCount64()};ReleaseSRWLockExclusive(&lock);
}
struct Vertex {float x,y,z,r,g,b,a;};
inline ComPtr<ID3D11Device> device;
inline ComPtr<ID3DDeviceContextState> state;
inline ComPtr<ID3D11VertexShader> vs;
inline ComPtr<ID3D11PixelShader> ps;
inline ComPtr<ID3D11InputLayout> layout;
inline ComPtr<ID3D11Buffer> vertices,constants;
inline ComPtr<ID3D11DepthStencilState> depth;
inline ComPtr<ID3D11RasterizerState> raster;
inline bool initialize(ID3D11Device* d){
    if(device.Get()==d)return state&&vs&&ps&&layout&&vertices&&constants&&depth&&raster;
    device=d;state.Reset();vs.Reset();ps.Reset();layout.Reset();vertices.Reset();constants.Reset();depth.Reset();raster.Reset();
    ComPtr<ID3D11Device1> d1;if(FAILED(d->QueryInterface(IID_PPV_ARGS(&d1))))return false;
    auto level=d->GetFeatureLevel();D3D_FEATURE_LEVEL chosen;
    if(FAILED(d1->CreateDeviceContextState(0,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),&chosen,&state)))return false;
    const char* shader=R"(
cbuffer Camera : register(b0) {row_major float4x4 vp;};
Texture2D<float4> StereoParams : register(t125);
struct V {float3 p:POSITION;float4 c:COLOR;};
struct O {float4 p:SV_POSITION;float4 c:COLOR;};
O VS(V v){O o;o.p=mul(float4(v.p,1),vp);float4 stereo=StereoParams.Load(int3(0,0,0));o.p.x+=stereo.x*(o.p.w-stereo.y);o.c=v.c;return o;}
float4 PS(O v):SV_TARGET{return v.c;}
)";
    ComPtr<ID3DBlob> v,p,error;
    if(FAILED(D3DCompile(shader,strlen(shader),"melee-debug",nullptr,nullptr,"VS","vs_5_0",0,0,&v,&error))||
       FAILED(D3DCompile(shader,strlen(shader),"melee-debug",nullptr,nullptr,"PS","ps_5_0",0,0,&p,&error)))return false;
    if(FAILED(d->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs))||
       FAILED(d->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps)))return false;
    D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(FAILED(d->CreateInputLayout(elements,2,v->GetBufferPointer(),v->GetBufferSize(),&layout)))return false;
    D3D11_BUFFER_DESC b{};b.ByteWidth=sizeof(Vertex)*4096;b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_VERTEX_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(FAILED(d->CreateBuffer(&b,nullptr,&vertices)))return false;
    b.ByteWidth=64;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;if(FAILED(d->CreateBuffer(&b,nullptr,&constants)))return false;
    D3D11_DEPTH_STENCIL_DESC z{};z.DepthEnable=FALSE;z.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;z.DepthFunc=D3D11_COMPARISON_ALWAYS;
    if(FAILED(d->CreateDepthStencilState(&z,&depth)))return false;
    D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;
    if(FAILED(d->CreateRasterizerState(&r,&raster)))return false;
    log("Melee collision overlay initialized: cyan right, magenta left; toggle in F11 developer panel\n");return true;
}
inline void draw(IDXGISwapChain* chain,const float* vp,bool valid){
    static amalur::MeleeDebugSettings settings;
    const bool enabled=settings.enabled();
    if(!enabled||!valid||!firstPerson.load()||!arm_rig::enabled.load()||interfaceView.load())return;
    mgs5vr::Pose poses[2];uint64_t frame,ticks[2];Sweep recent[2][4];
    AcquireSRWLockShared(&weapon_control::poseLock);
    poses[0]=weapon_control::bladeWorld[0];poses[1]=weapon_control::bladeWorld[1];frame=weapon_control::bladeTick;
    ticks[0]=weapon_control::tick;ticks[1]=weapon_control::leftTick;
    ReleaseSRWLockShared(&weapon_control::poseLock);
    auto now=GetTickCount64();if(!frame||frame>now||now-frame>=100||motion_controls::viewControls().selectedWeapon!=0)return;
    AcquireSRWLockShared(&lock);memcpy(recent,sweeps,sizeof(recent));ReleaseSRWLockShared(&lock);
    std::array<Vertex,4096> data{};unsigned count=0;
    auto line=[&](mgs5vr::Vec3 a,mgs5vr::Vec3 b,unsigned hand,float intensity){
        if(count+2>data.size())return;
        float red=hand?intensity:0.1f,green=hand?0.15f:intensity;
        data[count++]={a.x,a.y,a.z,red,green,intensity,1};data[count++]={b.x,b.y,b.z,red,green,intensity,1};
    };
    for(unsigned h=0;h<2;++h){if(!ticks[h]||ticks[h]>now||now-ticks[h]>=100||!mgs5vr::valid(poses[h]))continue;
        for(unsigned s=0;s<4;++s){auto center=mgs5vr::compose(poses[h],mgs5vr::Pose{{},offset(s)}).position;
            for(unsigned plane=0;plane<3;++plane)for(unsigned n=0;n<24;++n){
                auto point=[&](unsigned j){float a=float(j)*6.28318530718f/24,c=radius*cosf(a),v=radius*sinf(a);
                    return center+mgs5vr::Vec3{plane==0?0:c,plane==1?0:(plane==0?c:v),plane==2?0:v};};
                line(point(n),point(n+1),h,0.7f);
            }
            auto& sweep=recent[h][s];if(sweep.tick&&now>=sweep.tick&&now-sweep.tick<250)line(sweep.from,sweep.to,h,1.f);
        }
    }
    if(!count)return;
    ComPtr<ID3D11Device> d;if(FAILED(chain->GetDevice(IID_PPV_ARGS(&d))))return;
    if(!initialize(d.Get())){static bool warned=false;if(!warned){log("Melee overlay unavailable: D3D11.1 state or resources rejected\n");warned=true;}return;}
    ComPtr<ID3D11DeviceContext> context;d->GetImmediateContext(&context);
    ComPtr<ID3D11DeviceContext1> c;if(FAILED(context.As(&c)))return;
    ComPtr<ID3D11Texture2D> back;if(FAILED(chain->GetBuffer(0,IID_PPV_ARGS(&back))))return;
    ComPtr<ID3D11RenderTargetView> target;if(FAILED(d->CreateRenderTargetView(back.Get(),nullptr,&target)))return;
    D3D11_TEXTURE2D_DESC desc{};back->GetDesc(&desc);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(c->Map(vertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    memcpy(mapped.pData,data.data(),count*sizeof(Vertex));c->Unmap(vertices.Get(),0);
    if(FAILED(c->Map(constants.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return;
    memcpy(mapped.pData,vp,64);c->Unmap(constants.Get(),0);
    // Swap a complete pipeline state, including shader classes, UAVs and predication.
    // Retain geo-11's stereo parameters for the same per-eye correction as world shaders.
    ComPtr<ID3D11ShaderResourceView> stereo;c->VSGetShaderResources(125,1,&stereo);
    ComPtr<ID3DDeviceContextState> previous;c->SwapDeviceContextState(state.Get(),&previous);
    auto srv=stereo.Get();c->VSSetShaderResources(125,1,&srv);
    auto rt=target.Get();c->OMSetRenderTargets(1,&rt,nullptr);c->OMSetDepthStencilState(depth.Get(),0);
    c->RSSetState(raster.Get());D3D11_VIEWPORT viewport{0,0,float(desc.Width),float(desc.Height),0,1};c->RSSetViewports(1,&viewport);
    c->IASetInputLayout(layout.Get());c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    auto vb=vertices.Get();UINT stride=sizeof(Vertex),start=0;c->IASetVertexBuffers(0,1,&vb,&stride,&start);
    c->VSSetShader(vs.Get(),nullptr,0);c->PSSetShader(ps.Get(),nullptr,0);auto cb=constants.Get();c->VSSetConstantBuffers(0,1,&cb);
    c->Draw(count,0);
    // Do not retain the swapchain buffer in our saved debug state across ResizeBuffers.
    c->OMSetRenderTargets(0,nullptr,nullptr);ID3D11ShaderResourceView* empty=nullptr;c->VSSetShaderResources(125,1,&empty);
    c->SwapDeviceContextState(previous.Get(),nullptr);
}
}
