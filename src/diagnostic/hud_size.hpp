#pragma once
#include "../tracking/hud_settings.hpp"
#include "../tracking/menu_rotation.hpp"
#include "../tracking/map_panel_settings.hpp"

namespace hud_size {
static amalur::HudSettingsChannel channel;
static amalur::HudSettingsChannel menuChannel{L"Local\\AmalurMenuSettingsV1",L"Local\\AmalurMenuSettingsMutexV1"};
static float requested=.8f,uploaded=-1;
static float requestedMenu=.8f;
static amalur::MapPanelSettings mapPanelChannel{L"Local\\AmalurUiLayerV2",L"Local\\AmalurUiLayerMutexV2"};
static bool mapPanelRequested=false,wristRequested=false;
static amalur::MapPanelSettings wristChannel{L"Local\\AmalurWristHudV1",L"Local\\AmalurWristHudMutexV1"};
static bool uploadedInterface{},uploadedDialogue{},uploadedMenu{},uploadedGameplay{};
static amalur::MenuRotation menuRotation;
static std::array<float,12> uploadedRotation{};
static ComPtr<ID3D11Texture1D> texture;
static ComPtr<ID3D11ShaderResourceView> view;
static void poll(){if(channel.open(false))channel.read(requested);if(menuChannel.open(false))menuChannel.read(requestedMenu);mapPanelRequested=mapPanelChannel.read();wristRequested=wristChannel.read();}
// Bind only around our four replacement shaders and restore the caller's slot.
// Never patch geo-11's own parameter texture or reload its config per adjustment.
class Binding {
    ID3D11DeviceContext* context{};
    ComPtr<ID3D11ShaderResourceView> previous;
    ComPtr<ID3D11ShaderResourceView> previousRaw;
public:
    Binding(ID3D11DeviceContext* c,bool usesControl,bool flat=false,bool dialogue=false,bool menu=false,bool gameplay=false){
        if(!usesControl)return;
        if(!texture){
            ComPtr<ID3D11Device> device;c->GetDevice(&device);
            D3D11_TEXTURE1D_DESC d{};d.Width=4;d.MipLevels=1;d.ArraySize=1;
            d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            if(FAILED(device->CreateTexture1D(&d,nullptr,&texture)))return;
            if(FAILED(device->CreateShaderResourceView(texture.Get(),nullptr,&view))){texture.Reset();return;}
        }
        const float size=menu&&!dialogue?requestedMenu:requested;
        if(uploaded!=size||uploadedInterface!=flat||uploadedDialogue!=dialogue||uploadedMenu!=menu||uploadedGameplay!=gameplay||uploadedRotation!=menuRotation.matrix){
            float values[16]{size,1,flat?1.f:0.f,dialogue?1.f:(menu?2.f:(gameplay?3.f:0.f))};
            std::copy(menuRotation.matrix.begin(),menuRotation.matrix.end(),values+4);
            c->UpdateSubresource(texture.Get(),0,nullptr,values,sizeof(values),0);
            uploadedRotation=menuRotation.matrix;
            uploadedInterface=flat;
            uploadedDialogue=dialogue;
            uploadedMenu=menu;
            uploadedGameplay=gameplay;
            if(uploaded!=size)log("HUD size uploaded: %.0f%%\n",size*100);uploaded=size;
        }
        context=c;c->VSGetShaderResources(119,1,&previous);
        c->VSGetShaderResources(118,1,&previousRaw);
        ComPtr<ID3D11ShaderResourceView> stereo;c->VSGetShaderResources(125,1,&stereo);
        // Only alias the actual geo-11 buffer, never a legacy texture resource.
        D3D11_SHADER_RESOURCE_VIEW_DESC rawDesc{};if(stereo)stereo->GetDesc(&rawDesc);
        auto raw=rawDesc.ViewDimension==D3D11_SRV_DIMENSION_BUFFER?stereo.Get():nullptr;
        c->VSSetShaderResources(118,1,&raw);
        auto resource=view.Get();c->VSSetShaderResources(119,1,&resource);
    }
    ~Binding(){if(context){auto resource=previous.Get();context->VSSetShaderResources(119,1,&resource);auto raw=previousRaw.Get();context->VSSetShaderResources(118,1,&raw);}}
};
}
