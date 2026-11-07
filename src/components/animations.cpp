#include "../globals.h"
#include "animations.h"
#include "tabmanager.h"
#include "ui_drawing.h"
#include "editor.h"
#include <cmath>

namespace {
    struct WindowAnimation {
        HWND hwnd;
        RECT target;
        int offsetY;
        float durationMs;
        LARGE_INTEGER startQPC;
    };
    std::vector<WindowAnimation> openingWindows;

    struct FrameClock {
        HWND hwnd = NULL;
        HANDLE thread = NULL, stopEvent = NULL, timer = NULL;
        LARGE_INTEGER frequency = {};
        LONGLONG frameTicks = 0;
        volatile LONG framePending = 0;
        bool timerResolutionRequested = false;
        bool fallbackTimer = false;
    } frameClock;

    LARGE_INTEGER GetFrequency() {
        static LARGE_INTEGER frequency = []() {
            LARGE_INTEGER value;
            QueryPerformanceFrequency(&value);
            return value;
        }();
        return frequency;
    }

    bool HasActiveAnimations() {
        if (!openingWindows.empty()) return true;
        if (zenTopProgress != zenTopAnimTargetProgress || zenBottomProgress != zenBottomAnimTargetProgress) return true;
        for (const auto& tab : tabs) {
            if (tab.isOpening || tab.isClosing) return true;
        }
        return false;
    }

    LONGLONG GetFrameTicks(HWND hwnd, LARGE_INTEGER frequency) {
        MONITORINFOEXW monitor = {};
        monitor.cbSize = sizeof(monitor);
        DEVMODEW mode = {};
        mode.dmSize = sizeof(mode);
        DWORD refreshRate = 60;
        if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor) &&
            EnumDisplaySettingsW(monitor.szDevice, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency >= 30) {
            refreshRate = mode.dmDisplayFrequency;
        }
        return max(1LL, frequency.QuadPart / refreshRate);
    }

    DWORD WINAPI RunFrameClock(void*) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        LONGLONG nextFrame = now.QuadPart;
        HANDLE events[] = {frameClock.stopEvent, frameClock.timer};
        for (;;) {
            nextFrame += frameClock.frameTicks;
            QueryPerformanceCounter(&now);
            if (nextFrame <= now.QuadPart) nextFrame = now.QuadPart + frameClock.frameTicks;
            LARGE_INTEGER due;
            due.QuadPart = -max(1LL, (nextFrame - now.QuadPart) * 10000000LL / frameClock.frequency.QuadPart);
            if (!SetWaitableTimer(frameClock.timer, &due, 0, NULL, NULL, FALSE)) {
                PostMessageW(frameClock.hwnd, WM_APP_ANIMATION_FRAME, 1, 0);
                break;
            }
            if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) break;
            // At most one outstanding frame; a busy UI skips frames instead of building a backlog.
            if (InterlockedCompareExchange(&frameClock.framePending, 1, 0) == 0 &&
                !PostMessageW(frameClock.hwnd, WM_APP_ANIMATION_FRAME, 0, 0)) {
                InterlockedExchange(&frameClock.framePending, 0);
            }
        }
        return 0;
    }

    float SampleProgress(float from, float target, LARGE_INTEGER& start, LARGE_INTEGER now, float durationMs) {
        if (!enableAnimations) return target;
        if (start.QuadPart == 0) start = now;
        float distance = std::fabs(target - from);
        double duration = durationMs * max(0.35f, distance);
        double elapsed = (double)(now.QuadPart - start.QuadPart) * 1000.0 / GetFrequency().QuadPart;
        float t = (float)max(0.0, min(1.0, elapsed / duration));
        if (t >= 1.0f) return target;
        float remaining = 1.0f - t;
        float eased = 1.0f - remaining * remaining * remaining;
        return from + (target - from) * eased;
    }

    void InvalidateTabStrip(HWND hwnd) {
        RECT rc; GetClientRect(hwnd, &rc);
        RECT pad = GetPad(hwnd);
        int top = pad.top + GetTopBarOffset();
        RECT strip = {0, max(0, top), rc.right, max(0, top + 36)};
        if (strip.bottom > strip.top) InvalidateRect(hwnd, &strip, FALSE);
    }

    void PositionOpeningWindow(const WindowAnimation& animation, float progress) {
        if (!IsWindow(animation.hwnd)) return;
        int offset = (int)(animation.offsetY * (1.0f - progress) + 0.5f);
        SetWindowPos(animation.hwnd, NULL, animation.target.left, animation.target.top + offset, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        SetLayeredWindowAttributes(animation.hwnd, 0, (BYTE)(32 + 223 * progress + 0.5f), LWA_ALPHA);
    }
}

