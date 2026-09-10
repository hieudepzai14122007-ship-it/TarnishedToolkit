#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "runtime.hpp"
DWORD WINAPI toolkitThread(void*){
    // Pin the module: hook callbacks and worker must never outlive their DLL.
    HMODULE pinned{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&toolkitThread),&pinned);
    // Arm graphics before hashing the executable, so early swapchain creation is caught.
    tt::installOverlay();tt::initialize();
    while(true){tt::poll();Sleep(64);}return 0;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);HANDLE thread=CreateThread(nullptr,0,toolkitThread,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
    return TRUE;
}
