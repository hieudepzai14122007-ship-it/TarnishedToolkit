#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800
#include "input.hpp"
#include "input_policy.hpp"
#include "runtime.hpp"
#include "MinHook.h"
#include <dinput.h>
#include <atomic>
#include <mutex>
#include <array>
#include <cstring>

namespace tt::input {
namespace {
using SetPos=BOOL(WINAPI*)(int,int);
using Clip=BOOL(WINAPI*)(const RECT*);
using Raw=UINT(WINAPI*)(HRAWINPUT,UINT,LPVOID,PUINT,UINT);
using DeviceState=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*,DWORD,LPVOID);
using DeviceData=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD);
SetPos originalPos{},originalPhysicalPos{}; Clip originalClip{}; Raw originalRaw{};
DeviceState originalState{};DeviceData originalData{};
std::atomic<HWND> gameWindow{};
std::atomic_bool captured{};
std::mutex policyMutex;
InputPolicy policy;
RECT previousClip{};bool previousValid{};
bool foreground(){HWND h=gameWindow.load();return h && GetForegroundWindow()==h;}
BOOL WINAPI setPosHook(int x,int y){return ownsFocus()?TRUE:originalPos(x,y);}
BOOL WINAPI setPhysicalHook(int x,int y){return ownsFocus()?TRUE:originalPhysicalPos(x,y);}
BOOL WINAPI clipHook(const RECT* rect){
    if(ownsFocus())return TRUE;
    return originalClip(rect);
}
UINT WINAPI rawHook(HRAWINPUT handle,UINT command,LPVOID buffer,PUINT size,UINT headerSize){
    UINT result=originalRaw(handle,command,buffer,size,headerSize);
    if(result!=UINT(-1) && command==RID_INPUT && buffer && result>=sizeof(RAWINPUTHEADER) && ownsFocus()){
        auto* raw=static_cast<RAWINPUT*>(buffer);
        if(raw->header.dwType==RIM_TYPEMOUSE && result>=offsetof(RAWINPUT,data)+sizeof(RAWMOUSE))
            std::memset(&raw->data.mouse,0,sizeof(raw->data.mouse));
        else if(raw->header.dwType==RIM_TYPEKEYBOARD && result>=offsetof(RAWINPUT,data)+sizeof(RAWKEYBOARD)){
            raw->data.keyboard.MakeCode=0;raw->data.keyboard.VKey=0;raw->data.keyboard.Flags=RI_KEY_BREAK;
        }
    }
    return result;
}
HRESULT STDMETHODCALLTYPE stateHook(IDirectInputDevice8W* self,DWORD size,LPVOID buffer){
    HRESULT result=originalState(self,size,buffer);
    if(SUCCEEDED(result) && buffer && ownsFocus() && (size==256 || size==sizeof(DIMOUSESTATE) || size==sizeof(DIMOUSESTATE2)))std::memset(buffer,0,size);
    return result;
}
HRESULT STDMETHODCALLTYPE dataHook(IDirectInputDevice8W* self,DWORD size,LPDIDEVICEOBJECTDATA buffer,LPDWORD count,DWORD flags){
    HRESULT result=originalData(self,size,buffer,count,flags);
    if(SUCCEEDED(result) && count && ownsFocus())*count=0;
    return result;
}
bool hook(void* address,void* replacement,void** original){
    if(!address || MH_CreateHook(address,replacement,original)!=MH_OK)return false;
    if(MH_EnableHook(address)!=MH_OK){MH_RemoveHook(address);return false;}
    return true;
}
}
bool ownsFocus(){return captured.load(std::memory_order_relaxed) && foreground();}
bool gameplayFocused(){return foreground();}
void update(HWND window,bool menu){
    std::lock_guard lock(policyMutex);gameWindow=window;
    bool focused=foreground();auto transition=policy.update(menu,focused);
    if(transition==InputPolicy::Transition::Acquire){
        previousValid=GetClipCursor(&previousClip)!=FALSE;
        captured=true;
        if(originalClip)originalClip(nullptr);
    }else if(transition==InputPolicy::Transition::Release){
        captured=false;
        // Never re-confine the global cursor to a background window on Alt+Tab.
        // On ordinary close restore the existing clip; the game may replace it.
        if(originalClip)originalClip(focused && previousValid?&previousClip:nullptr);
        previousValid=false;
    }
}
void release(){update(gameWindow.load(),false);}
bool install(){
    auto user=GetModuleHandleW(L"user32.dll");
    bool pos=hook(reinterpret_cast<void*>(GetProcAddress(user,"SetCursorPos")),reinterpret_cast<void*>(setPosHook),reinterpret_cast<void**>(&originalPos));
    bool clip=hook(reinterpret_cast<void*>(GetProcAddress(user,"ClipCursor")),reinterpret_cast<void*>(clipHook),reinterpret_cast<void**>(&originalClip));
    auto physical=GetProcAddress(user,"SetPhysicalCursorPos");
    if(physical && reinterpret_cast<void*>(physical)!=reinterpret_cast<void*>(GetProcAddress(user,"SetCursorPos")))
        hook(reinterpret_cast<void*>(physical),reinterpret_cast<void*>(setPhysicalHook),reinterpret_cast<void**>(&originalPhysicalPos));
    hook(reinterpret_cast<void*>(GetProcAddress(user,"GetRawInputData")),reinterpret_cast<void*>(rawHook),reinterpret_cast<void**>(&originalRaw));
    // Obtain the standard DirectInput device implementations without acquiring
    // hardware. Never change the game's cooperative level or device ownership.
    IDirectInput8W* di{};IDirectInputDevice8W* mouse{};
    if(SUCCEEDED(DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,reinterpret_cast<void**>(&di),nullptr))){
        if(SUCCEEDED(di->CreateDevice(GUID_SysMouse,&mouse,nullptr))){
            auto vtable=*reinterpret_cast<void***>(mouse);
            hook(vtable[9],reinterpret_cast<void*>(stateHook),reinterpret_cast<void**>(&originalState));
            hook(vtable[10],reinterpret_cast<void*>(dataHook),reinterpret_cast<void**>(&originalData));
            mouse->Release();
        }
        di->Release();
    }
    log(pos&&clip?"Cursor recenter and confinement hooks installed.":"Cursor hooks incomplete; consult diagnostics before testing mouse capture.");
    return pos&&clip;
}
}
