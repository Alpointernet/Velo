#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include "../globals.h"

#define WM_APP_ANIMATION_FRAME (WM_APP + 1)

void TriggerZenTopAnimation(HWND hwnd, bool show);
void TriggerZenBottomAnimation(HWND hwnd, bool show);
void StartUIAnimationTimer(HWND hwnd);
void StopUIAnimationTimer();
void UpdateUIAnimations(HWND hwnd);
void FinishTabAnimations(HWND hwnd);
void FinishUIAnimations(HWND hwnd);
void ShowAnimatedWindow(HWND hwnd, int showCommand = SW_SHOW, int offsetY = 8, float durationMs = 120.0f);
void CancelWindowAnimation(HWND hwnd, bool finish = true);

#endif // ANIMATIONS_H
