#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdio>
using Microsoft::WRL::ComPtr;
static bool check(HRESULT result,const char* operation){if(FAILED(result)){printf("FAIL %s: 0x%08lx\n",operation,static_cast<unsigned long>(result));return false;}return true;}
int main() {
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"AmalurDiagnosticHarness";
    RegisterClassW(&wc);HWND window=CreateWindowW(wc.lpszClassName,L"Amalur diagnostic verification",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,wc.hInstance,nullptr);
    if(!window)return 1; // Hidden desktop test, no headset or visible window needed.
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    if(!check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context),"device"))return 1;
    ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory> factory;
    if(!check(device.As(&dxgi),"dxgi")||!check(dxgi->GetAdapter(&adapter),"adapter")||!check(adapter->GetParent(IID_PPV_ARGS(&factory)),"factory"))return 1;
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=640;desc.BufferDesc.Height=480;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;desc.OutputWindow=window;desc.Windowed=TRUE;
    ComPtr<IDXGISwapChain> chain;if(!check(factory->CreateSwapChain(device.Get(),&desc,&chain),"swapchain"))return 1;
    for(int frame=0;frame<12;++frame){
        if(frame==6&&!check(chain->ResizeBuffers(1,800,600,DXGI_FORMAT_UNKNOWN,0),"resize"))return 1;
        ComPtr<ID3D11Texture2D> back;ComPtr<ID3D11RenderTargetView> target;
        if(!check(chain->GetBuffer(0,IID_PPV_ARGS(&back)),"buffer")||!check(device->CreateRenderTargetView(back.Get(),nullptr,&target),"rtv"))return 1;
        const float color[]={.05f,.15f,.25f,1};context->ClearRenderTargetView(target.Get(),color);
        if(!check(chain->Present(0,0),"present"))return 1;
    }
    chain.Reset();context->ClearState();context->Flush();DestroyWindow(window);
    puts("PASS: device, swapchain, 12 presentations, resize, cleanup. Check diagnostic log for hook coverage.");return 0;
}
