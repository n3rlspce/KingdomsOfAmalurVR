#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <atomic>
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "pose_channel.hpp"
using Microsoft::WRL::ComPtr;
struct PendingMap {ID3D11DeviceContext* context{};ID3D11Resource* resource{};UINT subresource{};void* data{};};
#include "../diagnostic/render_pose.hpp"

static void check(bool condition,const char* message){
    if(!condition){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
using Matrix=std::array<float,16>;
using Constants=std::array<float,64>;
static Matrix multiply(const Matrix& a,const Matrix& b){
    Matrix result{};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){
        double value=0;for(unsigned k=0;k<4;++k)value+=double(a[r*4+k])*b[k*4+c];
        result[r*4+c]=static_cast<float>(value);
    }
    return result;
}
static Constants constants(const Matrix& vp){
    const Matrix world{0,2,0,0,-3,0,0,0,0,0,.75f,0,16964.85f,-31599.18f,5981.65f,1};
    const auto wvp=multiply(world,vp);Constants result{};
    std::memcpy(result.data(),world.data(),64);std::memcpy(result.data()+32,wvp.data(),64);return result;
}
static void camera(const Matrix& vp,uint64_t tick){
    unsigned char core[0x200]{};std::memcpy(core+0x104,vp.data(),64);
    amalur::PosePacket pose;pose.valid=1;pose.gameMode=1;pose.tick=tick;pose.position[0]=float(tick);
    render_pose::camera(core,pose);
}
static void upload(ID3D11DeviceContext* context,ID3D11Buffer* buffer,const Constants& values){
    D3D11_MAPPED_SUBRESOURCE mapped{};
    check(SUCCEEDED(context->Map(buffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)),"WARP dynamic constant buffer maps");
    render_pose::map(context,buffer,0,mapped.pData);
    std::memcpy(mapped.pData,values.data(),sizeof(values));
    render_pose::unmap(context,buffer,0);
    context->Unmap(buffer,0);
}
static void draw(ID3D11DeviceContext* context,ID3D11Buffer* buffer){
    context->VSSetConstantBuffers(4,1,&buffer);
    render_pose::draw(context);
    // Attribution depends on the verified buffer binding, not shader output.
    // No shaders/targets are needed for this bounded API-state harness.
    context->Draw(3,0);
}
static void expectPresent(uint64_t expected,const char* message){
    Matrix vp{};
    const auto pose=render_pose::beginPresent(vp.data());
    check(vp==render_pose::drawnVP,"overlay receives matched draw matrix");
    check(pose.valid==1&&pose.tick==expected&&pose.position[0]==float(expected),message);
}
int main(){
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"create isolated WARP device");
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=256;desc.Usage=D3D11_USAGE_DYNAMIC;
    desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    ComPtr<ID3D11Buffer> worldBuffer,otherBuffer;
    check(SUCCEEDED(device->CreateBuffer(&desc,nullptr,&worldBuffer))&&SUCCEEDED(device->CreateBuffer(&desc,nullptr,&otherBuffer)),"create real dynamic draw buffers");
    const Matrix older{1.3f,0,0,0,0,1.8f,0,0,0,0,1.001f,1,0,0,-.1f,0};
    auto newer=older;newer[0]=1.6f;newer[8]=.3f;newer[12]=4;
    camera(older,100);camera(newer,200);
    const auto oldConstants=constants(older),newConstants=constants(newer);

    upload(context.Get(),worldBuffer.Get(),oldConstants);
    check(render_pose::upload.serial==1,"first upload captured once");
    draw(context.Get(),worldBuffer.Get());
    check(render_pose::acceptedEpoch==render_pose::epoch.load(),"actual older draw accepted despite newer simulation camera");
    const auto serialAfterAcceptance=render_pose::upload.serial;
    const auto descriptorsAfterAcceptance=render_pose::descriptorCalls.load();
    for(unsigned i=0;i<1000;++i)upload(context.Get(),otherBuffer.Get(),newConstants);
    check(render_pose::upload.serial==serialAfterAcceptance,"post-acceptance uploads do not read mapped GPU data");
    check(render_pose::descriptorCalls.load()==descriptorsAfterAcceptance,"post-acceptance uploads skip descriptor queries too");
    expectPresent(100,"Present attributes the camera actually drawn, not latest simulated pose");

    draw(context.Get(),worldBuffer.Get());
    check(render_pose::acceptedEpoch==render_pose::epoch.load(),"unchanged constant buffer rematches in next epoch");
    check(render_pose::upload.serial==serialAfterAcceptance,"unchanged buffer reuse needs no extra upload read");
    upload(context.Get(),worldBuffer.Get(),newConstants);
    check(!render_pose::upload.resource,"rewrite after acceptance invalidates cached buffer");
    check(render_pose::upload.serial==serialAfterAcceptance,"rewrite after acceptance still avoids mapped GPU read");
    expectPresent(100,"post-draw rewrite cannot relabel already drawn image");

    draw(context.Get(),worldBuffer.Get());
    check(!render_pose::beginPresent().valid,"next epoch cannot reuse pre-rewrite cached contents");
    upload(context.Get(),worldBuffer.Get(),newConstants);draw(context.Get(),worldBuffer.Get());
    check(render_pose::upload.serial==serialAfterAcceptance+1,"fresh next-frame upload resumes bounded capture");
    expectPresent(200,"fresh upload restores correct newer camera attribution");
    // A candidate uploaded but not bound at the verified b4 slot is insufficient.
    upload(context.Get(),worldBuffer.Get(),oldConstants);draw(context.Get(),otherBuffer.Get());
    Matrix absent;absent.fill(1);
    check(!render_pose::beginPresent(absent.data()).valid,"unbound upload never labels a draw");
    check(absent==Matrix{},"missing draw clears overlay projection");
    context->ClearState();context->Flush();
    std::puts("PASS: WARP draw pose attribution, cached reuse, post-acceptance invalidation, fresh recovery and 1000 skipped GPU reads");
}
