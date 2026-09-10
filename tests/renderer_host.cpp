// This is a graphics integration test, never a gameplay simulator.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <filesystem>
#include <cstdio>
using Microsoft::WRL::ComPtr;
LRESULT CALLBACK windowProc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR,int){
 SetProcessDPIAware();
 wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);auto dll=std::filesystem::path(exe).parent_path()/L"TarnishedToolkit.dll";
 if(!LoadLibraryW(dll.c_str()))return 10;
 Sleep(1500);
 WNDCLASSW wc{};wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.lpszClassName=L"ToolkitRendererTest";RegisterClassW(&wc);
 HWND window=CreateWindowW(wc.lpszClassName,L"RENDERER TEST ONLY - not Elden Ring",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1200,800,nullptr,nullptr,instance,nullptr);
 ComPtr<IDXGIFactory4> factory;ComPtr<ID3D12Device> device;ComPtr<ID3D12CommandQueue> queue;ComPtr<IDXGISwapChain1> first;ComPtr<IDXGISwapChain3> swap;
 if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) || FAILED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))))return 11;
 D3D12_COMMAND_QUEUE_DESC qd{};if(FAILED(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue))))return 12;
 DXGI_SWAP_CHAIN_DESC1 sd{};sd.Width=1200;sd.Height=800;sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=2;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
 if(FAILED(factory->CreateSwapChainForHwnd(queue.Get(),window,&sd,nullptr,nullptr,&first)) || FAILED(first.As(&swap)))return 13;
 factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER);
 ComPtr<ID3D12DescriptorHeap> heap;D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=2;if(FAILED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap))))return 14;
 ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;
 device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator));device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list));list->Close();device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence));
 HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);UINT64 fenceValue=0;
 ShowWindow(window,SW_SHOW);UpdateWindow(window);
 MSG msg{};bool quit=false;
 while(!quit){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){quit=true;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}if(quit)break;
   if(fenceValue && fence->GetCompletedValue()<fenceValue){fence->SetEventOnCompletion(fenceValue,event);if(WaitForSingleObject(event,2000)!=WAIT_OBJECT_0)return 15;}
   RECT rect{};GetClientRect(window,&rect);UINT width=rect.right,height=rect.bottom;
   if(!width || !height){Sleep(25);continue;}
   if(width!=sd.Width || height!=sd.Height){if(FAILED(swap->ResizeBuffers(2,width,height,sd.Format,0)))return 16;sd.Width=width;sd.Height=height;}
   UINT index=swap->GetCurrentBackBufferIndex();ComPtr<ID3D12Resource> buffer;swap->GetBuffer(index,IID_PPV_ARGS(&buffer));
   auto rtv=heap->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=index*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);device->CreateRenderTargetView(buffer.Get(),nullptr,rtv);
   allocator->Reset();list->Reset(allocator.Get(),nullptr);D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=buffer.Get();b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;b.Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT;b.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;list->ResourceBarrier(1,&b);
   float color[]{.045f,.06f,.075f,1};list->ClearRenderTargetView(rtv,color,0,nullptr);b.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET;b.Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT;list->ResourceBarrier(1,&b);list->Close();ID3D12CommandList* lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);
   if(FAILED(swap->Present(1,0)))return 17;
   queue->Signal(fence.Get(),++fenceValue);
 }
 CloseHandle(event);return 0;
}
