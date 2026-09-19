#define NOMINMAX
#include "stereo_frame.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;

// Unlike StereoMailbox::lock(), an observer must not reset abandoned metadata.
struct ReadMailbox {
    HANDLE mapping{},mutex{};const amalur::StereoFrame* data{};
    ~ReadMailbox(){if(data)UnmapViewOfFile(data);if(mapping)CloseHandle(mapping);if(mutex)CloseHandle(mutex);}
    bool open(){
        mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,L"Local\\AmalurStereoFrameV1");
        mutex=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,L"Local\\AmalurStereoFrameMutexV1");
        if(mapping&&mutex)data=static_cast<const amalur::StereoFrame*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(amalur::StereoFrame)));
        return data!=nullptr;
    }
};
struct CpuLease {
    HANDLE mutex;bool held{};
    explicit CpuLease(HANDLE m):mutex(m){auto status=WaitForSingleObject(m,0);held=status==WAIT_OBJECT_0||status==WAIT_ABANDONED;}
    ~CpuLease(){if(held)ReleaseMutex(mutex);}
};
static bool finitePose(const amalur::PosePacket& p){
    for(float v:p.orientation)if(!std::isfinite(v))return false;
    for(float v:p.position)if(!std::isfinite(v))return false;
    for(float v:{p.projectionX,p.projectionY,p.worldScale,p.depth,p.convergence,p.horizontalFov})if(!std::isfinite(v))return false;
    return p.version==3&&p.valid&&p.gameMode==1&&p.projectionX>0&&p.projectionY>0;
}
static bool rgba(DXGI_FORMAT f){return f==DXGI_FORMAT_R8G8B8A8_TYPELESS||f==DXGI_FORMAT_R8G8B8A8_UNORM||f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;}
static bool bgra(DXGI_FORMAT f){return f==DXGI_FORMAT_B8G8R8A8_TYPELESS||f==DXGI_FORMAT_B8G8R8A8_UNORM||f==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;}

