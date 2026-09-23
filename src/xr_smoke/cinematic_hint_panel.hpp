#pragma once
// Small screen-anchored instruction, independent of game resolution/subtitles.
class CinematicHintPanel {
    static constexpr int width=768,height=72;
    XrSwapchain chain{};std::vector<XrSwapchainImageD3D11KHR> images;
    std::vector<unsigned char> pixels,upload;
public:
    ~CinematicHintPanel(){if(chain)xrDestroySwapchain(chain);}
    void initialize(XrSession session,DXGI_FORMAT format){
        XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};info.usageFlags=XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT|XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;info.format=format;info.sampleCount=1;info.width=width;info.height=height;info.faceCount=1;info.arraySize=1;info.mipCount=1;
        XR(xrCreateSwapchain(session,&info,&chain));uint32_t count{};XR(xrEnumerateSwapchainImages(chain,0,&count,nullptr));images.resize(count,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});XR(xrEnumerateSwapchainImages(chain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())));
        HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        void* bits{};HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
        if(!dc||!bitmap){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);throw std::runtime_error("Cinematic hint bitmap failed");}
        auto old=SelectObject(dc,bitmap);HFONT font=CreateFontW(-34,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto oldFont=SelectObject(dc,font);
        RECT rect{0,0,width,height};auto brush=CreateSolidBrush(RGB(14,21,31));FillRect(dc,&rect,brush);DeleteObject(brush);
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(237,242,250));DrawTextW(dc,L"Hold both grips + stick \u2191\u2193 to resize",-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);GdiFlush();
        pixels.resize(width*height*4);upload.resize(pixels.size());const auto in=static_cast<const unsigned char*>(bits);const bool bgra=format==DXGI_FORMAT_B8G8R8A8_UNORM||format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        for(size_t i=0;i<pixels.size();i+=4){pixels[i]=in[i+(bgra?0:2)];pixels[i+1]=in[i+1];pixels[i+2]=in[i+(bgra?2:0)];pixels[i+3]=225;}
        SelectObject(dc,oldFont);SelectObject(dc,old);DeleteObject(font);DeleteObject(bitmap);DeleteDC(dc);
    }
    XrCompositionLayerQuad draw(ID3D11DeviceContext* context,XrSpace space,const XrPosef& screen,float screenHeight,float alpha){
        upload=pixels;for(size_t i=3;i<upload.size();i+=4)upload[i]=static_cast<unsigned char>(pixels[i]*alpha);
        uint32_t index{};XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};XR(xrAcquireSwapchainImage(chain,&acquire,&index));XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=XR_INFINITE_DURATION;XR(xrWaitSwapchainImage(chain,&wait));context->UpdateSubresource(images[index].texture,0,nullptr,upload.data(),width*4,0);XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};XR(xrReleaseSwapchainImage(chain,&release));
        XrCompositionLayerQuad layer{XR_TYPE_COMPOSITION_LAYER_QUAD};layer.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;layer.space=space;layer.eyeVisibility=XR_EYE_VISIBILITY_BOTH;layer.subImage.swapchain=chain;layer.subImage.imageRect.extent={width,height};layer.pose=screen;
        const auto q=screen.orientation;XMFLOAT3 offset;XMStoreFloat3(&offset,XMVector3Rotate(XMVectorSet(0,screenHeight*.5f-.07f,.01f,0),XMVectorSet(q.x,q.y,q.z,q.w)));
        layer.pose.position.x+=offset.x;layer.pose.position.y+=offset.y;layer.pose.position.z+=offset.z;layer.size={.8f,.075f};return layer;
    }
};
