#define NOMINMAX
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
using Microsoft::WRL::ComPtr;
static void log(const char*,...){}
#include "../diagnostic/hud_size.hpp"
#include "../xr_smoke/vr_settings.hpp"
static void check(bool ok,const char* message){if(!ok){printf("FAIL: %s\n",message);std::exit(1);}}
int main(){
    std::wstring name=L"Local\\AmalurHudTest"+std::to_wstring(GetCurrentProcessId()),mutex=name+L"Mutex";
    amalur::HudSettingsChannel writer(name.c_str(),mutex.c_str()),reader(name.c_str(),mutex.c_str());
    check(writer.open(true)&&reader.open(false),"open isolated HUD channel");
    float value=0;writer.publish(.65f);check(reader.read(value)&&std::abs(value-.65f)<.0001f,"HUD setting crosses channel");
    writer.publish(std::numeric_limits<float>::quiet_NaN());writer.publish(2.f);
    check(reader.read(value)&&std::abs(value-.65f)<.0001f,"invalid settings preserve valid packet");

    VrSettings settings;settings.path+=L".test";settings.hudSize=.8f;settings.visible=true;
    settings.held[VK_LEFT]=true;settings.poll();check(std::abs(settings.hudSize-.75f)<.0001f,"HUD row adjusts in 5 percent steps");
    settings.held[VK_LEFT]=false;settings.poll();settings.hudSize=1;settings.load();
    check(std::abs(settings.hudSize-.75f)<.0001f,"HUD slider persists");
    settings.held[VK_HOME]=true;settings.poll();check(std::abs(settings.hudSize-.8f)<.0001f,"reset restores 80 percent");

    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"WARP device");
    D3D11_TEXTURE1D_DESC d{};d.Width=4;d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture1D> original;ComPtr<ID3D11ShaderResourceView> originalView;
    check(SUCCEEDED(device->CreateTexture1D(&d,nullptr,&original))&&SUCCEEDED(device->CreateShaderResourceView(original.Get(),nullptr,&originalView)),"original slot contents");
    auto ptr=originalView.Get();context->VSSetShaderResources(119,1,&ptr);
    context->VSSetShaderResources(118,1,&ptr);
    D3D11_BUFFER_DESC bd{};bd.ByteWidth=16;bd.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Buffer> rawBuffer;check(SUCCEEDED(device->CreateBuffer(&bd,nullptr,&rawBuffer)),"raw stereo buffer");
    D3D11_SHADER_RESOURCE_VIEW_DESC rawDesc{};rawDesc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;rawDesc.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;rawDesc.Buffer.NumElements=1;
    ComPtr<ID3D11ShaderResourceView> rawView;check(SUCCEEDED(device->CreateShaderResourceView(rawBuffer.Get(),&rawDesc,&rawView)),"raw stereo view");
    auto raw=rawView.Get();context->VSSetShaderResources(125,1,&raw);
    for(float size:{.4f,.8f,1.2f})for(bool flat:{false,true})for(bool dialogue:{false,true})for(bool menu:{false,true}){
        hud_size::requested=size;hud_size::requestedMenu=.6f;
        {hud_size::Binding binding(context.Get(),true,flat,dialogue,menu);ComPtr<ID3D11ShaderResourceView> current;context->VSGetShaderResources(119,1,&current);
            ComPtr<ID3D11ShaderResourceView> alias;context->VSGetShaderResources(118,1,&alias);check(alias.Get()==rawView.Get(),"raw stereo buffer aliases t118 without changing t125");
            check(current&&current.Get()!=originalView.Get(),"HUD binding installed");
            ComPtr<ID3D11Resource> resource;current->GetResource(&resource);
            d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture1D> staging;check(SUCCEEDED(device->CreateTexture1D(&d,nullptr,&staging)),"readback texture");context->CopyResource(staging.Get(),resource.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"GPU setting readback");
            auto values=static_cast<float*>(mapped.pData);check(std::abs(values[0]-(menu&&!dialogue?.6f:size))<.0001f&&values[1]==1,"GPU receives size and valid marker");
            check(values[2]==(flat?1.f:0.f)&&values[3]==(dialogue?1.f:(menu?2.f:0.f)),"GPU receives flat-view and dialogue transitions even at unchanged size");
            context->Unmap(staging.Get(),0);
        }
        ComPtr<ID3D11ShaderResourceView> restored;context->VSGetShaderResources(119,1,&restored);check(restored.Get()==originalView.Get(),"caller slot restored");
        context->VSGetShaderResources(118,1,&restored);check(restored.Get()==originalView.Get(),"raw alias slot restored");
    }
    {hud_size::Binding binding(context.Get(),false);ComPtr<ID3D11ShaderResourceView> current;context->VSGetShaderResources(119,1,&current);check(current.Get()==originalView.Get(),"ordinary draws unchanged");}
    puts("PASS: HUD IPC, input/persistence, GPU size/dialogue/flat transitions and binding restoration");
}