void StopUIAnimationTimer() {
    if (frameClock.fallbackTimer && frameClock.hwnd) KillTimer(frameClock.hwnd, 4);
    if (frameClock.thread) {
        SetEvent(frameClock.stopEvent);
        WaitForSingleObject(frameClock.thread, INFINITE);
        CloseHandle(frameClock.thread);
    }
    if (frameClock.timer) CloseHandle(frameClock.timer);
    if (frameClock.stopEvent) CloseHandle(frameClock.stopEvent);
    if (frameClock.timerResolutionRequested) timeEndPeriod(1);
    frameClock.thread = frameClock.timer = frameClock.stopEvent = NULL;
    frameClock.hwnd = NULL;
    frameClock.fallbackTimer = frameClock.timerResolutionRequested = false;
    InterlockedExchange(&frameClock.framePending, 0);
}

void StartUIAnimationTimer(HWND hwnd) {
    if (!enableAnimations || !HasActiveAnimations() || frameClock.thread || frameClock.fallbackTimer) return;
    frameClock.hwnd = hwnd;
    frameClock.frequency = GetFrequency();
    frameClock.frameTicks = GetFrameTicks(hwnd, frameClock.frequency);
    frameClock.stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    frameClock.timer = CreateWaitableTimerExW(NULL, NULL, 0x00000002 /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
    if (!frameClock.timer) {
        frameClock.timer = CreateWaitableTimerW(NULL, FALSE, NULL);
        frameClock.timerResolutionRequested = timeBeginPeriod(1) == TIMERR_NOERROR;
    }
    if (frameClock.stopEvent && frameClock.timer) frameClock.thread = CreateThread(NULL, 0, RunFrameClock, NULL, 0, NULL);
    if (!frameClock.thread) {
        StopUIAnimationTimer();
        frameClock.hwnd = hwnd;
        frameClock.fallbackTimer = SetTimer(hwnd, 4, USER_TIMER_MINIMUM, NULL) != 0;
        if (!frameClock.fallbackTimer) {
            FinishUIAnimations(hwnd);
            return;
        }
    }
    if (InterlockedCompareExchange(&frameClock.framePending, 1, 0) == 0 &&
        !PostMessageW(hwnd, WM_APP_ANIMATION_FRAME, 0, 0)) {
        InterlockedExchange(&frameClock.framePending, 0);
    }
}

void CancelWindowAnimation(HWND hwnd, bool finish) {
    for (int i = (int)openingWindows.size() - 1; i >= 0; --i) {
        if (openingWindows[i].hwnd == hwnd) {
            if (finish && IsWindow(hwnd)) SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
            openingWindows.erase(openingWindows.begin() + i);
        }
    }
    if (!HasActiveAnimations()) StopUIAnimationTimer();
}

void ShowAnimatedWindow(HWND hwnd, int showCommand, int offsetY, float durationMs) {
    if (!hwnd) return;
    CancelWindowAnimation(hwnd, false);
    if (!enableAnimations || !IsWindow(hwndMain)) {
        ShowWindow(hwnd, showCommand);
        UpdateWindow(hwnd);
        return;
    }

    WindowAnimation animation = {hwnd, {}, offsetY, durationMs, {}};
    GetWindowRect(hwnd, &animation.target);
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
    if (!SetLayeredWindowAttributes(hwnd, 0, 32, LWA_ALPHA)) {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style);
        ShowWindow(hwnd, showCommand);
        UpdateWindow(hwnd);
        return;
    }
    PositionOpeningWindow(animation, 0.0f);
    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);
    if (!IsWindow(hwnd)) return;
    QueryPerformanceCounter(&animation.startQPC);
    openingWindows.push_back(animation);
    StartUIAnimationTimer(hwndMain);
}

void TriggerZenTopAnimation(HWND hwnd, bool show) {
    zenTopVisible = show;
    float target = show ? 1.0f : 0.0f;
    if (!enableAnimations) {
        zenTopProgress = zenTopAnimTargetProgress = target;
        zenTopAnimStartProgress = target;
        UpdateZenChrome(hwnd);
        return;
    }
    if (zenTopAnimTargetProgress == target) return;
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    if (zenTopProgress != zenTopAnimTargetProgress) {
        zenTopProgress = SampleProgress(zenTopAnimStartProgress, zenTopAnimTargetProgress, zenTopAnimStartQPC, now,
            zenTopAnimTargetProgress > 0.0f ? 140.0f : 110.0f);
    }
    zenTopAnimStartProgress = zenTopProgress;
    zenTopAnimTargetProgress = target;
    zenTopAnimStartQPC = now;
    StartUIAnimationTimer(hwnd);
}

