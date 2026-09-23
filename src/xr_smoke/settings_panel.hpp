#pragma once
#include "vr_settings.hpp"
#include <vector>
#include "developer_tools.hpp"

// An OpenXR quad keeps text readable independently of game render resolution.
// GDI supplies the font rasterizer; no input injection or game HUD patch needed.
class SettingsPanel {
    static constexpr int width=1100,height=1290;
    XrSwapchain chain{};std::vector<XrSwapchainImageD3D11KHR> images;
    HDC dc{};HBITMAP bitmap{};HGDIOBJ oldBitmap{};HFONT font{},title{};void* bits{};
    std::vector<unsigned char> pixels;DXGI_FORMAT format{};
public:
    ~SettingsPanel(){if(chain)xrDestroySwapchain(chain);if(dc)SelectObject(dc,oldBitmap);if(bitmap)DeleteObject(bitmap);if(font)DeleteObject(font);if(title)DeleteObject(title);if(dc)DeleteDC(dc);}
    void initialize(XrSession session,DXGI_FORMAT color){
        format=color;XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};info.usageFlags=XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT|XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;info.format=color;info.sampleCount=1;info.width=width;info.height=height;info.faceCount=1;info.arraySize=1;info.mipCount=1;
        XR(xrCreateSwapchain(session,&info,&chain));uint32_t n{};XR(xrEnumerateSwapchainImages(chain,0,&n,nullptr));images.resize(n,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});XR(xrEnumerateSwapchainImages(chain,n,&n,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())));
        dc=CreateCompatibleDC(nullptr);BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!dc||!bitmap)throw std::runtime_error("Settings panel bitmap failed");oldBitmap=SelectObject(dc,bitmap);
        font=CreateFontW(-29,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        title=CreateFontW(-43,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");pixels.resize(width*height*4);
    }
    XrCompositionLayerQuad draw(ID3D11DeviceContext* context,XrSpace view,const VrSettings& s,float ipd,unsigned eyeWidth,unsigned eyeHeight,unsigned sourceWidth,unsigned sourceHeight,bool tracking,int stereoStatus,const DeveloperTools* developer=nullptr){
        RECT all{0,0,width,height};HBRUSH background=CreateSolidBrush(RGB(14,21,31));FillRect(dc,&all,background);DeleteObject(background);SetBkMode(dc,TRANSPARENT);SelectObject(dc,title);SetTextColor(dc,RGB(237,242,250));
        auto text=[&](int x,int y,const std::wstring& value){TextOutW(dc,x,y,value.c_str(),static_cast<int>(value.size()));};
        if(s.developerVisible&&developer){
            text(42,28,L"1 Settings    |    [ 2 Dev cheats ]");SelectObject(dc,font);
            SetTextColor(dc,RGB(116,194,217));text(42,95,L"Both sticks: open / close    X: switch tab    B: close");
            text(42,132,L"Hold stick up/down: scroll    Left/right: destination    A / RT: run");
            SetTextColor(dc,RGB(220,228,237));
            text(42,174,L"Keyboard: * / F11 open, Tab switches tabs, arrows / Enter.");
            for(int i=0;i<VrSettings::developerPanelRows;++i){
                int y=220+i*28;
                if(i==s.developerRow){RECT row{24,y-5,width-24,y+28};HBRUSH h=CreateSolidBrush(RGB(34,67,88));FillRect(dc,&row,h);DeleteObject(h);}
                SetTextColor(dc,(developer->busy()&&i<amalur::developer::panelRows)?RGB(130,143,156):RGB(227,235,244));
                std::wstring label;
                if(i==VrSettings::attackPanelRow)label=VrSettings::attackPanel.enabled()?L"Attack detection panel: ON":L"Attack detection panel: OFF";
                else if(i>=VrSettings::ablationFirstRow&&i<=VrSettings::ablationResetRow){
                    static const wchar_t* names[]{L"TEST: Skip socket corrections",L"TEST: Skip root smoothing",L"TEST: Native mesh input (keep VR solve)",L"TEST: Native mesh positions (VR rotations)",L"TEST: Native mesh rotations (VR positions)"};
                    label=i==VrSettings::ablationResetRow?L"Reset jitter experiments (all OFF)":
                        std::wstring(names[i-VrSettings::ablationFirstRow])+(amalur::bodyDebug.enabled(VrSettings::ablationBits[i-VrSettings::ablationFirstRow])?L": ON":L": OFF");
                }else label=i==amalur::developer::panelRows+4?(amalur::bodyDebug.enabled(amalur::nativeCamera)?L"Native game camera (VR arms kept): ON":L"Native game camera (VR arms kept): OFF"):i==VrSettings::mapPanelRow?(s.mapPanelPrototype?L"Map panel prototype: ON":L"Map panel prototype: OFF"):i==amalur::developer::panelRows+2?(amalur::bodyDebug.enabled(amalur::nativeTorso)?L"Native upper-body animation: ON":L"Native upper-body animation: OFF"):i==amalur::developer::panelRows+3?(amalur::bodyDebug.enabled(amalur::nativeArms)?L"Native arms and wrists: ON":L"Native arms and wrists: OFF"):i==amalur::developer::panelRows+1?(VrSettings::thirdPersonCamera.enabled()?L"Third-person camera (1 m): ON":L"Third-person camera (1 m): OFF"):i==amalur::developer::panelRows?(!amalur::meleeDebugAvailable?L"Weapon collisions: disabled (crash investigation)":VrSettings::meleeDebug.enabled()?L"Weapon collisions: ON":L"Weapon collisions: OFF"):amalur::developer::panelLabel(i);
                text(44,y,label);
            }
            SetTextColor(dc,RGB(159,177,195));
            const wchar_t* destinations[]={L"Give to inventory",L"Give + equip primary",L"Give + equip secondary",L"Equip existing primary",L"Equip existing secondary"};
            text(42,1045,s.developerRow==VrSettings::attackPanelRow?L"A / trigger / left-right: show or hide attack readout":s.developerRow>=VrSettings::ablationFirstRow?L"Try one experiment at a time; reset before the next.":s.developerRow==VrSettings::mapPanelRow?L"Paused-screen prototype: enable, close this panel, open Map.":s.developerRow>=amalur::developer::panelRows+2?L"A / trigger / left-right: toggle (both can be ON)":s.developerRow==amalur::developer::panelRows+1?L"A / trigger / left-right: toggle third-person camera":s.developerRow==amalur::developer::panelRows?L"A / trigger / left-right: toggle collision overlay":s.developerRow>=2&&s.developerRow<amalur::developer::rows?std::wstring(L"Weapon destination: ")+destinations[s.developerDestination]:L"Single action - destination applies to weapon rows only");
            text(42,1090,VrSettings::meleeDebug.enabled()?L"Weapon collisions: ON (stays on when switching weapons)":L"Weapon collisions: OFF");
            SetTextColor(dc,RGB(116,194,217));
            RECT statusRect{42,1125,width-42,1265};
            DrawTextW(dc,developer->status.c_str(),-1,&statusRect,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
        }else{
        text(42,28,L"[ 1 Settings ]    |    2 Dev cheats");SelectObject(dc,font);SetTextColor(dc,RGB(116,194,217));text(42,90,L"Both sticks: open / close    X: switch tab    B: close");
        text(42,130,L"Hold stick up/down: scroll    Left/right: adjust    Y: reset");
        wchar_t line[256];swprintf_s(line,L"Headset IPD: %.1f mm (runtime)   |   %s",ipd,s.interfaceView?L"Interface View":(tracking?L"6DoF active":L"Menu view - F10 enables tracking"));SetTextColor(dc,RGB(200,208,220));text(42,195,line);
        swprintf_s(line,L"Source / eye: %u x %u     XR / eye: %u x %u",sourceWidth,sourceHeight,eyeWidth,eyeHeight);text(42,234,line);
        const wchar_t* labels[]={L"Play mode",L"HUD size",L"Stereo depth strength",L"Convergence (game units)",L"Infinity alignment at 20% depth",L"World units per metre",L"Game horizontal FOV",L"Headset output scale",L"Sharpening",L"Reverse source eyes",L"Hand grip pitch (degrees)",L"Hand grip yaw (degrees)",L"Hand grip roll (degrees)",L"Interface View (Ctrl + I)",L"Fullscreen menu size",L"Dagger outward (+) / inward (-), cm",L"Dagger forward (+) / back (-), cm",L"Dagger up (+) / down (-), cm",L"Physical crouching (30 cm below recenter)",L"Seated mode (recenter at current height)",L"Heavy charge input",L"Head height (cm)",L"Arm thickness (%)",L"Realtime cutscenes",L"Wrist HUD",L"Camera forward / back (cm)",L"Game resolution",L"Skip menu / automatic Continue",L"Open pause menu (A / right trigger)",L"Recenter view (A / right trigger)"};
        float values[]={amalur::playMode.normal()?1.f:0.f,s.hudSize*100,s.depth,s.convergence,s.alignment*100,s.scale,s.fov,s.renderScale*100,s.sharpness*100,s.swap?1.f:0.f,s.gripPitch,s.gripYaw,s.gripRoll,s.interfaceView?1.f:0.f,s.interfaceScale*100,s.weaponX,s.weaponY,s.weaponZ,s.physicalCrouch?1.f:0.f,s.seatedMode?1.f:0.f,float(s.heavyChargeMode),float(s.headHeightCm),float(s.armThicknessPercent),s.cinematicFullVR?1.f:0.f,float(s.wristHud),float(s.cameraDepthCm),float(s.sourceResolutionPercent),s.menuRecovery.automaticContinue?1.f:0.f,0,0};
        for(int i=0;i<VrSettings::rowCount;++i){int y=282+i*26;if(i==s.selected){RECT row{24,y-6,width-24,y+28};HBRUSH h=CreateSolidBrush(RGB(34,67,88));FillRect(dc,&row,h);DeleteObject(h);}SetTextColor(dc,RGB(227,235,244));text(44,y,labels[i]);if(i==VrSettings::sourceResolutionRow)swprintf_s(line,L"%d%%",s.sourceResolutionPercent);else if(i==VrSettings::wristHudRow)swprintf_s(line,L"%s",s.wristHud==1?L"Left":s.wristHud==2?L"Right":L"Off");else if(i==VrSettings::cinematicModeRow)swprintf_s(line,L"%s",s.cinematicFullVR?L"Full VR":L"Window");else if(i==VrSettings::heavyChargeRow)swprintf_s(line,L"%s",s.heavyChargeMode?L"Right grip":L"Position");else if(i==VrSettings::rowCount-3)swprintf_s(line,L"%s",s.menuRecovery.automaticContinue?L"ON":L"OFF");else if(i==0)swprintf_s(line,L"%s",amalur::playMode.normal()?L"Third person":L"First person");else if(i==9||i==13||i==18||i==19)swprintf_s(line,L"%s",(i==18&&s.seatedMode)?L"Paused (seated)":(values[i]>0?L"ON":L"OFF"));else swprintf_s(line,(i==1||i==14)?L"%.0f%%":L"%.2f",values[i]);text(i==0?790:850,y,i>=VrSettings::rowCount-2?L"Run":line);}
        static_assert(sizeof(labels)/sizeof(labels[0])==VrSettings::rowCount);
        static_assert(sizeof(values)/sizeof(values[0])==VrSettings::rowCount);
        // Keyboard-operated slider, matching the panel's existing arrow controls.
        RECT track{360,332,780,336};HBRUSH rail=CreateSolidBrush(RGB(73,91,110));FillRect(dc,&track,rail);DeleteObject(rail);
        int knob=360+static_cast<int>((s.hudSize-.4f)/.8f*420);RECT thumb{knob-5,329,knob+5,339};HBRUSH accent=CreateSolidBrush(RGB(116,194,217));FillRect(dc,&thumb,accent);DeleteObject(accent);
        RECT interfaceTrack{360,670,780,674};HBRUSH interfaceRail=CreateSolidBrush(RGB(73,91,110));FillRect(dc,&interfaceTrack,interfaceRail);DeleteObject(interfaceRail);
        int interfaceKnob=360+static_cast<int>((s.interfaceScale-.5f)*420);RECT interfaceThumb{interfaceKnob-5,667,interfaceKnob+5,677};HBRUSH interfaceAccent=CreateSolidBrush(RGB(116,194,217));FillRect(dc,&interfaceThumb,interfaceAccent);DeleteObject(interfaceAccent);
        if(s.selected==VrSettings::sourceResolutionRow){
            swprintf_s(line,L"Requested per eye: %d x %d",3840*s.sourceResolutionPercent/100,2400*s.sourceResolutionPercent/100);text(42,1090,line);
            text(42,1132,L"Applies after adjustment stops; the image may briefly pause.");
            text(42,1174,L"Higher values add game detail and increase GPU load.");
            text(42,1210,L"Left / right: 10%     Y: reset to 3840 x 2400");
        }else if(s.selected==VrSettings::cameraDepthRow){
            SetTextColor(dc,RGB(227,235,244));text(64,1090,L"Backward");text(940,1090,L"Forward");
            RECT depthRail{220,1125,880,1131};HBRUSH dr=CreateSolidBrush(RGB(73,91,110));FillRect(dc,&depthRail,dr);DeleteObject(dr);
            int dx=220+(s.cameraDepthCm+100)*660/200;RECT depthThumb{dx-7,1117,dx+7,1139};HBRUSH dt=CreateSolidBrush(RGB(116,194,217));FillRect(dc,&depthThumb,dt);DeleteObject(dt);
            text(190,1160,L"-100 cm");text(524,1160,L"0");text(842,1160,L"+100 cm");
            text(64,1210,L"Left / right: 1 cm     Y: reset     Camera position on body");
        }else if(s.selected==VrSettings::armThicknessRow){
            SetTextColor(dc,RGB(227,235,244));text(64,1090,L"Slimmer");text(952,1090,L"Thicker");
            RECT rail{220,1125,880,1131};HBRUSH rh=CreateSolidBrush(RGB(73,91,110));FillRect(dc,&rail,rh);DeleteObject(rh);
            int x=220+(s.armThicknessPercent-50)*660/100;RECT thumb{x-7,1117,x+7,1139};HBRUSH th=CreateSolidBrush(RGB(116,194,217));FillRect(dc,&thumb,th);DeleteObject(th);
            text(205,1160,L"50%");text(518,1160,L"100%");text(848,1160,L"150%");
            text(64,1210,L"Left / right: 5%    Y: reset    VR arms; hand size and reach unchanged");
        }else if(s.selected==VrSettings::headHeightRow){
            SetTextColor(dc,RGB(227,235,244));
            text(64,1090,L"Lower");text(952,1090,L"Higher");
            RECT heightRail{220,1125,880,1131};HBRUSH hr=CreateSolidBrush(RGB(73,91,110));FillRect(dc,&heightRail,hr);DeleteObject(hr);
            const int hx=220+(s.headHeightCm+100)*660/200;
            RECT heightThumb{hx-7,1117,hx+7,1139};HBRUSH ht=CreateSolidBrush(RGB(116,194,217));FillRect(dc,&heightThumb,ht);DeleteObject(ht);
            text(190,1160,L"-100 cm");text(524,1160,L"0");text(842,1160,L"+100 cm");
            text(64,1210,L"Left / right: 1 cm     Y: reset     First-person head and hands");
        }else if(s.menuRecovery.status.empty()){
        SetTextColor(dc,RGB(159,177,195));text(42,1078,L"Dagger offsets follow each wrist; outward is mirrored for the left hand.");
        text(42,1112,L"Depth strength is not calibrated IPD. Hardware IPD uses Quest's dial.");
        swprintf_s(line,L"Stereo control: %s   |   F7 recenter   F10 tracking   F12 exit",stereoStatus==0?L"connected":L"waiting / unavailable");text(42,1153,line);
        text(42,1194,L"Game resolution changes source detail; headset scale changes output.");
        }else{RECT menuStatus{42,1078,width-42,1285};DrawTextW(dc,s.menuRecovery.status.c_str(),-1,&menuStatus,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);}
        }
        GdiFlush();auto in=static_cast<unsigned char*>(bits);bool bgra=format==DXGI_FORMAT_B8G8R8A8_UNORM||format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        for(size_t j=0;j<pixels.size();j+=4){pixels[j]=in[j+(bgra?0:2)];pixels[j+1]=in[j+1];pixels[j+2]=in[j+(bgra?2:0)];pixels[j+3]=200;}
        uint32_t index{};XrSwapchainImageAcquireInfo a{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};XR(xrAcquireSwapchainImage(chain,&a,&index));XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=XR_INFINITE_DURATION;XR(xrWaitSwapchainImage(chain,&wait));context->UpdateSubresource(images[index].texture,0,nullptr,pixels.data(),width*4,0);XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};XR(xrReleaseSwapchainImage(chain,&release));
        XrCompositionLayerQuad layer{XR_TYPE_COMPOSITION_LAYER_QUAD};layer.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;layer.space=view;layer.eyeVisibility=XR_EYE_VISIBILITY_BOTH;layer.subImage.swapchain=chain;layer.subImage.imageRect.extent={width,height};layer.pose.orientation.w=1;layer.pose.position.y=-.55f;layer.pose.position.z=-1.4f;layer.size={.825f,.9675f};return layer;
    }
};
