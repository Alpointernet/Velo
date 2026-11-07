#include "shortcuts.h"

namespace {
    HHOOK keyboardHook = NULL;
    bool interceptedCloseAll = false;

    LRESULT CALLBACK AppKeyboardHook(int code, WPARAM message, LPARAM parameter) {
        if (code == HC_ACTION) {
            const auto* key = (const KBDLLHOOKSTRUCT*)parameter;
            if (key->vkCode == 'W') {
                bool keyDown = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
                bool keyUp = message == WM_KEYUP || message == WM_SYSKEYUP;
                if (keyUp && interceptedCloseAll) {
                    interceptedCloseAll = false;
                    return 1;
                }
                if (keyDown && interceptedCloseAll) return 1;

                // A registered system hotkey can consume this combination before WM_KEYDOWN.
                // Handle it before that happens, strictly while this Velo window is active.
                if (keyDown && !(key->flags & LLKHF_ALTDOWN) &&
                    (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
                    HWND foreground = GetForegroundWindow();
                    if (hwndMain && IsWindowEnabled(hwndMain) &&
                        GetAncestor(foreground, GA_ROOTOWNER) == hwndMain &&
                        PostMessageW(hwndMain, WM_COMMAND, IDM_FILE_CLOSE_ALL_TABS, 0)) {
                        interceptedCloseAll = true;
                        return 1;
                    }
                }
            }
        }
        return CallNextHookEx(keyboardHook, code, message, parameter);
    }
}

HACCEL CreateAppAccelerators() {
    ACCEL shortcuts[] = {
        {FVIRTKEY | FCONTROL | FSHIFT, 'W', IDM_FILE_CLOSE_ALL_TABS},
        {FVIRTKEY | FCONTROL, 'W', IDM_FILE_CLOSE_TAB}
    };
    return CreateAcceleratorTableW(shortcuts, sizeof(shortcuts) / sizeof(shortcuts[0]));
}

void InitializeAppShortcuts() {
    if (!keyboardHook) keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, AppKeyboardHook, GetModuleHandleW(NULL), 0);
}

void ShutdownAppShortcuts() {
    if (keyboardHook) UnhookWindowsHookEx(keyboardHook);
    keyboardHook = NULL;
    interceptedCloseAll = false;
}