void TriggerZenBottomAnimation(HWND hwnd, bool show) {
    zenBottomVisible = show;
    float target = show ? 1.0f : 0.0f;
    if (!enableAnimations) {
        zenBottomProgress = zenBottomAnimTargetProgress = target;
        zenBottomAnimStartProgress = target;
        UpdateZenChrome(hwnd);
        return;
    }
    if (zenBottomAnimTargetProgress == target) return;
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    if (zenBottomProgress != zenBottomAnimTargetProgress) {
        zenBottomProgress = SampleProgress(zenBottomAnimStartProgress, zenBottomAnimTargetProgress, zenBottomAnimStartQPC, now,
            zenBottomAnimTargetProgress > 0.0f ? 140.0f : 110.0f);
    }
    zenBottomAnimStartProgress = zenBottomProgress;
    zenBottomAnimTargetProgress = target;
    zenBottomAnimStartQPC = now;
    StartUIAnimationTimer(hwnd);
}

static bool RemoveFinishedTabs() {
    bool erased = false;
    for (int i = (int)tabs.size() - 1; i >= 0; --i) {
        if (tabs[i].isClosing && tabs[i].animProgress <= 0.0f) {
            sptr_t docToRelease = tabs[i].docPointer;
            tabs.erase(tabs.begin() + i);
            if (activeTabIndex > (size_t)i) {
                activeTabIndex--;
            } else if (activeTabIndex >= tabs.size() && !tabs.empty()) {
                activeTabIndex = tabs.size() - 1;
            }
            if (docToRelease) Sci(SCI_RELEASEDOCUMENT, 0, docToRelease);
            erased = true;
        }
    }
    if (erased) {
        hoverElement = HOVER_NONE;
        pressedElement = HOVER_NONE;
        if (GetCapture() == hwndMain) ReleaseCapture();
    }
    return erased;
}

void FinishTabAnimations(HWND h) {
    bool changed = false;
    for (auto& tab : tabs) {
        changed = changed || tab.isOpening || tab.isClosing;
        tab.isOpening = false;
        tab.animProgress = tab.isClosing ? 0.0f : 1.0f;
    }
    if (RemoveFinishedTabs()) SaveSession();
    if (changed) UpdateUI(h);
    if (!HasActiveAnimations()) StopUIAnimationTimer();
}

void FinishUIAnimations(HWND hwnd) {
    zenTopProgress = zenTopAnimTargetProgress;
    zenBottomProgress = zenBottomAnimTargetProgress;
    zenTopAnimStartProgress = zenTopProgress;
    zenBottomAnimStartProgress = zenBottomProgress;
    for (const auto& animation : openingWindows) PositionOpeningWindow(animation, 1.0f);
    openingWindows.clear();
    FinishTabAnimations(hwnd);
    StopUIAnimationTimer();
    UpdateZenChrome(hwnd);
}

void UpdateUIAnimations(HWND hwnd) {
    if (!enableAnimations || IsIconic(hwnd)) {
        FinishUIAnimations(hwnd);
        return;
    }
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    for (int i = (int)openingWindows.size() - 1; i >= 0; --i) {
        if (!IsWindow(openingWindows[i].hwnd)) {
            openingWindows.erase(openingWindows.begin() + i);
            continue;
        }
        float progress = SampleProgress(0.0f, 1.0f, openingWindows[i].startQPC, now, openingWindows[i].durationMs);
        PositionOpeningWindow(openingWindows[i], progress);
        if (progress == 1.0f) openingWindows.erase(openingWindows.begin() + i);
    }
    float oldTop = zenTopProgress, oldBottom = zenBottomProgress;
    if (zenTopProgress != zenTopAnimTargetProgress) {
        zenTopProgress = SampleProgress(zenTopAnimStartProgress, zenTopAnimTargetProgress, zenTopAnimStartQPC, now,
            zenTopAnimTargetProgress > 0.0f ? 140.0f : 110.0f);
    }
    if (zenBottomProgress != zenBottomAnimTargetProgress) {
        zenBottomProgress = SampleProgress(zenBottomAnimStartProgress, zenBottomAnimTargetProgress, zenBottomAnimStartQPC, now,
            zenBottomAnimTargetProgress > 0.0f ? 140.0f : 110.0f);
    }

    bool tabsChanged = false;
    for (auto& tab : tabs) {
        if (tab.isOpening) {
            tab.animProgress = SampleProgress(tab.animStartProgress, 1.0f, tab.animStartQPC, now, 120.0f);
            if (tab.animProgress == 1.0f) tab.isOpening = false;
            tabsChanged = true;
        } else if (tab.isClosing) {
            tab.animProgress = SampleProgress(tab.animStartProgress, 0.0f, tab.animStartQPC, now, 100.0f);
            tabsChanged = true;
        }
    }
    if (RemoveFinishedTabs()) {
        UpdateUI(hwnd);
        SaveSession();
    }
    if (oldTop != zenTopProgress || oldBottom != zenBottomProgress) UpdateZenChrome(hwnd);
    if (tabsChanged) InvalidateTabStrip(hwnd);
    InterlockedExchange(&frameClock.framePending, 0);
    if (!HasActiveAnimations()) StopUIAnimationTimer();
}
