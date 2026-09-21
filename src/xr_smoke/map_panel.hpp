#pragma once
#include "stereo_source.hpp"
// Map prototype: one native menu image, one rigid quad shared by both eyes.
// The compositor performs head-motion perspective; no per-widget transform.
class MapPanel {
    XrSwapchain chain{};std::vector<XrSwapchainImageD3D11KHR> images;
    unsigned width{},height{};
public:
    ~MapPanel(){if(chain)xrDestroySwapchain(chain);}
    XrCompositionLayerQuad draw(XrSession session,XrSpace space,DXGI_FORMAT format,
        ID3D11Device* device,ID3D11DeviceContext* context,StereoSource& source,
        const XrPosef& pose,float scale,float sharpness){
        const unsigned wantedWidth=std::min(2048u,source.sourceWidth());
        const unsigned wantedHeight=std::max(1u,static_cast<unsigned>(double(wantedWidth)*source.sourceHeight()/source.sourceWidth()));
        if(!chain||width!=wantedWidth||height!=wantedHeight){
            if(chain){XR(xrDestroySwapchain(chain));chain={};}width=wantedWidth;height=wantedHeight;
            XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};info.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;info.format=format;info.sampleCount=1;info.width=width;info.height=height;info.faceCount=info.arraySize=info.mipCount=1;
            XR(xrCreateSwapchain(session,&info,&chain));uint32_t count{};XR(xrEnumerateSwapchainImages(chain,0,&count,nullptr));images.assign(count,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});XR(xrEnumerateSwapchainImages(chain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())));
        }
        uint32_t index{};XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};XR(xrAcquireSwapchainImage(chain,&acquire,&index));
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=XR_INFINITE_DURATION;XR(xrWaitSwapchainImage(chain,&wait));
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;D3D11_RENDER_TARGET_VIEW_DESC desc{};desc.Format=format;desc.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
        hrcheck(device->CreateRenderTargetView(images[index].texture,&desc,&target));auto rt=target.Get();context->OMSetRenderTargets(1,&rt,nullptr);
        D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context->RSSetViewports(1,&viewport);
        source.draw(context,0,-1,1,-1,1,1,1,0,sharpness);context->OMSetRenderTargets(0,nullptr,nullptr);
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};XR(xrReleaseSwapchainImage(chain,&release));
        XrCompositionLayerQuad layer{XR_TYPE_COMPOSITION_LAYER_QUAD};layer.space=space;layer.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
        layer.subImage.swapchain=chain;layer.subImage.imageRect.extent={static_cast<int32_t>(width),static_cast<int32_t>(height)};
        layer.pose=pose;layer.size={2.f*scale,2.f*scale*height/width};return layer;
    }
};
