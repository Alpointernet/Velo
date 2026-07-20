#ifndef UI_DRAWING_H
#define UI_DRAWING_H

#include "../globals.h"

void PaintTopBar(HWND h, HDC hdc, const RECT& rc);
void PaintHeaderBar(HWND h, HDC hdc, const RECT& rc);
void PaintStatusBar(HWND h, HDC hdc, const RECT& rc);
void PaintSearchBar(HWND h, HDC hdc, const RECT& rc);
void DrawBtn(HDC hdc, RECT rc, const wchar_t* text, bool hover, bool press, bool isClose = false, HFONT font = hIconFont, bool disabled = false, bool toggled = false, bool elevated = false);
void UpdateUI(HWND h);
void SyncScrollbars();
void ShowScrollbars(HWND h);
void ApplyDarkMode(HWND hwnd);
void TriggerSettingsMenu(HWND h);
void TriggerSearchDialog(HWND h);
HoverElement HitTest(HWND h, POINT pt);
void OnElementClicked(HWND h, HoverElement el);

#endif // UI_DRAWING_H
