#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <xinput.h>
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "runtime.hpp"
#include "input.hpp"
#include "input_policy.hpp"
#include <mutex>
#include <vector>
#include <deque>
#include <atomic>
#include <algorithm>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace tt {
using Microsoft::WRL::ComPtr;
namespace {
using CreateFn=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
using CreateHwndFn=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**);
using PresentFn=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using ResizeFn=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
using XInputFn=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
CreateFn originalCreate{};CreateHwndFn originalCreateHwnd{};
PresentFn originalPresent{};ResizeFn originalResize{};
XInputFn originalXInput{};
std::recursive_mutex renderMutex;
std::mutex eventMutex;
struct Event{HWND h;UINT m;WPARAM w;LPARAM l;};std::deque<Event> events;
struct Frame{ComPtr<ID3D12Resource> resource;ComPtr<ID3D12CommandAllocator> allocator;D3D12_CPU_DESCRIPTOR_HANDLE rtv{};UINT64 fence{};};
std::vector<Frame> frames;
ComPtr<ID3D12Device> device;
ComPtr<ID3D12CommandQueue> queue;
ComPtr<IDXGISwapChain3> swapchain;
ComPtr<ID3D12DescriptorHeap> rtvHeap,srvHeap;
ComPtr<ID3D12GraphicsCommandList> commandList;
ComPtr<ID3D12Fence> fence;
HANDLE fenceEvent{}; UINT64 nextFence{};
HWND gameWindow{};WNDPROC oldProc{};
ImGuiContext* context{}; bool initialized{},failed{};
std::atomic_bool captureInput{};
bool ownedFocus(){return input::ownsFocus();}
DWORD WINAPI xinputHook(DWORD index,XINPUT_STATE* output) {
    auto result=originalXInput(index,output);
    if(result==ERROR_SUCCESS && output && ownedFocus())ZeroMemory(&output->Gamepad,sizeof(output->Gamepad));
    return result;
}
LRESULT CALLBACK wndproc(HWND h,UINT m,WPARAM w,LPARAM l) {
    if(m==WM_KEYDOWN || m==WM_SYSKEYDOWN){
        if(w==static_cast<WPARAM>(menuKey.load()) && !(l&(1LL<<30))){menuOpen=!menuOpen;captureInput=menuOpen.load();input::update(h,menuOpen);return 0;}
        if(w==VK_BACK && (GetKeyState(VK_CONTROL)&0x8000) && (GetKeyState(VK_SHIFT)&0x8000)){disableAll();return 0;}
    }
    if(m==WM_KILLFOCUS || (m==WM_ACTIVATEAPP && !w)){menuOpen=false;captureInput=false;input::release();}
    {std::lock_guard lock(eventMutex);if(events.size()<512)events.push_back({h,m,w,l});}
    if(ownedFocus()){
        if((m>=WM_KEYFIRST && m<=WM_KEYLAST) && m!=WM_SYSKEYDOWN && m!=WM_SYSKEYUP)return 0;
        if(m>=WM_MOUSEFIRST && m<=WM_MOUSELAST)return 0;
        if(m==WM_INPUT)return DefWindowProcW(h,m,w,l);
    }
    return CallWindowProcW(oldProc,h,m,w,l);
}
bool waitFence(UINT64 value) {
    if(!value || fence->GetCompletedValue()>=value)return true;
    if(FAILED(fence->SetEventOnCompletion(value,fenceEvent)))return false;
    // Bounded waits prevent trapping the game after device loss.
    return WaitForSingleObject(fenceEvent,2000)==WAIT_OBJECT_0;
}
bool synchronize() {
    if(!queue || !fence)return true;
    const auto value=++nextFence;
    return SUCCEEDED(queue->Signal(fence.Get(),value)) && waitFence(value);
}
bool createFrames() {
    DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(swapchain->GetDesc(&desc)) || desc.BufferCount<2 || desc.BufferCount>8)return false;
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=desc.BufferCount;
    if(FAILED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtvHeap))))return false;
    auto handle=rtvHeap->GetCPUDescriptorHandleForHeapStart();auto stride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    frames.resize(desc.BufferCount);
    for(UINT i=0;i<desc.BufferCount;++i){auto& f=frames[i];
        if(FAILED(swapchain->GetBuffer(i,IID_PPV_ARGS(&f.resource))) || FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&f.allocator))))return false;
        f.rtv=handle;device->CreateRenderTargetView(f.resource.Get(),nullptr,handle);handle.ptr+=stride;
    }
    return true;
}
bool initializeRenderer() {
    if(FAILED(swapchain->GetDevice(IID_PPV_ARGS(&device))))return false;
    ComPtr<ID3D12Device> queueDevice;if(FAILED(queue->GetDevice(IID_PPV_ARGS(&queueDevice))) || queueDevice.Get()!=device.Get())return false;
    DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(swapchain->GetDesc(&desc)))return false;gameWindow=desc.OutputWindow;
    if(!createFrames())return false;
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if(FAILED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srvHeap))))return false;
    if(FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,frames[0].allocator.Get(),nullptr,IID_PPV_ARGS(&commandList))))return false;
    if(FAILED(commandList->Close()) || FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))return false;
    fenceEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!fenceEvent)return false;
    context=ImGui::CreateContext();ImGui::SetCurrentContext(context);
    auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad;io.IniFilename=nullptr;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf",18.f);
    if(io.Fonts->Fonts.empty())io.Fonts->AddFontDefault();
    if(!ImGui_ImplWin32_Init(gameWindow))return false;
    ImGui_ImplDX12_InitInfo info;info.Device=device.Get();info.CommandQueue=queue.Get();info.NumFramesInFlight=frames.size();info.RTVFormat=desc.BufferDesc.Format;info.SrvDescriptorHeap=srvHeap.Get();
    info.SrvDescriptorAllocFn=[](ImGui_ImplDX12_InitInfo* i,D3D12_CPU_DESCRIPTOR_HANDLE* cpu,D3D12_GPU_DESCRIPTOR_HANDLE* gpu){*cpu=i->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();*gpu=i->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();};
    info.SrvDescriptorFreeFn=[](ImGui_ImplDX12_InitInfo*,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_GPU_DESCRIPTOR_HANDLE){};
    if(!ImGui_ImplDX12_Init(&info))return false;
    SetLastError(0);auto previous=SetWindowLongPtrW(gameWindow,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(wndproc));
    if(!previous)return false;oldProc=reinterpret_cast<WNDPROC>(previous);
    initialized=true;log("D3D12 overlay initialized on game's exact swapchain and command queue.");return true;
}
void gamepad() {
    if(!originalXInput)return;
    XINPUT_STATE state{};bool found=false;for(DWORD i=0;i<4;++i)if(originalXInput(i,&state)==ERROR_SUCCESS){found=true;break;}
    static HoldActivation opening;static int previousChord{};
    int chord=settings().controllerChord;
    if(chord!=previousChord){opening={};previousChord=chord;}
    WORD mask=chord==1?(XINPUT_GAMEPAD_START|XINPUT_GAMEPAD_BACK):chord==2?(XINPUT_GAMEPAD_START|XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER):0;
    bool held=found && mask && state.Gamepad.wButtons==mask;
    if(opening.update(held,GetForegroundWindow()==gameWindow,GetTickCount64())){menuOpen=!menuOpen;input::update(gameWindow,menuOpen);}
    if(held)ZeroMemory(&state.Gamepad,sizeof(state.Gamepad));
    auto& io=ImGui::GetIO();if(!found || !ownedFocus()){
        io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;
        for(int key=ImGuiKey_GamepadStart;key<=ImGuiKey_GamepadRStickDown;++key)io.AddKeyAnalogEvent(static_cast<ImGuiKey>(key),false,0.f);
        return;
    }
    io.BackendFlags|=ImGuiBackendFlags_HasGamepad;
    auto button=[&](ImGuiKey key,WORD bit){io.AddKeyEvent(key,(state.Gamepad.wButtons&bit)!=0);};
    button(ImGuiKey_GamepadFaceDown,XINPUT_GAMEPAD_A);button(ImGuiKey_GamepadFaceRight,XINPUT_GAMEPAD_B);
    button(ImGuiKey_GamepadDpadUp,XINPUT_GAMEPAD_DPAD_UP);button(ImGuiKey_GamepadDpadDown,XINPUT_GAMEPAD_DPAD_DOWN);
    button(ImGuiKey_GamepadDpadLeft,XINPUT_GAMEPAD_DPAD_LEFT);button(ImGuiKey_GamepadDpadRight,XINPUT_GAMEPAD_DPAD_RIGHT);
    button(ImGuiKey_GamepadL1,XINPUT_GAMEPAD_LEFT_SHOULDER);button(ImGuiKey_GamepadR1,XINPUT_GAMEPAD_RIGHT_SHOULDER);
    auto axis=[&](ImGuiKey key,float value){float v=std::clamp((value-8000.f)/(32767.f-8000.f),0.f,1.f);io.AddKeyAnalogEvent(key,v>0,v);};
    axis(ImGuiKey_GamepadLStickLeft,-float(state.Gamepad.sThumbLX));axis(ImGuiKey_GamepadLStickRight,float(state.Gamepad.sThumbLX));
    axis(ImGuiKey_GamepadLStickUp,float(state.Gamepad.sThumbLY));axis(ImGuiKey_GamepadLStickDown,-float(state.Gamepad.sThumbLY));
}
HRESULT STDMETHODCALLTYPE presentHook(IDXGISwapChain* sc,UINT sync,UINT flags) {
    {
        std::lock_guard lock(renderMutex);
        ComPtr<IDXGISwapChain3> current;
        if(!failed && !(flags&DXGI_PRESENT_TEST) && SUCCEEDED(sc->QueryInterface(IID_PPV_ARGS(&current))) && current.Get()==swapchain.Get()){
            if(!initialized && !initializeRenderer()){failed=true;log("Overlay initialization failed; rendering disabled. Restart required.");}
            if(initialized && !failed){
                ImGui::SetCurrentContext(context);
                // ImGui can call SetCapture while processing mouse messages, which
                // re-enters WndProc. Never hold eventMutex across backend callbacks.
                std::deque<Event> pending;
                {std::lock_guard eventLock(eventMutex);pending.swap(events);}
                for(auto e:pending){ImGui_ImplWin32_WndProcHandler(e.h,e.m,e.w,e.l);if(e.m==WM_KILLFOCUS){ImGui::GetIO().ClearInputKeys();}}
                static bool wasMenuOpen=false;bool isOpen=menuOpen.load();
                if(wasMenuOpen!=isOpen){ImGui::GetIO().ClearInputKeys();ImGui::GetIO().ClearInputMouse();wasMenuOpen=isOpen;}
                captureInput=isOpen;input::update(gameWindow,isOpen);ImGui::GetIO().MouseDrawCursor=ownedFocus();
                ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();gamepad();ImGui::NewFrame();drawMenu();drawHud();ImGui::Render();
                auto index=swapchain->GetCurrentBackBufferIndex();
                if(index<frames.size() && ImGui::GetDrawData()->TotalVtxCount){auto& f=frames[index];
                    if(!waitFence(nextFence) || FAILED(f.allocator->Reset()) || FAILED(commandList->Reset(f.allocator.Get(),nullptr))){failed=true;log("GPU synchronization failed; rendering disabled.");}
                    else{
                        D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=f.resource.Get();barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;
                        commandList->ResourceBarrier(1,&barrier);commandList->OMSetRenderTargets(1,&f.rtv,FALSE,nullptr);ID3D12DescriptorHeap* heaps[]{srvHeap.Get()};commandList->SetDescriptorHeaps(1,heaps);
                        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),commandList.Get());std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);commandList->ResourceBarrier(1,&barrier);
                        if(SUCCEEDED(commandList->Close())){ID3D12CommandList* lists[]{commandList.Get()};queue->ExecuteCommandLists(1,lists);f.fence=++nextFence;if(FAILED(queue->Signal(fence.Get(),f.fence)))failed=true;}else failed=true;
                    }
                }
                if(failed){captureInput=false;menuOpen=false;input::release();disableAll();}
            }
        }
    }
    return originalPresent(sc,sync,flags);
}
HRESULT STDMETHODCALLTYPE resizeHook(IDXGISwapChain* sc,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags) {
    std::lock_guard lock(renderMutex);ComPtr<IDXGISwapChain3> current;sc->QueryInterface(IID_PPV_ARGS(&current));
    if(!initialized || current.Get()!=swapchain.Get())return originalResize(sc,count,width,height,format,flags);
    if(!synchronize()){failed=true;captureInput=false;input::release();return DXGI_ERROR_DEVICE_REMOVED;}
    ImGui::SetCurrentContext(context);ImGui_ImplDX12_InvalidateDeviceObjects();frames.clear();rtvHeap.Reset();
    auto hr=originalResize(sc,count,width,height,format,flags);
    if(!createFrames()){failed=true;captureInput=false;input::release();log("Swapchain resize failed; restart required.");}
    else {ImGui_ImplDX12_CreateDeviceObjects();log("Swapchain buffers recreated after resize.");}
    return hr;
}
void attach(IUnknown* suppliedDevice,IDXGISwapChain* sc) {
    if(!sc)return;
    std::lock_guard lock(renderMutex);if(swapchain)return;
    ComPtr<ID3D12CommandQueue> exactQueue;
    if(FAILED(suppliedDevice->QueryInterface(IID_PPV_ARGS(&exactQueue))) || exactQueue->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT)return;
    DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(sc->GetDesc(&desc)))return;DWORD pid{};GetWindowThreadProcessId(desc.OutputWindow,&pid);if(pid!=GetCurrentProcessId())return;
    if(FAILED(sc->QueryInterface(IID_PPV_ARGS(&swapchain))))return;queue=exactQueue;
    void** methods=*reinterpret_cast<void***>(sc);
    if(MH_CreateHook(methods[8],reinterpret_cast<void*>(presentHook),reinterpret_cast<void**>(&originalPresent))!=MH_OK ||
       MH_CreateHook(methods[13],reinterpret_cast<void*>(resizeHook),reinterpret_cast<void**>(&originalResize))!=MH_OK){failed=true;log("Swapchain hooks failed.");return;}
    MH_QueueEnableHook(methods[8]);MH_QueueEnableHook(methods[13]);
    if(MH_ApplyQueued()!=MH_OK){failed=true;log("Swapchain hook activation failed.");return;}
    log("Captured D3D12 swapchain creation and its command queue.");
}
HRESULT STDMETHODCALLTYPE createHook(IDXGIFactory* factory,IUnknown* dev,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** out){auto hr=originalCreate(factory,dev,desc,out);if(SUCCEEDED(hr))attach(dev,*out);return hr;}
HRESULT STDMETHODCALLTYPE createHwndHook(IDXGIFactory2* factory,IUnknown* dev,HWND hwnd,const DXGI_SWAP_CHAIN_DESC1* desc,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full,IDXGIOutput* restrictTo,IDXGISwapChain1** out){auto hr=originalCreateHwnd(factory,dev,hwnd,desc,full,restrictTo,out);if(SUCCEEDED(hr))attach(dev,*out);return hr;}
}
bool installOverlay(){
    if(MH_Initialize()!=MH_OK){log("MinHook initialization failed.");return false;}
    input::install();
    ComPtr<IDXGIFactory2> factory;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))){log("DXGI factory unavailable.");return false;}
    void** methods=*reinterpret_cast<void***>(factory.Get());
    if(MH_CreateHook(methods[10],reinterpret_cast<void*>(createHook),reinterpret_cast<void**>(&originalCreate))!=MH_OK ||
       MH_CreateHook(methods[15],reinterpret_cast<void*>(createHwndHook),reinterpret_cast<void**>(&originalCreateHwnd))!=MH_OK){log("DXGI factory hooks failed.");return false;}
    MH_QueueEnableHook(methods[10]);MH_QueueEnableHook(methods[15]);
    if(MH_ApplyQueued()!=MH_OK){log("DXGI factory hook activation failed.");return false;}
    // Only intercept the game's process-local XInput function. UI calls use the original.
    auto module=LoadLibraryW(L"xinput1_4.dll");auto input=module?GetProcAddress(module,"XInputGetState"):nullptr;
    if(input && MH_CreateHook(reinterpret_cast<void*>(input),reinterpret_cast<void*>(xinputHook),reinterpret_cast<void**>(&originalXInput))==MH_OK)MH_EnableHook(reinterpret_cast<void*>(input));
    log("Overlay armed. DLL must load before the game's first swapchain creation.");return true;
}
}
