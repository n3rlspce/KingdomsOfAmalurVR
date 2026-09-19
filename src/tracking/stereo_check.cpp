#define NOMINMAX
#include "../xr_smoke/stereo_source.hpp"
#include <vector>
using Microsoft::WRL::ComPtr;
static void check(bool ok,const char* what){if(!ok){printf("FAIL: %s\n",what);exit(1);}}
int main(int argc,char** argv){
    ComPtr<ID3D11Device> d;ComPtr<ID3D11DeviceContext> c;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,nullptr,&c)),"device");
    if(argc==3&&strcmp(argv[1],"--capture")==0){
        StereoSource source;check(source.initialize(d.Get()),"presenter shaders");
        check(source.acquire(d.Get(),c.Get()),"live Katanga stereo texture");
        check(source.capture(d.Get(),c.Get(),argv[2]),"capture");puts("PASS: live stereo texture captured");return 0;
    }
    const wchar_t* name=L"Local\\AmalurStereoUnitTest";
    HANDLE map=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(ULONG_PTR),name);
    auto data=static_cast<ULONG_PTR*>(MapViewOfFile(map,FILE_MAP_WRITE,0,0,sizeof(ULONG_PTR)));check(data!=nullptr,"test mapping");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=8;desc.Height=4;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED;
    UINT pixels[32];for(int y=0;y<4;++y)for(int x=0;x<8;++x)pixels[y*8+x]=x<4?0xff0000ff:0xff00ff00;
    D3D11_SUBRESOURCE_DATA initial{pixels,32,0};ComPtr<ID3D11Texture2D> source;
    check(SUCCEEDED(d->CreateTexture2D(&desc,&initial,&source)),"source");ComPtr<IDXGIResource> resource;check(SUCCEEDED(source.As(&resource)),"shared interface");HANDLE handle{};resource->GetSharedHandle(&handle);*data=reinterpret_cast<ULONG_PTR>(handle);c->Flush();
    StereoSource stereo(name);check(stereo.initialize(d.Get())&&stereo.acquire(d.Get(),c.Get()),"shared source and shaders");
    desc.Width=4;desc.MiscFlags=0;ComPtr<ID3D11Texture2D> target;check(SUCCEEDED(d->CreateTexture2D(&desc,nullptr,&target)),"target");ComPtr<ID3D11RenderTargetView> rtv;d->CreateRenderTargetView(target.Get(),nullptr,&rtv);
    D3D11_VIEWPORT vp{0,0,4,4,0,1};c->RSSetViewports(1,&vp);auto rt=rtv.Get();c->OMSetRenderTargets(1,&rt,nullptr);
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> read;d->CreateTexture2D(&desc,nullptr,&read);
    for(int eye=0;eye<2;++eye){
        stereo.draw(c.Get(),eye,-1,1,-1,1,1,1);c->CopyResource(read.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE m{};
        check(SUCCEEDED(c->Map(read.Get(),0,D3D11_MAP_READ,0,&m)),"readback");auto p=static_cast<unsigned char*>(m.pData)+2*m.RowPitch+2*4;
        check(eye==0?(p[0]>250&&p[1]<5):(p[1]>250&&p[0]<5),"correct eye image sampled");c->Unmap(read.Get(),0);
    }
    // A midgray display-encoded input must survive the sRGB output round trip.
    // Sampling it as linear instead would brighten 128 to about 188.
    for(auto& p:pixels)p=0xff808080;
    c->UpdateSubresource(source.Get(),0,nullptr,pixels,32,0);c->Flush();
    check(stereo.acquire(d.Get(),c.Get()),"gray source snapshot");
    D3D11_TEXTURE2D_DESC colorDesc{};target->GetDesc(&colorDesc);colorDesc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
    ComPtr<ID3D11Texture2D> colorTarget;check(SUCCEEDED(d->CreateTexture2D(&colorDesc,nullptr,&colorTarget)),"sRGB target");
    D3D11_RENDER_TARGET_VIEW_DESC colorView{};colorView.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;colorView.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11RenderTargetView> colorRtv;check(SUCCEEDED(d->CreateRenderTargetView(colorTarget.Get(),&colorView,&colorRtv)),"sRGB RTV");
    rt=colorRtv.Get();c->OMSetRenderTargets(1,&rt,nullptr);stereo.draw(c.Get(),0,-1,1,-1,1,1,1);
    c->CopyResource(read.Get(),colorTarget.Get());D3D11_MAPPED_SUBRESOURCE gray{};
    check(SUCCEEDED(c->Map(read.Get(),0,D3D11_MAP_READ,0,&gray)),"gray readback");
    auto gp=static_cast<unsigned char*>(gray.pData)+2*gray.RowPitch+2*4;
    printf("sRGB round trip: input=128 output=%u,%u,%u\n",gp[0],gp[1],gp[2]);
    check(gp[0]>=127&&gp[0]<=129&&gp[1]>=127&&gp[1]<=129&&gp[2]>=127&&gp[2]<=129,"midgray brightness preserved");c->Unmap(read.Get(),0);
    UnmapViewOfFile(data);CloseHandle(map);puts("PASS: shared stereo texture, left/right routing, gamma and GPU pixel readback");
}
