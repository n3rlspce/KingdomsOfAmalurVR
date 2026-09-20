#define NOMINMAX
#include "../xr_smoke/stereo_source.hpp"
#include <string>
#include <unordered_map>
#include <set>
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
        amalur::PosePacket got;uint64_t seq=last;start=GetTickCount64();
        while((!source.acquirePaired(consumer.Get(),cc.Get(),got,&seq)||seq==last)&&GetTickCount64()-start<1000)Sleep(1);
        check(seq>last&&got.position[0]==i,"image sequence retains original pose");last=seq;
        D3D11_VIEWPORT vp{0,0,4,4,0,1};cc->RSSetViewports(1,&vp);auto rt=rtv.Get();cc->OMSetRenderTargets(1,&rt,nullptr);
        source.draw(cc.Get(),0,-1,1,-1,1,1,1);cc->OMSetRenderTargets(0,nullptr,nullptr);cc->CopyResource(read.Get(),output.Get());
        D3D11_MAPPED_SUBRESOURCE m{};check(SUCCEEDED(cc->Map(read.Get(),0,D3D11_MAP_READ,0,&m)),"readback");
        auto p=static_cast<unsigned char*>(m.pData)+m.RowPitch*2+8;
        check(i%2?(p[0]>250&&p[1]<5):(p[1]>250&&p[0]<5),"GPU pixel matches paired pose");cc->Unmap(read.Get(),0);
    }
    // Deliberately different eye colours expose cross-eye bilinear/sharpen taps.
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<8;++x)pixels[y*8+x]=x<4?0xff202040:0xff804020;
    pc->UpdateSubresource(input.Get(),0,nullptr,pixels,32,0);
    amalur::PosePacket edgePose;edgePose.valid=1;edgePose.tick=GetTickCount64();
    auto edgeStart=GetTickCount64();while(!publisher.publish(producer.Get(),pc.Get(),input.Get(),edgePose)&&GetTickCount64()-edgeStart<1000)Sleep(1);
    amalur::PosePacket edgeGot;uint64_t edgeSeq=last;edgeStart=GetTickCount64();
    while((!source.acquirePaired(consumer.Get(),cc.Get(),edgeGot,&edgeSeq)||edgeSeq==last)&&GetTickCount64()-edgeStart<1000)Sleep(1);
    check(edgeSeq>last,"edge fixture acquired");last=edgeSeq;
    for(int eye:{0,1})for(float sharpness:{0.f,.6f}){
        D3D11_VIEWPORT vp{0,0,4,4,0,1};cc->RSSetViewports(1,&vp);auto rt=rtv.Get();cc->OMSetRenderTargets(1,&rt,nullptr);
        source.draw(cc.Get(),eye,eye?-1.25f:-.75f,eye?.75f:1.25f,-1,1,1,1,0,sharpness);
        cc->OMSetRenderTargets(0,nullptr,nullptr);cc->CopyResource(read.Get(),output.Get());
        D3D11_MAPPED_SUBRESOURCE m{};check(SUCCEEDED(cc->Map(read.Get(),0,D3D11_MAP_READ,0,&m)),"eye edge readback");
        auto p=static_cast<unsigned char*>(m.pData)+m.RowPitch*2+(eye?0:12);
        const unsigned char expected[2][3]{{64,32,32},{32,64,128}};
        for(unsigned c=0;c<3;++c)check(std::abs(int(p[c])-int(expected[eye][c]))<=1,"bilinear and sharpen taps cannot cross eye seam");
        cc->Unmap(read.Get(),0);
    }
    amalur::StereoMailbox inspect(name.c_str(),mutex.c_str());check(inspect.open(false),"test mailbox");
    std::set<ULONG_PTR> handles;uint64_t poolGeneration{};
    auto publishTag=[&](unsigned tag){
        for(auto& pixel:pixels)pixel=0xff000000u|(tag&0x00ffffffu);
        pc->UpdateSubresource(input.Get(),0,nullptr,pixels,32,0);
        amalur::PosePacket p;p.valid=1;p.tick=GetTickCount64();p.position[0]=static_cast<float>(tag);
        auto start=GetTickCount64();bool published=false;
        while(!(published=publisher.publish(producer.Get(),pc.Get(),input.Get(),p))&&GetTickCount64()-start<1000)Sleep(1);
        check(published,"fast producer progresses with slow consumer");
        amalur::MailboxLock lock(inspect);check(lock.frame!=nullptr,"inspect publication");
        handles.insert(lock.frame->texture);
        if(!poolGeneration)poolGeneration=lock.frame->generation;
        check(poolGeneration==lock.frame->generation,"bounded pool reuses one generation under contention");
    };
    auto consumeTag=[&](unsigned tag){
        amalur::PosePacket got;uint64_t seq=last;auto start=GetTickCount64();
        while((!source.acquirePaired(consumer.Get(),cc.Get(),got,&seq)||seq==last)&&GetTickCount64()-start<1000)Sleep(1);
        check(seq>last&&got.position[0]==tag,"slow consumer gets newest complete pose");last=seq;
        D3D11_VIEWPORT vp{0,0,4,4,0,1};cc->RSSetViewports(1,&vp);auto rt=rtv.Get();cc->OMSetRenderTargets(1,&rt,nullptr);
        source.draw(cc.Get(),0,-1,1,-1,1,1,1);cc->OMSetRenderTargets(0,nullptr,nullptr);cc->CopyResource(read.Get(),output.Get());
        D3D11_MAPPED_SUBRESOURCE m{};check(SUCCEEDED(cc->Map(read.Get(),0,D3D11_MAP_READ,0,&m)),"slow consumer readback");
        auto p=static_cast<unsigned char*>(m.pData)+m.RowPitch*2+8;
        check(p[0]==(tag&255)&&p[1]==((tag>>8)&255)&&p[2]==((tag>>16)&255),"newest GPU pixel matches newest pose");cc->Unmap(read.Get(),0);
    };
    unsigned tag=240;
    for(unsigned burst=0;burst<24;++burst){for(unsigned n=0;n<8;++n)publishTag(++tag);consumeTag(tag);}
    check(handles.size()<=3,"fast producer uses at most three shared textures");
    // Hold a GPU lease after releasing the metadata lock. Publication can use
    // other slots, but must never reclaim this slot until key 0 is returned.
    publishTag(++tag);const auto leasedTag=tag;
    ComPtr<ID3D11Texture2D> leased;ComPtr<IDXGIKeyedMutex> lease;
    {amalur::MailboxLock lock(inspect);check(lock.frame!=nullptr,"lease metadata lock");
        check(SUCCEEDED(consumer->OpenSharedResource(reinterpret_cast<HANDLE>(lock.frame->texture),IID_PPV_ARGS(&leased)))&&SUCCEEDED(leased.As(&lease)),"open held lease");
        check(lease->AcquireSync(1,1000)==S_OK,"acquire held GPU lease");}
    for(unsigned n=0;n<12;++n)publishTag(++tag);
    D3D11_TEXTURE2D_DESC heldDesc{};leased->GetDesc(&heldDesc);heldDesc.Usage=D3D11_USAGE_STAGING;heldDesc.BindFlags=0;heldDesc.MiscFlags=0;heldDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> heldRead;check(SUCCEEDED(consumer->CreateTexture2D(&heldDesc,nullptr,&heldRead)),"held lease staging");
    cc->CopyResource(heldRead.Get(),leased.Get());D3D11_MAPPED_SUBRESOURCE heldPixels{};
    check(SUCCEEDED(cc->Map(heldRead.Get(),0,D3D11_MAP_READ,0,&heldPixels)),"held lease readback");
    auto held=static_cast<const unsigned char*>(heldPixels.pData);
    check(held[0]==(leasedTag&255)&&held[1]==((leasedTag>>8)&255)&&held[2]==((leasedTag>>16)&255),"consumer-leased image never overwritten while pool wraps");
    cc->Unmap(heldRead.Get(),0);check(lease->ReleaseSync(0)==S_OK,"return held lease");
    consumeTag(tag);check(handles.size()<=3,"held lease does not grow pool");
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
    puts("PASS: 240 GPU pairs, newest-frame bursts, three-slot bounded reuse, lease exclusion, expiry, recovery, invalid tracking and resize");
}
