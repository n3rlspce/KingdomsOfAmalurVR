#define NOMINMAX
#include "../xr_smoke/stereo_source.hpp"
#include <string>
#include <unordered_map>
#include <cmath>
using Microsoft::WRL::ComPtr;
static void check(bool ok,const char* what){if(!ok){printf("FAIL: %s\n",what);exit(1);}}
int main(int argc,char** argv){
    setvbuf(stdout,nullptr,_IONBF,0);
    ComPtr<ID3D11Device> producer,consumer;ComPtr<ID3D11DeviceContext> pc,cc;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&producer,nullptr,&pc)),"producer device");
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&consumer,nullptr,&cc)),"consumer device");
    bool cameraProbe=argc==2&&strcmp(argv[1],"--camera")==0;
    if(argc==2&&(strcmp(argv[1],"--live")==0||cameraProbe)){
        StereoSource source;check(source.initialize(consumer.Get()),"shaders");
        amalur::PoseChannel poses;std::unordered_map<uint64_t,amalur::PosePacket> history;
        if(cameraProbe)check(poses.open(true),"headless camera pose writer");
        uint64_t last=0;unsigned count=0,tracked=0;auto start=GetTickCount64();
        while(GetTickCount64()-start<(cameraProbe?10000u:5000u)){
            if(cameraProbe){amalur::PosePacket p;p.valid=1;p.gameMode=1;p.tick=GetTickCount64();
                float angle=.12f*std::sin(static_cast<float>(p.tick-start)*.002f);
                p.orientation[1]=std::sin(angle*.5f);p.orientation[3]=std::cos(angle*.5f);
                poses.publish(p);history[p.tick]=p;
            }
            amalur::PosePacket pose;uint64_t seq{};
            if(source.acquirePaired(consumer.Get(),cc.Get(),pose,&seq)&&seq!=last){last=seq;++count;if(pose.valid){++tracked;
                if(cameraProbe){auto found=history.find(pose.tick);check(found!=history.end(),"rendered pose belongs to transmitted history");
                    check(memcmp(found->second.orientation,pose.orientation,sizeof(pose.orientation))==0,"pose stays exact through game render and GPU handoff");}
            }}
            Sleep(1);
        }
        printf("Live paired frames=%u tracked=%u source=%ux%u per eye\n",count,tracked,source.sourceWidth(),source.sourceHeight());
        check(count>10,"live producer advances without headset");
        if(cameraProbe)check(tracked>10,"tracked camera frames verified without headset");
        check(source.capture(consumer.Get(),cc.Get(),"captures/paired-live.bmp"),"paired live capture");return 0;
    }
    std::wstring name=L"Local\\AmalurPairedTest"+std::to_wstring(GetCurrentProcessId()),mutex=name+L"Mutex";
    amalur::StereoPublisher publisher(name.c_str(),mutex.c_str());
    StereoSource source(L"Unused",name.c_str(),mutex.c_str());check(source.initialize(consumer.Get()),"shaders");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=8;desc.Height=4;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> input;check(SUCCEEDED(producer->CreateTexture2D(&desc,nullptr,&input)),"input");
    desc.Width=4;desc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;ComPtr<ID3D11Texture2D> output;check(SUCCEEDED(consumer->CreateTexture2D(&desc,nullptr,&output)),"output");
    D3D11_RENDER_TARGET_VIEW_DESC rv{};rv.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;rv.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11RenderTargetView> rtv;check(SUCCEEDED(consumer->CreateRenderTargetView(output.Get(),&rv,&rtv)),"rtv");
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> read;check(SUCCEEDED(consumer->CreateTexture2D(&desc,nullptr,&read)),"staging");
    UINT pixels[32];uint64_t last{};
    for(unsigned i=1;i<=240;++i){
        for(auto& pixel:pixels)pixel=i%2?0xff0000ff:0xff00ff00;
        pc->UpdateSubresource(input.Get(),0,nullptr,pixels,32,0);
        amalur::PosePacket pose;pose.valid=1;pose.tick=GetTickCount64();pose.position[0]=static_cast<float>(i);
        auto start=GetTickCount64();while(!publisher.publish(producer.Get(),pc.Get(),input.Get(),pose)&&GetTickCount64()-start<1000)Sleep(1);
        check(GetTickCount64()-start<1000,"producer progresses");
        pose.position[0]=-1;check(!publisher.publish(producer.Get(),pc.Get(),input.Get(),pose),"unconsumed image cannot be overwritten");
        amalur::PosePacket got;uint64_t seq=last;start=GetTickCount64();
        while((!source.acquirePaired(consumer.Get(),cc.Get(),got,&seq)||seq==last)&&GetTickCount64()-start<1000)Sleep(1);
        check(seq>last&&got.position[0]==i,"image sequence retains original pose");last=seq;
        D3D11_VIEWPORT vp{0,0,4,4,0,1};cc->RSSetViewports(1,&vp);auto rt=rtv.Get();cc->OMSetRenderTargets(1,&rt,nullptr);
        source.draw(cc.Get(),0,-1,1,-1,1,1,1);cc->OMSetRenderTargets(0,nullptr,nullptr);cc->CopyResource(read.Get(),output.Get());
        D3D11_MAPPED_SUBRESOURCE m{};check(SUCCEEDED(cc->Map(read.Get(),0,D3D11_MAP_READ,0,&m)),"readback");
        auto p=static_cast<unsigned char*>(m.pData)+m.RowPitch*2+8;
        check(i%2?(p[0]>250&&p[1]<5):(p[1]>250&&p[0]<5),"GPU pixel matches paired pose");cc->Unmap(read.Get(),0);
    }
    Sleep(1010);amalur::PosePacket pose;check(!source.acquirePaired(consumer.Get(),cc.Get(),pose),"stopped producer expires");
    pose.valid=0;pose.tick=GetTickCount64();check(publisher.publish(producer.Get(),pc.Get(),input.Get(),pose),"producer recovers after consumer pause");
    auto start=GetTickCount64();uint64_t seq=last;
    while((!source.acquirePaired(consumer.Get(),cc.Get(),pose,&seq)||seq==last)&&GetTickCount64()-start<1000)Sleep(1);
    check(seq>last&&!pose.valid,"new generation retains tracking loss");
    desc.Width=16;desc.Height=4;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;desc.CPUAccessFlags=0;
    input.Reset();check(SUCCEEDED(producer->CreateTexture2D(&desc,nullptr,&input)),"resized input");
    check(publisher.publish(producer.Get(),pc.Get(),input.Get(),pose),"resize publishes new generation");
    start=GetTickCount64();while(source.sourceWidth()!=8&&GetTickCount64()-start<1000){source.acquirePaired(consumer.Get(),cc.Get(),pose);Sleep(1);}
    check(source.sourceWidth()==8,"consumer follows source resize");
    puts("PASS: 240 GPU image/pose pairs, overwrite exclusion, expiry, recovery, invalid tracking and resize");
}
