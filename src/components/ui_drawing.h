#ifndef UI_DRAWING_H
#define UI_DRAWING_H

#include "../globals.h"
#include <vector>
#include <string>

struct PathPart {
    std::wstring text;
    RECT rect;
    std::wstring fullPath;
};

extern std::vector<PathPart> g_pathParts;
extern RECT g_rcFileName;

struct FileChangeNoticeLayout {
    RECT message = {}, reload = {}, keepEdits = {};
    bool hideDetails = false;
};

FileChangeNoticeLayout GetFileChangeNoticeLayout(HWND h, HDC hdc, const RECT& rc);
int GetTopBarHeight();
int GetTopBarOffset();
int GetBottomBarHeight();
int GetBottomBarOffset();
RECT GetEditorViewport(HWND h);
void LayoutEditor(HWND h);
void UpdateZenChrome(HWND h);
void UpdateZenHover(HWND h, POINT pt);
void ToggleZenMode(HWND hwnd);
void FillRectColor(HDC hdc, const RECT& rc, COLORREF color);
RECT GetPad(HWND h);

void PaintTopBar(HWND h, HDC hdc, const RECT& rc);
void PaintHeaderBar(HWND h, HDC hdc, const RECT& rc);
void PaintStatusBar(HWND h, HDC hdc, const RECT& rc);
void PaintSearchBar(HWND h, HDC hdc, const RECT& rc);
void DrawBtn(HDC hdc, RECT rc, const wchar_t* text, bool hover, bool press, bool isClose = false, HFONT font = hIconFont, bool disabled = false, bool toggled = false, bool elevated = false, bool noBg = false);
void UpdateUI(HWND h);
void SyncScrollbars();
void ShowScrollbars(HWND h);
void ApplyDarkMode(HWND hwnd);
void TriggerSettingsMenu(HWND h);
void TriggerLanguagePicker(HWND h);
void TriggerSearchDialog(HWND h);
HoverElement HitTest(HWND h, POINT pt);
void OnElementClicked(HWND h, HoverElement el);
void TriggerTabRename(HWND h, int index);

#endif // UI_DRAWING_H
