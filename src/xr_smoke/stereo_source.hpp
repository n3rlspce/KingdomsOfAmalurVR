#pragma once
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include "../tracking/stereo_frame.hpp"

// Katanga's documented shared-surface protocol, as used by the MIT-licensed
// VRScreenCap Katanga loader. See research/STEREO_REUSE.md for provenance.
class StereoSource {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    HANDLE mapping_{};const volatile ULONG_PTR* mapped_{};ULONG_PTR handle_{};
    Ptr<ID3D11Texture2D> shared_,copy_;
    Ptr<ID3D11ShaderResourceView> view_;
    Ptr<ID3D11VertexShader> vs_;Ptr<ID3D11PixelShader> ps_;
    Ptr<ID3D11Buffer> constants_;Ptr<ID3D11SamplerState> sampler_;
    ULONGLONG retry_{};
    const wchar_t* mappingName_;
    amalur::StereoMailbox pairedBox_;
    amalur::StereoFrame pairedFrame_{};
    uint32_t openedProducer_{};uint64_t openedGeneration_{};
public:
    unsigned sourceWidth()const{D3D11_TEXTURE2D_DESC d{};if(copy_)copy_->GetDesc(&d);return d.Width/2;}
    unsigned sourceHeight()const{D3D11_TEXTURE2D_DESC d{};if(copy_)copy_->GetDesc(&d);return d.Height;}
    explicit StereoSource(const wchar_t* name=L"Local\\KatangaMappedFile",
        const wchar_t* pairedName=L"Local\\AmalurStereoFrameV1",const wchar_t* pairedMutex=L"Local\\AmalurStereoFrameMutexV1")
        :mappingName_(name),pairedBox_(pairedName,pairedMutex){}
    ~StereoSource(){if(mapped_)UnmapViewOfFile(const_cast<const ULONG_PTR*>(mapped_));if(mapping_)CloseHandle(mapping_);}
    bool initialize(ID3D11Device* device) {
        const char* code=R"(
cbuffer C:register(b0){float4 tangents;float4 parameters;float4 quality;}
Texture2D source:register(t0);SamplerState linearClamp:register(s0);
struct V{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
V vs(uint id:SV_VertexID){V o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float4 ps(V i):SV_TARGET{
 float2 ray=float2(lerp(tangents.x,tangents.y,i.uv.x),lerp(tangents.w,tangents.z,i.uv.y));
 float2 uv=float2(.5+.5*ray.x*parameters.x+parameters.w,.5-.5*ray.y*parameters.y);
 if(any(uv<0)||any(uv>1))return float4(0,0,0,1);
 uv.x=(uv.x+parameters.z)*.5;
 float3 color=source.Sample(linearClamp,uv).rgb;
 uint w,h;source.GetDimensions(w,h);float2 texel=1.0/float2(w,h);
 float3 neighbors=source.Sample(linearClamp,uv+float2(texel.x,0)).rgb+source.Sample(linearClamp,uv-float2(texel.x,0)).rgb+source.Sample(linearClamp,uv+float2(0,texel.y)).rgb+source.Sample(linearClamp,uv-float2(0,texel.y)).rgb;
 return float4(saturate(color+quality.x*(color-neighbors*.25)),1);
})";
        Ptr<ID3DBlob> vertex,pixel,error;
        if(FAILED(D3DCompile(code,strlen(code),nullptr,nullptr,nullptr,"vs","vs_4_0",0,0,&vertex,&error))||
           FAILED(D3DCompile(code,strlen(code),nullptr,nullptr,nullptr,"ps","ps_4_0",0,0,&pixel,&error)))return false;
        if(FAILED(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs_))||
           FAILED(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps_)))return false;
        D3D11_BUFFER_DESC b{};b.ByteWidth=48;b.Usage=D3D11_USAGE_DEFAULT;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;s.MaxLOD=D3D11_FLOAT32_MAX;
        return SUCCEEDED(device->CreateBuffer(&b,nullptr,&constants_))&&SUCCEEDED(device->CreateSamplerState(&s,&sampler_));
    }
    bool acquire(ID3D11Device* device,ID3D11DeviceContext* context) {
        auto now=GetTickCount64();
        if(!mapped_){
            if(now<retry_)return false;retry_=now+1000;
            mapping_=OpenFileMappingW(FILE_MAP_READ,FALSE,mappingName_);
            if(!mapping_)return false;
            mapped_=static_cast<const volatile ULONG_PTR*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,sizeof(ULONG_PTR)));
            if(!mapped_){CloseHandle(mapping_);mapping_=nullptr;return false;}
        }
        ULONG_PTR handle=*mapped_;
        return snapshot(device,context,handle,false);
    }
    bool acquirePaired(ID3D11Device* device,ID3D11DeviceContext* context,amalur::PosePacket& pose,
        uint64_t* sequence=nullptr){
        if(pairedBox_.open(false)){
            amalur::MailboxLock lock(pairedBox_);
            if(lock.frame&&lock.frame->version==1&&lock.frame->texture){
                const auto& f=*lock.frame;
                if(f.producer!=openedProducer_||f.generation!=openedGeneration_){
                    view_.Reset();shared_.Reset();copy_.Reset();handle_=0;pairedFrame_={};
                    openedProducer_=f.producer;openedGeneration_=f.generation;
                }
                if(f.sequence!=pairedFrame_.sequence&&snapshot(device,context,f.texture,true))pairedFrame_=f;
            }
        }
        auto now=GetTickCount64();
        if(!pairedFrame_.texture||now<pairedFrame_.published||now-pairedFrame_.published>1000)return false;
        pose=pairedFrame_.pose;if(sequence)*sequence=pairedFrame_.sequence;return true;
    }
    bool snapshot(ID3D11Device* device,ID3D11DeviceContext* context,ULONG_PTR handle,bool paired){
        auto now=GetTickCount64();
        if(!handle){view_.Reset();shared_.Reset();copy_.Reset();handle_=0;return false;}
        if(handle!=handle_||!view_){
            if(now<retry_&&handle==handle_)return false;retry_=now+1000;handle_=handle;
            view_.Reset();shared_.Reset();copy_.Reset();
            HRESULT hr=device->OpenSharedResource(reinterpret_cast<HANDLE>(handle),IID_PPV_ARGS(&shared_));
            if(FAILED(hr)){printf("Stereo shared texture open failed: %08lx\n",static_cast<unsigned long>(hr));return false;}
            D3D11_TEXTURE2D_DESC d{};shared_->GetDesc(&d);
            if(d.ArraySize!=1||d.SampleDesc.Count!=1||d.Width<2||d.Width%2)return false;
            DXGI_FORMAT format=d.Format;
            // geo-11's final color is display-encoded even when its shared
            // texture is labelled UNORM. Decode before the XR sRGB RTV encodes.
            // A typeless private copy permits an sRGB SRV without changing bytes.
            if(format==DXGI_FORMAT_R8G8B8A8_TYPELESS||format==DXGI_FORMAT_R8G8B8A8_UNORM||format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB){
                d.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            }
            if(format==DXGI_FORMAT_B8G8R8A8_TYPELESS||format==DXGI_FORMAT_B8G8R8A8_UNORM||format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB){
                d.Format=DXGI_FORMAT_B8G8R8A8_TYPELESS;format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            }
            d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.MiscFlags=0;d.CPUAccessFlags=0;d.Usage=D3D11_USAGE_DEFAULT;
            D3D11_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=format;sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sv.Texture2D.MipLevels=d.MipLevels;
            if(FAILED(device->CreateTexture2D(&d,nullptr,&copy_))||FAILED(device->CreateShaderResourceView(copy_.Get(),&sv,&view_)))return false;
            printf("Stereo source opened: %ux%u full SBS, format=%u\n",d.Width,d.Height,d.Format);
        }
        Ptr<IDXGIKeyedMutex> keyed;
        if(paired&&(FAILED(shared_.As(&keyed))||keyed->AcquireSync(1,0)!=S_OK))return false;
        context->CopyResource(copy_.Get(),shared_.Get());
        if(paired){context->Flush();if(keyed->ReleaseSync(0)!=S_OK)return false;}
        return true;
    }
    bool capture(ID3D11Device* device,ID3D11DeviceContext* context,const char* path){
        if(!copy_)return false;D3D11_TEXTURE2D_DESC d{};copy_->GetDesc(&d);
        bool rgba=d.Format==DXGI_FORMAT_R8G8B8A8_UNORM||d.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||d.Format==DXGI_FORMAT_R8G8B8A8_TYPELESS;
        bool bgra=d.Format==DXGI_FORMAT_B8G8R8A8_UNORM||d.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB||d.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS;
        if(!rgba&&!bgra)return false;
        d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
        Ptr<ID3D11Texture2D> staging;if(FAILED(device->CreateTexture2D(&d,nullptr,&staging)))return false;
        context->CopyResource(staging.Get(),copy_.Get());D3D11_MAPPED_SUBRESOURCE m{};
        if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)))return false;
        FILE* f{};fopen_s(&f,path,"wb");if(!f){context->Unmap(staging.Get(),0);return false;}
        BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+d.Width*d.Height*4;
        BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=d.Width;info.biHeight=-static_cast<LONG>(d.Height);info.biPlanes=1;info.biBitCount=32;
        fwrite(&file,sizeof(file),1,f);fwrite(&info,sizeof(info),1,f);
        for(UINT y=0;y<d.Height;++y)for(UINT x=0;x<d.Width;++x){
            const auto p=static_cast<const unsigned char*>(m.pData)+y*m.RowPitch+x*4;
            unsigned char pixel[4]={p[rgba?2:0],p[1],p[rgba?0:2],255};fwrite(pixel,4,1,f);
        }
        context->Unmap(staging.Get(),0);fclose(f);return true;
    }
    void draw(ID3D11DeviceContext* context,int eye,float left,float right,float down,float up,float px,float py,float sourceBias=0,float sharpness=0){
        float data[12]={left,right,down,up,px,py,float(eye),sourceBias,sharpness,0,0,0};
        context->UpdateSubresource(constants_.Get(),0,nullptr,data,0,0);
        auto cb=constants_.Get();auto srv=view_.Get();auto sampler=sampler_.Get();
        context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs_.Get(),nullptr,0);context->PSSetShader(ps_.Get(),nullptr,0);
        context->PSSetConstantBuffers(0,1,&cb);context->PSSetShaderResources(0,1,&srv);context->PSSetSamplers(0,1,&sampler);
        context->Draw(3,0);srv=nullptr;context->PSSetShaderResources(0,1,&srv);
    }
};
