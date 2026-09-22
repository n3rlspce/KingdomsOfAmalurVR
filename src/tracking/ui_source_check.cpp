#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "../xr_smoke/stereo_source.hpp"
#include <stdexcept>
#include <vector>
#include <string>
#include <cmath>
using Microsoft::WRL::ComPtr;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void hr(HRESULT result){require(SUCCEEDED(result),"D3D failure");}
int main(){try{
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> c;D3D_FEATURE_LEVEL level;
    hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&c));
    constexpr unsigned w=65,h=49;std::vector<unsigned char> bytes(w*h*4);
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){auto p=&bytes[(y*w+x)*4];if(x<8||x>w-9){p[x<8?0:2]=128;p[3]=128;} }
    D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{bytes.data(),w*4,0};ComPtr<ID3D11Texture2D> source;hr(device->CreateTexture2D(&d,&initial,&source));
    const auto name=L"Local\\AmalurUiSourceCheck"+std::to_wstring(GetCurrentProcessId()),mutex=name+L"Mutex";
    amalur::StereoPublisher publisher(name.c_str(),mutex.c_str());StereoSource reader(L"unused",name.c_str(),mutex.c_str(),true);require(reader.initialize(device.Get()),"source shader compiles");
    amalur::PosePacket sent;sent.valid=1;sent.gameMode=7;sent.tick=GetTickCount64();require(publisher.publish(device.Get(),c.Get(),source.Get(),sent),"publish mono UI");
    amalur::PosePacket received;require(reader.acquirePaired(device.Get(),c.Get(),received),"acquire mono UI");require(received.gameMode==7&&received.tick==sent.tick,"UI metadata paired");
    require(reader.sourceWidth()==w&&reader.sourceHeight()==h,"mono dimensions cannot halve or crop canvas");
    d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;ComPtr<ID3D11Texture2D> target;hr(device->CreateTexture2D(&d,nullptr,&target));ComPtr<ID3D11RenderTargetView> rtv;hr(device->CreateRenderTargetView(target.Get(),nullptr,&rtv));auto rt=rtv.Get();c->OMSetRenderTargets(1,&rt,nullptr);D3D11_VIEWPORT vp{0,0,float(w),float(h),0,1};c->RSSetViewports(1,&vp);reader.draw(c.Get(),0,-1,1,-1,1,1,1,0,0,true);c->OMSetRenderTargets(0,nullptr,nullptr);
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> staging;hr(device->CreateTexture2D(&d,nullptr,&staging));c->CopyResource(staging.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE m{};hr(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&m));
    auto pixel=[&](unsigned x,unsigned y){return reinterpret_cast<float*>(static_cast<unsigned char*>(m.pData)+m.RowPitch*y)+x*4;};
    for(unsigned y:{0u,h/2,h-1}){auto a=pixel(0,y),b=pixel(w-1,y),z=pixel(w/2,y);
        require(std::abs(a[0]-128.f/255)<.01f&&std::abs(a[3]-128.f/255)<.01f,"left edge premultiplied red and alpha");
        require(std::abs(b[2]-128.f/255)<.01f&&std::abs(b[3]-128.f/255)<.01f,"right edge full-width blue and alpha");
        require(z[3]<.001f&&z[0]<.001f,"empty UI remains transparent");
    }c->Unmap(staging.Get(),0);
    puts("PASS: mono GPU transport and metadata, odd dimensions, full width/height, transparent background, premultiplied color/alpha");return 0;
}catch(const std::exception& e){printf("FAIL: %s\n",e.what());return 1;}}
