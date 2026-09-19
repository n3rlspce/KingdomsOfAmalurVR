#pragma once
#include "../tracking/hud_settings.hpp"

namespace hud_size {
static amalur::HudSettingsChannel channel;
static float requested=.8f,uploaded=-1;
static ComPtr<ID3D11Texture1D> texture;
static ComPtr<ID3D11ShaderResourceView> view;
static void poll(){if(channel.open(false))channel.read(requested);}
// Bind only around our four replacement shaders and restore the caller's slot.
// Never patch geo-11's own parameter texture or reload its config per adjustment.
class Binding {
    ID3D11DeviceContext* context{};
    ComPtr<ID3D11ShaderResourceView> previous;
public:
    Binding(ID3D11DeviceContext* c,bool usesControl){
        if(!usesControl)return;
        if(!texture){
            ComPtr<ID3D11Device> device;c->GetDevice(&device);
            D3D11_TEXTURE1D_DESC d{};d.Width=1;d.MipLevels=1;d.ArraySize=1;
            d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            if(FAILED(device->CreateTexture1D(&d,nullptr,&texture)))return;
            if(FAILED(device->CreateShaderResourceView(texture.Get(),nullptr,&view))){texture.Reset();return;}
        }
        if(uploaded!=requested){
            float values[4]{requested,1,0,0};c->UpdateSubresource(texture.Get(),0,nullptr,values,sizeof(values),0);
            uploaded=requested;log("HUD size uploaded: %.0f%%\n",uploaded*100);
        }
        context=c;c->VSGetShaderResources(119,1,&previous);
        auto resource=view.Get();c->VSSetShaderResources(119,1,&resource);
    }
    ~Binding(){if(context){auto resource=previous.Get();context->VSSetShaderResources(119,1,&resource);}}
};
}
