#include "../globals.h"
#include "animations.h"
#include "tabmanager.h"
#include "ui_drawing.h"
#include "editor.h"

void TriggerZenTopAnimation(HWND hwnd, bool show) {
    zenTopVisible = show;
    float target = show ? 1.0f : 0.0f;
    if (!enableAnimations) {
        zenTopProgress = target;
        zenTopAnimTargetProgress = target;
        RECT rc; GetClientRect(hwnd, &rc);
        SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (zenTopAnimTargetProgress != target) {
        zenTopAnimTargetProgress = target;
        zenTopAnimStartProgress = zenTopProgress;
        zenTopAnimStart = GetTickCount64();
        timeBeginPeriod(1);
        SetTimer(hwnd, 4, 16, NULL);
    }
}

void TriggerZenBottomAnimation(HWND hwnd, bool show) {
    zenBottomVisible = show;
    float target = show ? 1.0f : 0.0f;
    if (!enableAnimations) {
        zenBottomProgress = target;
        zenBottomAnimTargetProgress = target;
        RECT rc; GetClientRect(hwnd, &rc);
        SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (zenBottomAnimTargetProgress != target) {
        zenBottomAnimTargetProgress = target;
        zenBottomAnimStartProgress = zenBottomProgress;
        zenBottomAnimStart = GetTickCount64();
        timeBeginPeriod(1);
        SetTimer(hwnd, 4, 16, NULL);
    }
}

void UpdateZenAnimations(HWND hwnd) {
    ULONGLONG now = GetTickCount64();
    bool animating = false;
    const float DURATION = 200.0f;

    if (zenTopProgress != zenTopAnimTargetProgress) {
        float elapsed = (float)(now - zenTopAnimStart);
        float t = elapsed / DURATION;
        if (t >= 1.0f) {
            zenTopProgress = zenTopAnimTargetProgress;
        } else {
            float invT = 1.0f - t;
            float easeOut = 1.0f - (invT * invT);
            zenTopProgress = zenTopAnimStartProgress + (zenTopAnimTargetProgress - zenTopAnimStartProgress) * easeOut;
            animating = true;
        }
    }

    if (zenBottomProgress != zenBottomAnimTargetProgress) {
        float elapsed = (float)(now - zenBottomAnimStart);
        float t = elapsed / DURATION;
        if (t >= 1.0f) {
            zenBottomProgress = zenBottomAnimTargetProgress;
        } else {
            float invT = 1.0f - t;
            float easeOut = 1.0f - (invT * invT);
            zenBottomProgress = zenBottomAnimStartProgress + (zenBottomAnimTargetProgress - zenBottomAnimStartProgress) * easeOut;
            animating = true;
        }
    }

    RECT rc; GetClientRect(hwnd, &rc);
    SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
    InvalidateRect(hwnd, NULL, FALSE);

    if (!animating) {
        KillTimer(hwnd, 4);
        timeEndPeriod(1);
    }
}

void UpdateTabAnimations(HWND h) {
    static LARGE_INTEGER qpcFreq = {0, 0};
    if (qpcFreq.QuadPart == 0) {
        QueryPerformanceFrequency(&qpcFreq);
    }
    
    LARGE_INTEGER nowQPC;
    QueryPerformanceCounter(&nowQPC);

    bool animating = false;
    const float DURATION = 150.0f; // ms

    for (size_t i = 0; i < tabs.size(); ++i) {
        if (tabs[i].isOpening) {
            double elapsed = (double)(nowQPC.QuadPart - tabs[i].animStartQPC.QuadPart) * 1000.0 / (double)qpcFreq.QuadPart;
            float t = (float)(elapsed / DURATION);
            if (t >= 1.0f) {
                tabs[i].animProgress = 1.0f;
                tabs[i].isOpening = false;
            } else {
                tabs[i].animProgress = t * t * (3.0f - 2.0f * t);
                animating = true;
            }
        } else if (tabs[i].isClosing) {
            double elapsed = (double)(nowQPC.QuadPart - tabs[i].animStartQPC.QuadPart) * 1000.0 / (double)qpcFreq.QuadPart;
            float t = (float)(elapsed / DURATION);
            if (t >= 1.0f) {
                tabs[i].animProgress = 0.0f;
            } else {
                float p = t * t * (3.0f - 2.0f * t);
                tabs[i].animProgress = 1.0f - p;
                animating = true;
            }
        }
    }

    bool erased = false;
    for (int i = (int)tabs.size() - 1; i >= 0; --i) {
        if (tabs[i].isClosing && tabs[i].animProgress <= 0.0f) {
            sptr_t docToRelease = tabs[i].docPointer;
            if (docToRelease) Sci(SCI_RELEASEDOCUMENT, 0, docToRelease);
            tabs.erase(tabs.begin() + i);
            if (activeTabIndex > (size_t)i) {
                activeTabIndex--;
            } else if (activeTabIndex >= tabs.size() && !tabs.empty()) {
                activeTabIndex = tabs.size() - 1;
            }
            erased = true;
        }
    }

    if (erased && activeTabIndex < tabs.size()) {
        Sci(SCI_SETDOCPOINTER, 0, tabs[activeTabIndex].docPointer);
    }

    RECT rcMain; GetClientRect(h, &rcMain);
    RECT pad = GetPad(h);
    RECT rcTop = { 0, 0, rcMain.right, pad.top + 36 };
    RedrawWindow(h, &rcTop, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOCHILDREN);

    bool stillAnimating = false;
    for (size_t i = 0; i < tabs.size(); ++i) {
        if (tabs[i].isOpening || tabs[i].isClosing) {
            stillAnimating = true;
            break;
        }
    }

    if (!stillAnimating) {
        KillTimer(h, 5);
        timeEndPeriod(1);
        UpdateUI(h);
    }
}
