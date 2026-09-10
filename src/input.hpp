#pragma once
#include <windows.h>
namespace tt::input {
bool install();
void update(HWND window, bool menuVisible);
void release();
bool ownsFocus();
bool gameplayFocused();
}