int wmain(int argc,wchar_t** argv){
    try {
        fs::path directory=fs::path(L"captures")/(L"stereo-spy-"+std::to_wstring(GetTickCount64()));
        if(argc==3&&std::wstring(argv[1])==L"--directory")directory=argv[2];
        else if(argc!=1){std::cerr<<"Usage: stereo_spy [--directory OUTPUT]\n";return 2;}
        ReadMailbox mailbox;if(!mailbox.open()){std::cerr<<"No live paired stereo mailbox.\n";return 3;}
        ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
        if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context))){std::cerr<<"D3D11 device failed.\n";return 4;}
        fs::create_directories(directory);
        std::ofstream metadata(directory/L"frames.jsonl",std::ios::app);metadata.exceptions(std::ios::badbit|std::ios::failbit);metadata<<std::setprecision(9);
        ComPtr<ID3D11Texture2D> shared,staging;ComPtr<IDXGIKeyedMutex> keyed;
        D3D11_TEXTURE2D_DESC desc{};uint32_t producer{};uint64_t generation{},lastSequence{};ULONG_PTR openedTexture{};
        unsigned captured=0;const auto start=GetTickCount64(),deadline=start+5000;
        for(unsigned slot=0;slot<10&&GetTickCount64()<deadline;++slot){
            const auto due=start+slot*500,slotEnd=due+500;
            while(GetTickCount64()<due)Sleep(1);
            bool copied=false;amalur::StereoFrame frame{};uint64_t observed{};
            while(!copied&&GetTickCount64()<slotEnd){
                {
                    CpuLease lease(mailbox.mutex);
                    if(lease.held){
                        const auto candidate=*mailbox.data;const auto now=GetTickCount64();
                        if(candidate.version==1&&candidate.texture&&candidate.sequence!=lastSequence&&finitePose(candidate.pose)
                            &&candidate.published<=now&&now-candidate.published<1000&&candidate.pose.tick<=now&&now-candidate.pose.tick<1000){
                            if(candidate.producer!=producer||candidate.generation!=generation||candidate.texture!=openedTexture||!shared){
                                shared.Reset();staging.Reset();keyed.Reset();
                                if(SUCCEEDED(device->OpenSharedResource(reinterpret_cast<HANDLE>(candidate.texture),IID_PPV_ARGS(&shared)))){
                                    shared->GetDesc(&desc);
                                    if(desc.ArraySize==1&&desc.SampleDesc.Count==1&&desc.Width>=2&&!(desc.Width%2)&&desc.Height
                                        &&(rgba(desc.Format)||bgra(desc.Format))&&SUCCEEDED(shared.As(&keyed))){
                                        auto sd=desc;sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.MiscFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
                                        if(FAILED(device->CreateTexture2D(&sd,nullptr,&staging)))staging.Reset();
                                    }
                                    producer=candidate.producer;generation=candidate.generation;openedTexture=candidate.texture;
                                }
                            }
                            if(staging&&keyed&&keyed->AcquireSync(1,0)==S_OK){
                                context->CopyResource(staging.Get(),shared.Get());context->Flush();
                                // Return the SAME consumer key: do not consume the
                                // live bridge's frame or allow the producer to overwrite it.
                                const auto released=keyed->ReleaseSync(1);
                                if(FAILED(released))throw std::runtime_error("Cannot return shared texture consumer key");
                                frame=candidate;observed=now;copied=true;
                            }
                        }
                    }
                } // CPU metadata lock released before readback or file I/O.
                if(!copied)Sleep(1);
            }
            if(!copied)continue;
            D3D11_MAPPED_SUBRESOURCE mapped{};HRESULT mappedResult;
            do {
                mappedResult=context->Map(staging.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
                if(mappedResult==DXGI_ERROR_WAS_STILL_DRAWING)Sleep(1);
            }while(mappedResult==DXGI_ERROR_WAS_STILL_DRAWING&&GetTickCount64()<deadline);
            if(FAILED(mappedResult)){if(mappedResult!=DXGI_ERROR_WAS_STILL_DRAWING)std::cerr<<"Staging readback failed.\n";continue;}
            const unsigned sourceWidth=desc.Width/2,width=std::min(640u,sourceWidth);
            const unsigned height=std::max(1u,static_cast<unsigned>((uint64_t(desc.Height)*width+sourceWidth/2)/sourceWidth));
            std::vector<unsigned char> image(size_t(width)*height);
            for(unsigned y=0;y<height;++y){
                const unsigned sy=std::min(desc.Height-1,static_cast<unsigned>((uint64_t(y)*desc.Height+desc.Height/2)/height));
                const auto row=static_cast<const unsigned char*>(mapped.pData)+size_t(sy)*mapped.RowPitch;
                for(unsigned x=0;x<width;++x){
                    const unsigned sx=std::min(sourceWidth-1,static_cast<unsigned>((uint64_t(x)*sourceWidth+sourceWidth/2)/width));
                    const auto pixel=row+size_t(sx)*4;
                    const unsigned r=pixel[rgba(desc.Format)?0:2],g=pixel[1],b=pixel[rgba(desc.Format)?2:0];
                    image[size_t(y)*width+x]=static_cast<unsigned char>((77*r+150*g+29*b+128)>>8);
                }
            }
            context->Unmap(staging.Get(),0);
            const auto file="frame-"+std::to_string(frame.producer)+"-"+std::to_string(frame.generation)+"-"+std::to_string(frame.sequence)+".pgm";
            {std::ofstream output(directory/file,std::ios::binary);output.exceptions(std::ios::badbit|std::ios::failbit);
                output<<"P5\n"<<width<<" "<<height<<"\n255\n";output.write(reinterpret_cast<const char*>(image.data()),image.size());}
            const auto& p=frame.pose;
            metadata<<"{\"file\":\""<<file<<"\",\"producer\":"<<frame.producer<<",\"generation\":"<<frame.generation
                <<",\"sequence\":"<<frame.sequence<<",\"published\":"<<frame.published<<",\"observed\":"<<observed
                <<",\"poseTick\":"<<p.tick<<",\"orientation\":["<<p.orientation[0]<<","<<p.orientation[1]<<","<<p.orientation[2]<<","<<p.orientation[3]
                <<"],\"position\":["<<p.position[0]<<","<<p.position[1]<<","<<p.position[2]<<"],\"projectionX\":"<<p.projectionX<<",\"projectionY\":"<<p.projectionY
                <<",\"worldScale\":"<<p.worldScale<<",\"horizontalFov\":"<<p.horizontalFov<<",\"depth\":"<<p.depth<<",\"convergence\":"<<p.convergence
                <<",\"recenter\":"<<p.recenter<<",\"sourceEye\":0,\"sourceWidth\":"<<sourceWidth<<",\"sourceHeight\":"<<desc.Height
                <<",\"width\":"<<width<<",\"height\":"<<height<<"}\n";metadata.flush();
            lastSequence=frame.sequence;++captured;
        }
        std::cout<<"Captured "<<captured<<" tracked image/pose pairs; observer returned GPU key 1.\n";
        return captured?0:5;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
