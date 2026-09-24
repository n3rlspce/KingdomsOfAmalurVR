#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <array>

namespace amalur {
// A native-resolution, transparent UI canvas. Only full-output-sized color
// targets with positively identified UI shaders are eligible.
// The caller supplies positively identified UI draws. Nothing is replayed and
// the native viewport, scissor rectangles, textures and stencil are retained.
class UiCapture {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    UINT width_=0,height_=0,firstSlice_=0,firstSlices_=1;
    unsigned rejection_=0;
    D3D11_TEXTURE2D_DESC observed_{};D3D11_VIEWPORT viewport_{};
    Ptr<IUnknown> firstEye_;
    Ptr<ID3D11Texture2D> canvas_,mono_;
    Ptr<ID3D11RenderTargetView> target_;
    Ptr<ID3D11Texture2D> otherCanvas_;
    Ptr<ID3D11RenderTargetView> otherTarget_;
    Ptr<ID3D11BlendState> blend_,sourceBlend_;
    bool haveBlend_=false,cleared_=false,otherCleared_=false;
    unsigned draws_=0;
public:
    // Native room copies share HUD vertex shaders. Recognize them even when
    // capture is inactive so they cannot fall through to HUD transformation.
    static bool sceneBackdrop(ID3D11DeviceContext* c){
        Ptr<ID3D11RenderTargetView> target;c->OMGetRenderTargets(1,&target,nullptr);if(!target)return false;
        Ptr<ID3D11Resource> output;target->GetResource(&output);Ptr<ID3D11Texture2D> destination;
        if(FAILED(output.As(&destination)))return false;
        D3D11_TEXTURE2D_DESC d{};destination->GetDesc(&d);
        Ptr<ID3D11ShaderResourceView> input;c->PSGetShaderResources(0,1,&input);if(!input)return false;
        Ptr<ID3D11Resource> source;input->GetResource(&source);Ptr<ID3D11Texture2D> image;
        if(FAILED(source.As(&image)))return false;
        D3D11_TEXTURE2D_DESC sd{};image->GetDesc(&sd);
        if(!(sd.BindFlags&D3D11_BIND_RENDER_TARGET))return false;
        if(sd.Width==d.Width&&sd.Height==d.Height)return true;
        // Loading/resize can retain the previous scene target. Permit modest
        // aspect changes, but keep square minimap render targets eligible.
        const double aspect=double(sd.Width)/sd.Height,outputAspect=double(d.Width)/d.Height;
        return aspect>=1.3&&aspect<=2.4&&aspect>outputAspect*.85&&aspect<outputAspect*1.15
            &&sd.Width>=d.Width/4&&sd.Height>=d.Height/4;
    }
    void output(ID3D11Texture2D* texture){
        D3D11_TEXTURE2D_DESC d{};if(texture)texture->GetDesc(&d);width_=d.Width;height_=d.Height;
    }
    void releaseOutput(){width_=height_=0;firstEye_.Reset();}
    void beginFrame(){cleared_=otherCleared_=false;draws_=0;firstEye_.Reset();}
    unsigned rejection()const{return rejection_;}
    const D3D11_TEXTURE2D_DESC& observed()const{return observed_;}
    const D3D11_VIEWPORT& observedViewport()const{return viewport_;}
    unsigned draws()const{return draws_;}
    ID3D11Texture2D* texture(ID3D11DeviceContext* context=nullptr){
        if(!canvas_)return nullptr;
        D3D11_TEXTURE2D_DESC d{},old{};canvas_->GetDesc(&d);
        if(d.ArraySize==1)return canvas_.Get();
        if(!context)return nullptr;
        if(mono_)mono_->GetDesc(&old);
        if(!mono_||d.Width!=old.Width||d.Height!=old.Height||d.Format!=old.Format){
            Ptr<ID3D11Device> device;context->GetDevice(&device);d.ArraySize=1;
            Ptr<ID3D11Texture2D> flat;if(FAILED(device->CreateTexture2D(&d,nullptr,&flat)))return nullptr;mono_=flat;
        }
        // Keep the native layered draw/depth view intact. Only after rendering
        // copy eye zero into a mono texture for the compositor/shared channel.
        context->CopySubresourceRegion(mono_.Get(),0,0,0,0,canvas_.Get(),0,nullptr);
        return mono_.Get();
    }
    class Binding {
        UiCapture& owner_;
        ID3D11DeviceContext* context_=nullptr;
        Ptr<ID3D11RenderTargetView> previous_;
        Ptr<ID3D11DepthStencilView> depth_;
        Ptr<ID3D11BlendState> blend_;
        FLOAT factors_[4]{};UINT mask_{};
        bool backdrop_=false;
    public:
        Binding(UiCapture& owner,ID3D11DeviceContext* c,bool eligible):owner_(owner){
            owner.rejection_=1;
            if(!eligible||!owner.width_||c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
            std::array<ID3D11RenderTargetView*,8> raw{};
            c->OMGetRenderTargets(8,raw.data(),&depth_);
            previous_.Attach(raw[0]);bool multiple=false;
            for(unsigned i=1;i<raw.size();++i)if(raw[i]){multiple=true;raw[i]->Release();}
            owner.rejection_=2;if(!previous_||multiple)return;
            Ptr<ID3D11Resource> resource;Ptr<IUnknown> identity;
            previous_->GetResource(&resource);resource.As(&identity);
            Ptr<ID3D11Texture2D> source; if(FAILED(resource.As(&source)))return;
            D3D11_TEXTURE2D_DESC d{},old{};source->GetDesc(&d);owner.observed_=d;owner.rejection_=3;
            if(d.SampleDesc.Count!=1||d.MipLevels!=1||d.Width!=owner.width_||d.Height!=owner.height_)return;
            UINT viewportCount=1;D3D11_VIEWPORT viewport{};c->RSGetViewports(&viewportCount,&viewport);
            owner.viewport_=viewport;owner.rejection_=4;
            if(viewportCount!=1||viewport.TopLeftX!=0||viewport.TopLeftY!=0||viewport.Width!=d.Width||viewport.Height!=d.Height)return;
            D3D11_RENDER_TARGET_VIEW_DESC rt{};previous_->GetDesc(&rt);
            UINT sourceSlice=0,captureSlices=1;owner.rejection_=5;
            if(rt.ViewDimension==D3D11_RTV_DIMENSION_TEXTURE2DARRAY){
                captureSlices=rt.Texture2DArray.ArraySize;
                if(captureSlices<1||captureSlices>2||rt.Texture2DArray.FirstArraySlice+captureSlices>d.ArraySize)return;
                sourceSlice=rt.Texture2DArray.FirstArraySlice;
                // Preserve layered routing for geometry shaders writing both eyes.
                // Single-eye views are still normalized to capture slice zero.
                rt.Texture2DArray.FirstArraySlice=0;
            }else if(rt.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D)return;
            // A native pause background may sample a full-size scene render
            // target through the same 2D shader. Keep that draw in the world;
            // copying it into the transparent layer recreates the nested room.
            // These native UI pixel shaders sample t0. Other slots may retain
            // unused scene textures from a previous draw and are not evidence.
            backdrop_=UiCapture::sceneBackdrop(c);
            owner.rejection_=6;if(backdrop_)return;
            // Do not let unrelated depth-tested geometry into the overlay.
            Ptr<ID3D11DepthStencilState> ds;UINT stencil{};c->OMGetDepthStencilState(&ds,&stencil);
            D3D11_DEPTH_STENCIL_DESC dd{};if(ds)ds->GetDesc(&dd);
            owner.rejection_=7;if(!ds||(dd.DepthEnable&&dd.DepthWriteMask!=D3D11_DEPTH_WRITE_MASK_ZERO))return;
            if(!owner.firstEye_){owner.firstEye_=identity;owner.firstSlice_=sourceSlice;owner.firstSlices_=captureSlices;}
            const bool other=owner.firstEye_.Get()!=identity.Get()||owner.firstSlice_!=sourceSlice||owner.firstSlices_!=captureSlices;
            auto& canvas=other?owner.otherCanvas_:owner.canvas_;
            auto& captureTarget=other?owner.otherTarget_:owner.target_;
            auto& cleared=other?owner.otherCleared_:owner.cleared_;
            Ptr<ID3D11Device> device;c->GetDevice(&device);
            if(canvas)canvas->GetDesc(&old);
            if(!canvas||d.Width!=old.Width||d.Height!=old.Height||d.Format!=old.Format||captureSlices!=old.ArraySize){
                d.ArraySize=captureSlices;d.Usage=D3D11_USAGE_DEFAULT;d.CPUAccessFlags=0;
                d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
                // This device is below geo-11. Keep the second native eye in
                // a separate sink to avoid accumulating its alpha twice.
                d.MiscFlags=0;
                Ptr<ID3D11Texture2D> texture;Ptr<ID3D11RenderTargetView> target;
                owner.rejection_=8;
                if(FAILED(device->CreateTexture2D(&d,nullptr,&texture))||FAILED(device->CreateRenderTargetView(texture.Get(),&rt,&target)))return;
                canvas=texture;captureTarget=target;cleared=false;
            }
            c->OMGetBlendState(&blend_,factors_,&mask_);
            if(!owner.haveBlend_||owner.sourceBlend_.Get()!=blend_.Get()){
                D3D11_BLEND_DESC b{};
                if(blend_)blend_->GetDesc(&b);
                else {b.RenderTarget[0].SrcBlend=b.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;b.RenderTarget[0].DestBlend=b.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ZERO;b.RenderTarget[0].BlendOp=b.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;b.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;}
                // Native RGB blending already produces premultiplied color on
                // transparent black. Accumulate coverage independently of the
                // native framebuffer's otherwise unused alpha equation.
                if(b.RenderTarget[0].BlendEnable){
                    b.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
                    b.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
                    b.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
                }
                if(b.RenderTarget[0].RenderTargetWriteMask&(D3D11_COLOR_WRITE_ENABLE_RED|D3D11_COLOR_WRITE_ENABLE_GREEN|D3D11_COLOR_WRITE_ENABLE_BLUE))b.RenderTarget[0].RenderTargetWriteMask|=D3D11_COLOR_WRITE_ENABLE_ALPHA;
                Ptr<ID3D11BlendState> fixed;owner.rejection_=9;if(FAILED(device->CreateBlendState(&b,&fixed)))return;
                owner.blend_=fixed;owner.sourceBlend_=blend_;owner.haveBlend_=true;
            }
            if(!cleared){const FLOAT clear[4]{};c->ClearRenderTargetView(captureTarget.Get(),clear);cleared=true;}
            auto target=captureTarget.Get();c->OMSetRenderTargets(1,&target,depth_.Get());
            c->OMSetBlendState(owner.blend_.Get(),factors_,mask_);context_=c;owner.rejection_=0;
        }
        explicit operator bool()const{return context_!=nullptr;}
        bool nativeBackdrop()const{return backdrop_;}
        ~Binding(){if(context_){auto target=previous_.Get();context_->OMSetRenderTargets(1,&target,depth_.Get());context_->OMSetBlendState(blend_.Get(),factors_,mask_);++owner_.draws_;}}
    };
};
}
