#include "../globals.h"
#include "ui_drawing.h"
#include "editor.h"
#include "tabmanager.h"
#include "dialogs.h"
#include "animations.h"

extern int tabRenameIndex;
extern HWND hwndTabRenameEdit;
void FillRectColor(HDC hdc, const RECT& rc, COLORREF color) {
    SetBkColor(hdc, color);
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
}

RECT GetPad(HWND h) {
    if (zenMode) return { 0, 0, 0, 0 };
    if (!IsZoomed(h)) return { 1, 0, 1, 1 };
    RECT rc; GetWindowRect(h, &rc);
    MONITORINFO mi = { sizeof(mi) }; GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
    return { mi.rcWork.left - rc.left, mi.rcWork.top - rc.top, rc.right - mi.rcWork.right, rc.bottom - mi.rcWork.bottom };
}

void UpdateUI(HWND h) {
    RECT rc; GetClientRect(h, &rc); RECT pad = GetPad(h);
    bool inlineReplace = (rc.right - pad.right - pad.left > 1230);
    int offset = searchVisible ? (replaceVisible ? (inlineReplace ? 36 : 72) : 36) : 0;
    int fullTopBarH = showTopBar ? 70 : 36;
    int topBarH = fullTopBarH;
    if (zenMode) topBarH = (int)(fullTopBarH * zenTopProgress + 0.5f);
    int topH = pad.top + topBarH + (zenMode && zenTopProgress < 0.001f ? 0 : EDITOR_TOP_MARGIN) + offset;
    RECT rcTop = { 0, 0, rc.right, topH };
    
    int fullBottomBarH = 24;
    int bottomBarH = fullBottomBarH;
    if (zenMode) bottomBarH = (int)(fullBottomBarH * zenBottomProgress + 0.5f);
    RECT rcStatus = { 0, rc.bottom - pad.bottom - bottomBarH, rc.right, rc.bottom };
    
    InvalidateRect(h, &rcTop, FALSE); InvalidateRect(h, &rcStatus, FALSE);
    std::wstring title = (tabs[activeTabIndex].filePath.empty() ? L"Untitled" : tabs[activeTabIndex].filePath) + (tabs[activeTabIndex].isModified ? L"*" : L"") + L" - Velo";
    static std::wstring lastTitle = L"";
    if (title != lastTitle) {
        lastTitle = title;
        SetWindowTextW(h, title.c_str());
    }
}

void SyncScrollbars() {
    if (!hwndScintilla || !hwndVScroll || !hwndHScroll) return;
    RECT rcSci; GetClientRect(hwndScintilla, &rcSci);
    RECT pad = GetPad(hwndMain);
    bool inlineReplace = (pad.left + pad.right > 0 ? (GetSystemMetrics(SM_CXSCREEN) /* approximate, rc.right is not available */) : 0);
    // Wait, GetClientRect(hwndMain, &rcMain) is better.
    RECT rcMain; GetClientRect(hwndMain, &rcMain);
    bool isInline = (rcMain.right - pad.right - pad.left > 1230);
    
    int offset = 0;
    if (searchVisible) offset = replaceVisible ? (isInline ? 36 : 72) : 36;
    int fullTopBarH = showTopBar ? 70 : 36;
    int topBarH = fullTopBarH;
    if (zenMode) topBarH = (int)(fullTopBarH * zenTopProgress + 0.5f);
    int sciX = pad.left, sciY = pad.top + topBarH + offset + (zenMode && zenTopProgress < 0.001f ? 0 : EDITOR_TOP_MARGIN);

    int marginW = GetTotalMarginWidth(); 
    int vLineH = Sci(SCI_TEXTHEIGHT);
    int vVis = vLineH > 0 ? rcSci.bottom / vLineH : 1;
    int hVis = rcSci.right - marginW;

    int vTotal = Sci(SCI_GETLINECOUNT);
    int maxVPos = max(0, vTotal - (int)(vVis * 0.6));
    if (Sci(SCI_GETFIRSTVISIBLELINE) > maxVPos) {
        Sci(SCI_SETFIRSTVISIBLELINE, maxVPos);
    }
    
    int vPos = Sci(SCI_GETFIRSTVISIBLELINE);
    int hPos = Sci(SCI_GETXOFFSET);
    int hTotal = Sci(SCI_GETSCROLLWIDTH);
    bool needV = (vTotal > vVis);
    bool needH = (hTotal > hVis);

    static int lastVPos = -1, lastHPos = -1, lastSciW = -1, lastSciH = -1;
    if (vPos == lastVPos && hPos == lastHPos && rcSci.right == lastSciW && rcSci.bottom == lastSciH) return;
    lastVPos = vPos; lastHPos = hPos; lastSciW = rcSci.right; lastSciH = rcSci.bottom;

    if (needV) {
        int trackLen = rcSci.bottom - (needH ? CUSTOM_SB_SIZE : 0) - 4;
        int thumbLen = max(20, (int)((double)vVis / (maxVPos + vVis) * trackLen));
        int mScroll = maxVPos;
        int tPos = mScroll > 0 ? min((int)((double)vPos / mScroll * (trackLen - thumbLen)), trackLen - thumbLen) : 0;
        SetWindowPos(hwndVScroll, HWND_TOP, sciX + rcSci.right - CUSTOM_SB_SIZE - 2, sciY + 2 + tPos, CUSTOM_SB_SIZE, thumbLen, (scrollbarsVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOACTIVATE);
    } else ShowWindow(hwndVScroll, SW_HIDE);

    if (needH) {
        int trackLen = hVis - (needV ? CUSTOM_SB_SIZE : 0) - 4; 
        int thumbLen = max(20, (int)((double)hVis / hTotal * trackLen));
        int mScroll = hTotal - hVis;
        int tPos = mScroll > 0 ? (int)((double)hPos / mScroll * (trackLen - thumbLen)) : 0;
        SetWindowPos(hwndHScroll, HWND_TOP, sciX + marginW + 2 + tPos, sciY + rcSci.bottom - CUSTOM_SB_SIZE - 2, thumbLen, CUSTOM_SB_SIZE, (scrollbarsVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOACTIVATE);
    } else ShowWindow(hwndHScroll, SW_HIDE);
}

void ShowScrollbars(HWND h) {
    scrollbarsVisible = true;
    SyncScrollbars();
    SetTimer(h, 1, 1500, NULL);
}

void ApplyDarkMode(HWND hwnd) {
    int val = 1; DwmSetWindowAttribute(hwnd, 20, &val, sizeof(val));
    COLORREF border = theme.border; DwmSetWindowAttribute(hwnd, 34, &border, sizeof(border));
    if (HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32)) {
        if (auto SetMode = (int(WINAPI*)(int))GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135))) SetMode(1);
        if (auto AllowDark = (bool(WINAPI*)(HWND, bool))GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133))) AllowDark(hwnd, true);
        if (auto Flush = (void(WINAPI*)())GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136))) Flush();
    }
}

static const wchar_t* GetLanguageName() {
    if (activeTabIndex >= tabs.size()) return L"Plain Text";
    std::string manualLang = tabs[activeTabIndex].manualLanguage;
    if (!manualLang.empty()) {
        if (manualLang == "cpp") return L"C++";
        if (manualLang == "python") return L"Python";
        if (manualLang == "hypertext") return L"HTML";
        if (manualLang == "css") return L"CSS";
        if (manualLang == "markdown") return L"Markdown";
        if (manualLang == "null") return L"Plain Text";
    }
    std::wstring fp = tabs[activeTabIndex].filePath;
    size_t dot = fp.find_last_of(L'.');
    if (dot == std::wstring::npos) return L"Plain Text";
    std::wstring e = fp.substr(dot + 1);
    if (!_wcsicmp(e.c_str(), L"cpp") || !_wcsicmp(e.c_str(), L"cc") || !_wcsicmp(e.c_str(), L"h") || !_wcsicmp(e.c_str(), L"hpp")) return L"C++";
    if (!_wcsicmp(e.c_str(), L"c")) return L"C";
    if (!_wcsicmp(e.c_str(), L"cs")) return L"C#";
    if (!_wcsicmp(e.c_str(), L"py")) return L"Python";
    if (!_wcsicmp(e.c_str(), L"js") || !_wcsicmp(e.c_str(), L"jsx")) return L"JavaScript";
    if (!_wcsicmp(e.c_str(), L"ts") || !_wcsicmp(e.c_str(), L"tsx")) return L"TypeScript";
    if (!_wcsicmp(e.c_str(), L"html") || !_wcsicmp(e.c_str(), L"htm")) return L"HTML";
    if (!_wcsicmp(e.c_str(), L"xml")) return L"XML";
    if (!_wcsicmp(e.c_str(), L"css")) return L"CSS";
    if (!_wcsicmp(e.c_str(), L"scss")) return L"SCSS";
    if (!_wcsicmp(e.c_str(), L"json") || !_wcsicmp(e.c_str(), L"jsonc")) return L"JSON";
    if (!_wcsicmp(e.c_str(), L"md") || !_wcsicmp(e.c_str(), L"markdown")) return L"Markdown";
    if (!_wcsicmp(e.c_str(), L"java")) return L"Java";
    if (!_wcsicmp(e.c_str(), L"go")) return L"Go";
    if (!_wcsicmp(e.c_str(), L"rs")) return L"Rust";
    if (!_wcsicmp(e.c_str(), L"rb")) return L"Ruby";
    if (!_wcsicmp(e.c_str(), L"sql")) return L"SQL";
    if (!_wcsicmp(e.c_str(), L"yaml") || !_wcsicmp(e.c_str(), L"yml")) return L"YAML";
    if (!_wcsicmp(e.c_str(), L"toml")) return L"TOML";
    if (!_wcsicmp(e.c_str(), L"ini") || !_wcsicmp(e.c_str(), L"cfg") || !_wcsicmp(e.c_str(), L"conf") || !_wcsicmp(e.c_str(), L"config")) return L"INI";
    if (!_wcsicmp(e.c_str(), L"env")) return L"ENV";
    if (!_wcsicmp(e.c_str(), L"bat") || !_wcsicmp(e.c_str(), L"cmd")) return L"Batch";
    if (!_wcsicmp(e.c_str(), L"ps1")) return L"PowerShell";
    if (!_wcsicmp(e.c_str(), L"sh")) return L"Shell";
    if (!_wcsicmp(e.c_str(), L"ahk")) return L"AutoHotkey";
    if (!_wcsicmp(e.c_str(), L"rc")) return L"Resource";
    if (!_wcsicmp(e.c_str(), L"iss")) return L"Inno Setup";
    if (!_wcsicmp(e.c_str(), L"txt") || !_wcsicmp(e.c_str(), L"log")) return L"Plain Text";
    return L"Plain Text";
}

RECT GetEolRect(HWND h, HDC hdc, const RECT& rc) {
    RECT pad = GetPad(h);
    int pos = hwndScintilla ? Sci(SCI_GETCURRENTPOS) : 0;
    int line = hwndScintilla ? Sci(SCI_LINEFROMPOSITION, pos) + 1 : 1;
    int col = hwndScintilla ? Sci(SCI_GETCOLUMN, pos) + 1 : 1;
    int eolMode = hwndScintilla ? Sci(SCI_GETEOLMODE) : 0;
    const wchar_t* eol = (eolMode == SC_EOL_CRLF) ? L"CRLF" : ((eolMode == SC_EOL_CR) ? L"CR" : L"LF");
    const wchar_t* lang = GetLanguageName();
    HFONT oldFont = hSmallFont ? (HFONT)SelectObject(hdc, hSmallFont) : NULL;
    RECT rcLangMeasure = { 0 }; DrawTextW(hdc, lang, -1, &rcLangMeasure, DT_CALCRECT | DT_SINGLELINE);
    int wLang = rcLangMeasure.right - rcLangMeasure.left;
    RECT rcDivMeasure = { 0 }; DrawTextW(hdc, L"   |   ", -1, &rcDivMeasure, DT_CALCRECT | DT_SINGLELINE);
    int wDiv = rcDivMeasure.right - rcDivMeasure.left;
    RECT rcEolMeasure = { 0 }; DrawTextW(hdc, eol, -1, &rcEolMeasure, DT_CALCRECT | DT_SINGLELINE);
    int wEol = rcEolMeasure.right - rcEolMeasure.left;
    if (oldFont) SelectObject(hdc, oldFont);
    int rightLimit = rc.right - pad.right - 10;
    int eolRight = rightLimit - wLang - wDiv;
    int eolLeft = eolRight - wEol;
    return { eolLeft, 0, eolRight, 24 };
}

RECT GetLangRect(HWND h, HDC hdc, const RECT& rc) {
    RECT pad = GetPad(h);
    const wchar_t* lang = GetLanguageName();
    HFONT oldFont = hSmallFont ? (HFONT)SelectObject(hdc, hSmallFont) : NULL;
    RECT rcLangMeasure = { 0 }; DrawTextW(hdc, lang, -1, &rcLangMeasure, DT_CALCRECT | DT_SINGLELINE);
    int wLang = rcLangMeasure.right - rcLangMeasure.left;
    if (oldFont) SelectObject(hdc, oldFont);
    int rightLimit = rc.right - pad.right - 10;
    int langRight = rightLimit;
    int langLeft = langRight - wLang;
    return { langLeft, 0, langRight, 24 };
}

HoverElement HitTest(HWND h, POINT pt) {
    RECT rc; GetClientRect(h, &rc); RECT pad = GetPad(h);
    
    int fullTopBarH = showTopBar ? 70 : 36;
    int topOffset = (zenMode && zenTopProgress < 0.999f) ? (int)((zenTopProgress - 1.0f) * fullTopBarH) : 0;
    POINT topPt = pt;
    topPt.y -= topOffset;

    if (!zenMode || zenTopProgress > 0.05f) {
        if (topPt.y >= pad.top && topPt.y < pad.top + 35) {
            if (pt.x >= rc.right - pad.right - 45 && pt.x < rc.right - pad.right) return HOVER_CLOSE;
            if (pt.x >= rc.right - pad.right - 90 && pt.x < rc.right - pad.right - 45) return HOVER_MAXIMIZE;
            if (pt.x >= rc.right - pad.right - 135 && pt.x < rc.right - pad.right - 90) return HOVER_MINIMIZE;
            if (pt.x >= pad.left + 10 && pt.x < pad.left + 35) return Sci(SCI_CANUNDO) ? HOVER_UNDO : HOVER_NONE;
            if (pt.x >= pad.left + 35 && pt.x < pad.left + 60) return Sci(SCI_CANREDO) ? HOVER_REDO : HOVER_NONE;
            
            int startX = pad.left + 70;
            int maxTabRight = rc.right - pad.right - 135;
            
            int totalW = 0;
            for (size_t i = 0; i < tabs.size(); ++i) totalW += GetTabWidth(i);
            bool overflow = (startX + totalW > maxTabRight);
            int tabLimit = overflow ? (maxTabRight - 30) : maxTabRight;

            int draggedIdx = -1;
            if (GetCapture() == h && pressedElement >= HOVER_TAB_BASE && pressedElement < HOVER_TAB_CLOSE_BASE) {
                draggedIdx = pressedElement - HOVER_TAB_BASE;
            }

            int rightmostTabX = startX;
            int curX = startX;
            for (size_t i = 0; i < tabs.size(); ++i) {
                int tabW = GetTabWidth(i);
                if (tabs[i].isClosing) {
                    curX += tabW;
                    continue;
                }
                if (curX < tabLimit && pt.x >= curX && pt.x < curX + tabW) {
                    if (pt.x < tabLimit) {
                        if (pt.x >= curX + tabW - 25 && pt.x < curX + tabW - 5 && topPt.y >= pad.top + 8 && topPt.y < pad.top + 28) return (HoverElement)(HOVER_TAB_CLOSE_BASE + i);
                        return (HoverElement)(HOVER_TAB_BASE + i);
                    }
                }
                if (i != (size_t)draggedIdx && tabW > 0) {
                    rightmostTabX = max(rightmostTabX, curX + tabW);
                }
                curX += tabW;
            }

            if (draggedIdx >= 0) {
                POINT ptCursor; GetCursorPos(&ptCursor); ScreenToClient(h, &ptCursor);
                int tabW = GetTabWidth(draggedIdx);
                int drawX = ptCursor.x - dragGrabOffset;
                if (drawX < startX) drawX = startX;
                if (drawX + tabW > tabLimit) drawX = tabLimit - tabW;
                if (tabW > 0) rightmostTabX = max(rightmostTabX, drawX + tabW);
            }

            int addTabX = overflow ? tabLimit : min(rightmostTabX, tabLimit);
            if (pt.x >= addTabX && pt.x < addTabX + 30) return HOVER_ADD_TAB;
        }
    }

    int bottomOffset = (zenMode && zenBottomProgress < 0.999f) ? (int)((1.0f - zenBottomProgress) * 24.0f) : 0;
    POINT bottomPt = pt;
    bottomPt.y += bottomOffset;

    if (!zenMode || zenBottomProgress > 0.05f) {
        if (bottomPt.y >= rc.bottom - 24 && bottomPt.y < rc.bottom) {
            if (pt.x >= pad.left && pt.x < pad.left + 30) return HOVER_SETTINGS;
            if (pt.x >= pad.left + 30 && pt.x < pad.left + 60) return HOVER_SEARCH;
            
            if (hwndScintilla) {
                HDC hdc = GetDC(h);
                RECT rcEol = GetEolRect(h, hdc, rc);
                RECT rcLang = GetLangRect(h, hdc, rc);
                ReleaseDC(h, hdc);
                
                int sbTop = rc.bottom - pad.bottom - 24;
                if (pt.x >= rcEol.left && pt.x < rcEol.right && bottomPt.y >= sbTop && bottomPt.y < sbTop + 24) {
                    return HOVER_STATUS_EOL;
                }
                if (pt.x >= rcLang.left && pt.x < rcLang.right && bottomPt.y >= sbTop && bottomPt.y < sbTop + 24) {
                    return HOVER_STATUS_LANG;
                }
            }
        }
    }

    if (!zenMode || zenTopProgress > 0.05f) {
        if (showTopBar && topPt.y >= pad.top + 35 && topPt.y < pad.top + 70) {
            int maxRight = rc.right - pad.right - 320;
            for (size_t i = 0; i < g_pathParts.size(); ++i) {
                if (g_pathParts[i].rect.left >= maxRight) break;
                RECT partRc = g_pathParts[i].rect;
                if (partRc.right > maxRight) partRc.right = maxRight;
                if (pt.x >= partRc.left && pt.x < partRc.right) return (HoverElement)(HOVER_PATH_PART_BASE + i);
            }
            if (pt.x >= g_rcFileName.left && pt.x < g_rcFileName.right && topPt.y >= g_rcFileName.top && topPt.y < g_rcFileName.bottom) {
                return HOVER_FILE_NAME;
            }
        }
    }
    int topBarH = showTopBar ? 70 : 36;
    if (searchVisible && pt.y >= pad.top + topBarH && pt.y < pad.top + topBarH + (replaceVisible ? (rc.right - pad.right - pad.left > 1230 ? 36 : 72) : 36)) {
        bool inlineReplace = (rc.right - pad.right - pad.left > 1230);
        int topY = pad.top + topBarH;
        int relY = pt.y - topY;
        
        if (pt.y >= topY && pt.y < topY + 36) {
            if (pt.x >= rc.right - pad.right - 275 && pt.x < rc.right - pad.right - 251 && pt.y >= topY + 6 && pt.y < topY + 30) return HOVER_SEARCH_PREV;
            if (pt.x >= rc.right - pad.right - 246 && pt.x < rc.right - pad.right - 222 && pt.y >= topY + 6 && pt.y < topY + 30) return HOVER_SEARCH_NEXT;
            if (pt.x >= rc.right - pad.right - 212 && pt.x < rc.right - pad.right - 132 && pt.y >= topY + 6 && pt.y < topY + 30) return HOVER_SEARCH_SELECT_ALL;
            if (pt.x >= rc.right - pad.right - 122 && pt.x < rc.right - pad.right - 42 && pt.y >= topY + 6 && pt.y < topY + 30) return HOVER_SEARCH_REPLACE_TOGGLE;
            if (pt.x >= rc.right - pad.right - 32 && pt.x < rc.right - pad.right - 8 && pt.y >= topY + 6 && pt.y < topY + 30) return HOVER_SEARCH_CLOSE;
            
            if (replaceVisible && inlineReplace) {
                int repX = pad.left + 420;
                int btn1X = repX + 342;
                if (pt.x >= btn1X && pt.x < btn1X + 80 && pt.y >= topY + 6 && pt.y < topY + 30) return HOVER_REPLACE_NEXT;
                int btn2X = btn1X + 90;
                if (pt.x >= btn2X && pt.x < btn2X + 90 && pt.y >= topY + 6 && pt.y < topY + 30) return HOVER_REPLACE_ALL;
            }
        } 
        
        if (replaceVisible && !inlineReplace && pt.y >= topY + 36 && pt.y < topY + 72) {
            if (pt.x >= pad.left + 350 && pt.x < pad.left + 430 && pt.y >= topY + 36 + 6 && pt.y < topY + 36 + 30) return HOVER_REPLACE_NEXT;
            if (pt.x >= pad.left + 440 && pt.x < pad.left + 530 && pt.y >= topY + 36 + 6 && pt.y < topY + 36 + 30) return HOVER_REPLACE_ALL;
        }
    }
    return HOVER_NONE;
}

void DrawBtn(HDC hdc, RECT rc, const wchar_t* text, bool hover, bool press, bool isClose, HFONT font, bool disabled, bool toggled, bool elevated, bool noBg) {
    COLORREF textCol = disabled ? theme.textDisabled : theme.textNormal;
    COLORREF bgCol = elevated ? theme.hoverBg : 0;
    bool hasBg = elevated;
    if (!disabled) {
        if (noBg) {
            if (hover || press) textCol = theme.textActive;
            hasBg = false;
        }
        else if (isClose && hover) { bgCol = press ? theme.closePress : theme.closeHover; textCol = theme.textActive; hasBg = true; } // Close buttons have distinct native red
        else if (toggled) { bgCol = hover ? theme.hoverBg : theme.border; textCol = theme.accent; hasBg = true; }
        else if (press) { bgCol = theme.hoverBg; if (!isClose) textCol = theme.textActive; hasBg = true; }
        else if (hover) { bgCol = theme.border; if (!isClose) textCol = theme.textActive; hasBg = true; }
    } else {
        hasBg = false;
    }
    if (hasBg) FillRectColor(hdc, rc, bgCol);
    HFONT oldFont = font ? (HFONT)SelectObject(hdc, font) : NULL; int oldBk = SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, textCol);
    RECT textRc = rc; textRc.top += 1; textRc.bottom += 1;
    DrawTextW(hdc, text, -1, &textRc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    SetBkMode(hdc, oldBk); if (oldFont) SelectObject(hdc, oldFont);
}

void PaintTopBar(HWND h, HDC hdc, const RECT& rc) {
    RECT pad = GetPad(h);
    FillRectColor(hdc, { 0, 0, rc.right, pad.top + 35 }, theme.tabBg);
    
    bool canUndo = Sci(SCI_CANUNDO) != 0;
    bool canRedo = Sci(SCI_CANREDO) != 0;
    DrawBtn(hdc, { pad.left + 10, pad.top, pad.left + 35, pad.top + 35 }, L"\uE7A7", hoverElement == HOVER_UNDO, pressedElement == HOVER_UNDO, false, hIconFont, !canUndo, false, false);
    DrawBtn(hdc, { pad.left + 35, pad.top, pad.left + 60, pad.top + 35 }, L"\uE7A6", hoverElement == HOVER_REDO, pressedElement == HOVER_REDO, false, hIconFont, !canRedo, false, false);
    
    int startX = pad.left + 70;
    int totalW = 0;
    for (size_t i = 0; i < tabs.size(); ++i) {
        totalW += GetTabWidth(i);
    }
    
    int maxTabRight = rc.right - pad.right - 135;
    bool overflow = (startX + totalW > maxTabRight);
    int tabLimit = overflow ? (maxTabRight - 30) : maxTabRight;
    
    // Set clipping region for tab drawing
    HRGN hRgn = CreateRectRgn(startX, pad.top, tabLimit, pad.top + 36);
    SelectClipRgn(hdc, hRgn);
    
    int curX = startX; HFONT oldFont = hUIFont ? (HFONT)SelectObject(hdc, hUIFont) : NULL;
    int activeTabLeft = 0, activeTabRight = 0;
    int draggedIdx = -1;
    if (GetCapture() == h && pressedElement >= HOVER_TAB_BASE && pressedElement < HOVER_TAB_CLOSE_BASE) {
        draggedIdx = pressedElement - HOVER_TAB_BASE;
    }
    int draggedLogicalX = 0;

    auto drawTab = [&](size_t i, int drawX, bool isDragged) {
        int tabW = GetTabWidth(i);
        if (tabW <= 2) return;
        
        int saveState = SaveDC(hdc);
        IntersectClipRect(hdc, drawX, pad.top, min(drawX + tabW, tabLimit), pad.top + 35);

        bool active = (i == activeTabIndex), hover = (hoverElement == HOVER_TAB_BASE + i || hoverElement == HOVER_TAB_CLOSE_BASE + i);
        if (isDragged) hover = true;
        
        FillRectColor(hdc, { drawX, pad.top, drawX + tabW, pad.top + 35 }, active ? theme.bg : (hover ? theme.hoverBg : theme.tabBg));
        FillRectColor(hdc, { drawX + tabW - 1, pad.top, drawX + tabW, pad.top + 35 }, theme.tabBg);
        if (active) {
            FillRectColor(hdc, { drawX, pad.top, drawX + tabW - 1, pad.top + 2 }, theme.accent);
            if (!isDragged) { activeTabLeft = drawX; activeTabRight = drawX + tabW - 1; }
        }
        
        if (tabW > 25) {
            RECT rcText = { drawX + 10, pad.top, drawX + tabW - 25, pad.top + 35 };
            SetTextColor(hdc, active ? theme.textActive : theme.textDim); SetBkMode(hdc, TRANSPARENT);
            DrawTextW(hdc, tabs[i].title.c_str(), -1, &rcText, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        }
        
        if (!isDragged && hwndTabRenameEdit && IsWindowVisible(hwndTabRenameEdit) && tabRenameIndex == i) {
            RECT rcBorder = { drawX + 9, pad.top + 7, drawX + tabW - 24, pad.top + 29 };
            HBRUSH hBrBorder = CreateSolidBrush(theme.border);
            FrameRect(hdc, &rcBorder, hBrBorder);
            DeleteObject(hBrBorder);
        }
        
        if (tabW > 35) {
            RECT rcClose = { drawX + tabW - 22, pad.top + 8, drawX + tabW - 6, pad.top + 28 };
            bool cHover = (hoverElement == HOVER_TAB_CLOSE_BASE + i), cPress = (pressedElement == HOVER_TAB_CLOSE_BASE + i);
            if (tabs[i].isModified && !cHover) {
                FillRectColor(hdc, { drawX + tabW - 16, pad.top + 14, drawX + tabW - 10, pad.top + 20 }, theme.textDim);
            } else {
                DrawBtn(hdc, rcClose, L"\uE711", cHover, cPress, true, hIconFont, false, false, false, true);
            }
        }

        RestoreDC(hdc, saveState);
    };

    int rightmostTabX = startX;
    for (size_t i = 0; i < tabs.size(); ++i) {
        int tabW = GetTabWidth(i);
        if (i == (size_t)draggedIdx) {
            draggedLogicalX = curX;
            FillRectColor(hdc, { curX, pad.top, curX + tabW, pad.top + 35 }, theme.tabBg);
        } else {
            drawTab(i, curX, false);
            if (tabW > 0) rightmostTabX = max(rightmostTabX, curX + tabW);
        }
        curX += tabW;
    }
    
    if (draggedIdx >= 0) {
        POINT pt; GetCursorPos(&pt); ScreenToClient(h, &pt);
        int tabW = GetTabWidth(draggedIdx);
        int drawX = pt.x - dragGrabOffset;
        if (drawX < startX) drawX = startX;
        if (drawX + tabW > tabLimit) drawX = tabLimit - tabW;
        drawTab(draggedIdx, drawX, true);
        if (draggedIdx == activeTabIndex) { activeTabLeft = drawX; activeTabRight = drawX + tabW - 1; }
        if (tabW > 0) rightmostTabX = max(rightmostTabX, drawX + tabW);
    }
    
    // Clear clipping region so we can draw other components normally
    SelectClipRgn(hdc, NULL);
    DeleteObject(hRgn);
    
    if (activeTabRight > activeTabLeft) {
        // Only paint active tab lines if the active tab is at least partially visible
        if (activeTabLeft < tabLimit) {
            int rightLineLimit = min(activeTabRight, tabLimit);
            FillRectColor(hdc, { pad.left, pad.top + 35, activeTabLeft, pad.top + 36 }, theme.border);
            FillRectColor(hdc, { rightLineLimit, pad.top + 35, rc.right - pad.right, pad.top + 36 }, theme.border);
            FillRectColor(hdc, { activeTabLeft, pad.top + 35, rightLineLimit, pad.top + 36 }, theme.bg);
            FillRectColor(hdc, { activeTabLeft - 1, pad.top, activeTabLeft, pad.top + 36 }, theme.border);
            if (activeTabRight < tabLimit) {
                FillRectColor(hdc, { activeTabRight, pad.top, activeTabRight + 1, pad.top + 36 }, theme.border);
            }
        } else {
            FillRectColor(hdc, { pad.left, pad.top + 35, rc.right - pad.right, pad.top + 36 }, theme.border);
        }
    } else {
        FillRectColor(hdc, { pad.left, pad.top + 35, rc.right - pad.right, pad.top + 36 }, theme.border);
    }
    
    int addTabX = overflow ? tabLimit : min(rightmostTabX, tabLimit);
    DrawBtn(hdc, { addTabX, pad.top, addTabX + 30, pad.top + 35 }, L"\uE710", hoverElement == HOVER_ADD_TAB, pressedElement == HOVER_ADD_TAB, false, hIconFont, false, false, false);
    
    int btnX = rc.right - pad.right - 135;
    DrawBtn(hdc, { btnX, pad.top, btnX + 45, pad.top + 35 }, L"\uE921", hoverElement == HOVER_MINIMIZE, pressedElement == HOVER_MINIMIZE, false, hWindowIconFont, false, false, false);
    DrawBtn(hdc, { btnX + 45, pad.top, btnX + 90, pad.top + 35 }, (IsZoomed(h) || zenMode) ? L"\uE923" : L"\uE922", hoverElement == HOVER_MAXIMIZE, pressedElement == HOVER_MAXIMIZE, false, hWindowIconFont, false, false, false);
    DrawBtn(hdc, { btnX + 90, pad.top, btnX + 135, pad.top + 35 }, L"\uE8BB", hoverElement == HOVER_CLOSE, pressedElement == HOVER_CLOSE, true, hWindowIconFont, false, false, false);
    if (oldFont) SelectObject(hdc, oldFont);
}

std::vector<PathPart> g_pathParts;
RECT g_rcFileName = {0, 0, 0, 0};

void PaintHeaderBar(HWND h, HDC hdc, const RECT& rc) {
    if (!showTopBar) return;
    RECT pad = GetPad(h);
    FillRectColor(hdc, { 0, pad.top + 36, rc.right, pad.top + 70 }, 0x2B2521);
    FillRectColor(hdc, { pad.left, pad.top + 69, rc.right - pad.right, pad.top + 70 }, 0x3C312C);
    FillRectColor(hdc, { 0, pad.top + 36, rc.right, pad.top + 70 }, theme.bg);
    FillRectColor(hdc, { pad.left, pad.top + 69, rc.right - pad.right, pad.top + 70 }, theme.border);
    std::wstring pathStr = (activeTabIndex < tabs.size()) ? tabs[activeTabIndex].filePath : L"", fileName = pathStr.empty() ? L"Untitled" : GetFileName(pathStr);
    
    SetBkMode(hdc, TRANSPARENT); HFONT oldFont = hUIFont ? (HFONT)SelectObject(hdc, hUIFont) : NULL;
    RECT rcMeasure = { 0 }; DrawTextW(hdc, fileName.c_str(), -1, &rcMeasure, DT_CALCRECT | DT_SINGLELINE);
    int fileW = rcMeasure.right - rcMeasure.left;
    RECT rcFile = { pad.left + 15, pad.top + 35, pad.left + 15 + fileW, pad.top + 70 };
    g_rcFileName = rcFile;
    SetTextColor(hdc, (hoverElement == HOVER_FILE_NAME) ? theme.accent : theme.textActive); DrawTextW(hdc, fileName.c_str(), -1, &rcFile, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    
    g_pathParts.clear();
    if (!pathStr.empty()) {
        size_t lastSlash = pathStr.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            std::wstring parentDir = pathStr.substr(0, lastSlash + 1);
            int curX = pad.left + 15 + fileW + 10;
            size_t start = 0, pos;
            std::wstring currentFullPath = L"";
            while ((pos = parentDir.find_first_of(L"\\/", start)) != std::wstring::npos) {
                std::wstring part = parentDir.substr(start, pos - start + 1);
                currentFullPath += part;
                RECT rcPartMeasure = { 0 };
                DrawTextW(hdc, part.c_str(), -1, &rcPartMeasure, DT_CALCRECT | DT_SINGLELINE);
                int partW = rcPartMeasure.right - rcPartMeasure.left;
                RECT rcPart = { curX, pad.top + 35, curX + partW, pad.top + 70 };
                g_pathParts.push_back({part, rcPart, currentFullPath});
                curX += partW;
                start = pos + 1;
            }
            if (start < parentDir.length()) {
                std::wstring part = parentDir.substr(start);
                currentFullPath += part;
                RECT rcPartMeasure = { 0 };
                DrawTextW(hdc, part.c_str(), -1, &rcPartMeasure, DT_CALCRECT | DT_SINGLELINE);
                int partW = rcPartMeasure.right - rcPartMeasure.left;
                RECT rcPart = { curX, pad.top + 35, curX + partW, pad.top + 70 };
                g_pathParts.push_back({part, rcPart, currentFullPath});
            }
        }
    }
    
    int maxRight = rc.right - pad.right - 320;
    for (size_t i = 0; i < g_pathParts.size(); ++i) {
        if (g_pathParts[i].rect.left >= maxRight) break;
        RECT drawRc = g_pathParts[i].rect;
        if (drawRc.right > maxRight) drawRc.right = maxRight;
        HoverElement el = (HoverElement)(HOVER_PATH_PART_BASE + i);
        bool isHover = (hoverElement == el);
        SetTextColor(hdc, isHover ? theme.accent : theme.textDim);
        DrawTextW(hdc, g_pathParts[i].text.c_str(), -1, &drawRc, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    }
    
    if (oldFont) SelectObject(hdc, oldFont);
}

void PaintStatusBar(HWND h, HDC hdc, const RECT& rc) {
    RECT pad = GetPad(h);
    FillRectColor(hdc, { 0, 0, rc.right, 24 }, theme.tabBg);
    FillRectColor(hdc, { pad.left, 0, rc.right - pad.right, 1 }, theme.border);
    DrawBtn(hdc, { pad.left, 0, pad.left + 30, 24 }, L"\uE713", hoverElement == HOVER_SETTINGS, pressedElement == HOVER_SETTINGS, false, hIconFont, false, false, false);
    DrawBtn(hdc, { pad.left + 30, 0, pad.left + 60, 24 }, L"\uE721", hoverElement == HOVER_SEARCH, pressedElement == HOVER_SEARCH, false, hIconFont, false, false, false);
    if (searchVisible) FillRectColor(hdc, { pad.left + 30, 22, pad.left + 60, 24 }, theme.accent);
    int rightLimit = rc.right - pad.right - 10;
    int pos = hwndScintilla ? Sci(SCI_GETCURRENTPOS) : 0;
    int line = hwndScintilla ? Sci(SCI_LINEFROMPOSITION, pos) + 1 : 1;
    int col = hwndScintilla ? Sci(SCI_GETCOLUMN, pos) + 1 : 1;
    int eolMode = hwndScintilla ? Sci(SCI_GETEOLMODE) : 0;
    const wchar_t* eol = (eolMode == SC_EOL_CRLF) ? L"CRLF" : ((eolMode == SC_EOL_CR) ? L"CR" : L"LF");
    const wchar_t* lang = GetLanguageName();
    HFONT oldFont = hSmallFont ? (HFONT)SelectObject(hdc, hSmallFont) : NULL;
    SetBkMode(hdc, TRANSPARENT);
    
    // Language
    bool isLangHovered = (hoverElement == HOVER_STATUS_LANG);
    SetTextColor(hdc, isLangHovered ? theme.accent : theme.textDim);
    RECT rcLang = { 0, 1, rightLimit, 25 };
    DrawTextW(hdc, lang, -1, &rcLang, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    
    RECT rcLangMeasure = { 0 }; DrawTextW(hdc, lang, -1, &rcLangMeasure, DT_CALCRECT | DT_SINGLELINE);
    int wLang = rcLangMeasure.right - rcLangMeasure.left;
    rightLimit -= wLang;
    
    // Divider
    SetTextColor(hdc, theme.border);
    RECT rcDiv1 = { 0, 1, rightLimit, 25 };
    DrawTextW(hdc, L"   |   ", -1, &rcDiv1, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    
    RECT rcDivMeasure = { 0 }; DrawTextW(hdc, L"   |   ", -1, &rcDivMeasure, DT_CALCRECT | DT_SINGLELINE);
    int wDiv = rcDivMeasure.right - rcDivMeasure.left;
    rightLimit -= wDiv;
    
    // EOL Format (Highlights on hover!)
    bool isEolHovered = (hoverElement == HOVER_STATUS_EOL);
    SetTextColor(hdc, isEolHovered ? theme.accent : theme.textDim);
    RECT rcEol = { 0, 1, rightLimit, 25 };
    DrawTextW(hdc, eol, -1, &rcEol, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    
    RECT rcEolMeasure = { 0 }; DrawTextW(hdc, eol, -1, &rcEolMeasure, DT_CALCRECT | DT_SINGLELINE);
    int wEol = rcEolMeasure.right - rcEolMeasure.left;
    rightLimit -= wEol;
    
    // Divider
    SetTextColor(hdc, theme.border);
    RECT rcDiv2 = { 0, 1, rightLimit, 25 };
    DrawTextW(hdc, L"   |   ", -1, &rcDiv2, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    rightLimit -= wDiv;
    
    // Encoding
    SetTextColor(hdc, theme.textDim);
    RECT rcEnc = { 0, 1, rightLimit, 25 };
    DrawTextW(hdc, L"UTF-8", -1, &rcEnc, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    
    RECT rcEncMeasure = { 0 }; DrawTextW(hdc, L"UTF-8", -1, &rcEncMeasure, DT_CALCRECT | DT_SINGLELINE);
    int wEnc = rcEncMeasure.right - rcEncMeasure.left;
    rightLimit -= wEnc;
    
    // Divider
    SetTextColor(hdc, theme.border);
    RECT rcDiv3 = { 0, 1, rightLimit, 25 };
    DrawTextW(hdc, L"   |   ", -1, &rcDiv3, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    rightLimit -= wDiv;
    
    // Position Info
    SetTextColor(hdc, theme.textDim);
    wchar_t posInfo[128]; swprintf_s(posInfo, L"Ln %d, Col %d", line, col);
    RECT rcPos = { 0, 1, rightLimit, 25 };
    DrawTextW(hdc, posInfo, -1, &rcPos, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    
    SelectObject(hdc, oldFont);
}

void TriggerSettingsMenu(HWND h) {
    std::vector<PopupMenuItem> items = {
        { L"New Tab", IDM_FILE_NEW, false, false, L"Ctrl+N" },
        { L"Open File...", IDM_FILE_OPEN, false, false, L"Ctrl+O" },
        { L"Save File", IDM_FILE_SAVE, false, false, L"Ctrl+S" },
        { L"Save File As...", IDM_FILE_SAVE_AS, false, false, L"Ctrl+Shift+S" },
        { L"Close Tab", IDM_FILE_CLOSE_TAB, false, false, L"Ctrl+W" },
        { L"", 0, true, false, L"" },
        { L"Word Wrap", IDM_TOGGLE_WRAP, false, Sci(SCI_GETWRAPMODE) != SC_WRAP_NONE, L"" },
        { L"Line Numbers", IDM_TOGGLE_LINES, false, Sci(SCI_GETMARGINWIDTHN, 0) > 0, L"" },
        { L"", 0, true, false, L"" },
        { L"Settings...", IDM_SETTINGS_DIALOG, false, false, L"" },
        { L"", 0, true, false, L"" },
        { L"Exit", IDM_FILE_EXIT, false, false, L"" }
    };
    
    RECT rc; GetClientRect(h, &rc); RECT pad = GetPad(h);
    POINT pt = { pad.left, rc.bottom - 24 }; ClientToScreen(h, &pt);
    
    int selectedId = ShowCustomPopupMenu(h, pt.x, pt.y, items, true);
    if (selectedId != 0) {
        PostMessageW(h, WM_COMMAND, MAKEWPARAM(selectedId, 0), 0);
    }
}

void TriggerLanguagePicker(HWND h) {
    const wchar_t* langNames[] = {
        L"Auto Detect",
        L"AutoHotkey",
        L"Batch",
        L"C", L"C#", L"C++",
        L"CSS",
        L"ENV",
        L"Go",
        L"HTML",
        L"INI", L"Inno Setup",
        L"Java", L"JavaScript", L"JSON",
        L"Markdown",
        L"Plain Text",
        L"PowerShell", L"Python",
        L"Resource", L"Ruby", L"Rust",
        L"SCSS", L"Shell", L"SQL",
        L"TOML", L"TypeScript",
        L"XML",
        L"YAML"
    };
    const char* langIds[] = {
        "",
        "cpp",
        "cpp",
        "cpp", "cpp", "cpp",
        "css",
        "cpp",
        "cpp",
        "hypertext",
        "cpp", "cpp",
        "cpp", "cpp", "cpp",
        "markdown",
        "null",
        "cpp", "python",
        "cpp", "cpp", "cpp",
        "css", "cpp", "cpp",
        "cpp", "cpp",
        "hypertext",
        "cpp"
    };
    int numLangs = sizeof(langNames) / sizeof(langNames[0]);

    std::string currentLang = "";
    if (activeTabIndex < tabs.size()) {
        currentLang = tabs[activeTabIndex].manualLanguage;
    }

    std::vector<PopupMenuItem> items;
    for (int i = 0; i < numLangs; ++i) {
        bool isChecked = (i == 0 && currentLang.empty()) || (!currentLang.empty() && currentLang == langIds[i] && i > 0);
        if (i == 0 && !currentLang.empty()) isChecked = false;
        items.push_back({ langNames[i], IDM_LANG_BASE + i, false, isChecked, L"" });
    }

    RECT rc; GetClientRect(h, &rc);
    POINT pt = { rc.right - 10, rc.bottom - 24 };
    ClientToScreen(h, &pt);

    int selectedId = ShowCustomPopupMenu(h, pt.x, pt.y, items, true);
    if (selectedId >= IDM_LANG_BASE && selectedId < IDM_LANG_BASE + numLangs) {
        int idx = selectedId - IDM_LANG_BASE;
        if (activeTabIndex < tabs.size()) {
            tabs[activeTabIndex].manualLanguage = langIds[idx];
            ApplySyntax();
            UpdateUI(h);
            InvalidateRect(h, NULL, FALSE);
        }
    }
}

void PaintSearchBar(HWND h, HDC hdc, const RECT& rc) {
    RECT pad = GetPad(h);
    bool inlineReplace = (rc.right - pad.right - pad.left > 1230);
    int topY = pad.top + (showTopBar ? 70 : 36);
    int height = replaceVisible ? (inlineReplace ? 36 : 72) : 36;
    
    // Background
    FillRectColor(hdc, { 0, topY, rc.right, topY + height }, theme.tabBg);
    
    // Bottom border
    FillRectColor(hdc, { pad.left, topY + height - 1, rc.right - pad.right, topY + height }, theme.border);
    
    // Draw Border around Edit Box
    RECT rcSearchBorder = { pad.left + 8, topY + 7, pad.left + 8 + 332, topY + 7 + 22 };
    HBRUSH hBr = CreateSolidBrush(theme.border);
    FrameRect(hdc, &rcSearchBorder, hBr);
    DeleteObject(hBr);
    
    // Match counter text (e.g. 25/46)
    wchar_t cBuf[32];
    if (totalMatchesCount > 0) {
        swprintf_s(cBuf, L"%d/%d", currentMatchIndex, totalMatchesCount);
    } else {
        int len = GetWindowTextLengthW(hwndSearchEdit);
        if (len > 0) {
            swprintf_s(cBuf, L"0/0");
        } else {
            cBuf[0] = L'\0';
        }
    }
    
    if (cBuf[0] != L'\0') {
        SetTextColor(hdc, theme.textDim); // Muted gray
        SetBkMode(hdc, TRANSPARENT);
        HFONT oldFont = (HFONT)SelectObject(hdc, hSmallFont);
        RECT rcCounter = { pad.left + 350, topY, pad.left + 420, topY + 36 };
        DrawTextW(hdc, cBuf, -1, &rcCounter, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        SelectObject(hdc, oldFont);
    }
    
    // Prev Arrow (Segoe MDL2 Left Arrow / Chevron Left)
    RECT rcPrev = { rc.right - pad.right - 275, topY + 6, rc.right - pad.right - 251, topY + 30 };
    DrawBtn(hdc, rcPrev, L"\uE00E", hoverElement == HOVER_SEARCH_PREV, pressedElement == HOVER_SEARCH_PREV);
    
    // Next Arrow (Segoe MDL2 Right Arrow / Chevron Right)
    RECT rcNext = { rc.right - pad.right - 246, topY + 6, rc.right - pad.right - 222, topY + 30 };
    DrawBtn(hdc, rcNext, L"\uE00F", hoverElement == HOVER_SEARCH_NEXT, pressedElement == HOVER_SEARCH_NEXT);
    
    // Select All
    RECT rcSelectAll = { rc.right - pad.right - 212, topY + 6, rc.right - pad.right - 132, topY + 30 };
    DrawBtn(hdc, rcSelectAll, L"Select All", hoverElement == HOVER_SEARCH_SELECT_ALL, pressedElement == HOVER_SEARCH_SELECT_ALL, false, hUIFont);
    
    // Replace Toggle
    RECT rcReplaceToggle = { rc.right - pad.right - 122, topY + 6, rc.right - pad.right - 42, topY + 30 };
    DrawBtn(hdc, rcReplaceToggle, L"Replace...", hoverElement == HOVER_SEARCH_REPLACE_TOGGLE, pressedElement == HOVER_SEARCH_REPLACE_TOGGLE, false, hUIFont, false, replaceVisible);
    
    // Close button
    RECT rcClose = { rc.right - pad.right - 32, topY + 6, rc.right - pad.right - 8, topY + 30 };
    DrawBtn(hdc, rcClose, L"\uE711", hoverElement == HOVER_SEARCH_CLOSE, pressedElement == HOVER_SEARCH_CLOSE, true, hIconFont, false, false, false);
    
    // Row 2 (Replace)
    if (replaceVisible) {
        int repX = inlineReplace ? pad.left + 420 : pad.left + 8;
        int repY = inlineReplace ? topY : topY + 36;
        
        // Draw Border around Replace Edit Box
        RECT rcReplaceBorder = { repX, repY + 7, repX + 332, repY + 7 + 22 };
        HBRUSH hBrRep = CreateSolidBrush(theme.border);
        FrameRect(hdc, &rcReplaceBorder, hBrRep);
        DeleteObject(hBrRep);
        
        int btn1X = inlineReplace ? repX + 342 : pad.left + 350;
        // Replace Next Button
        RECT rcRepNext = { btn1X, repY + 6, btn1X + 80, repY + 30 };
        DrawBtn(hdc, rcRepNext, L"Replace", hoverElement == HOVER_REPLACE_NEXT, pressedElement == HOVER_REPLACE_NEXT, false, hUIFont);
        
        int btn2X = inlineReplace ? btn1X + 90 : pad.left + 440;
        // Replace All Button
        RECT rcRepAll = { btn2X, repY + 6, btn2X + 90, repY + 30 };
        DrawBtn(hdc, rcRepAll, L"Replace All", hoverElement == HOVER_REPLACE_ALL, pressedElement == HOVER_REPLACE_ALL, false, hUIFont);
    }
}

void TriggerSearchDialog(HWND h) {
    searchVisible = !searchVisible;
    if (searchVisible) {
        ShowWindow(hwndSearchEdit, SW_SHOW);
        if (replaceVisible) {
            ShowWindow(hwndReplaceEdit, SW_SHOW);
        }
        SetFocus(hwndSearchEdit);
        SendMessage(hwndSearchEdit, EM_SETSEL, 0, -1);
        UpdateSearchMatches();
    } else {
        replaceVisible = false;
        ShowWindow(hwndSearchEdit, SW_HIDE);
        ShowWindow(hwndReplaceEdit, SW_HIDE);
        SetFocus(hwndScintilla);
    }
    RECT rc; GetClientRect(h, &rc);
    SendMessage(h, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
    UpdateUI(h);
}

void OnElementClicked(HWND h, HoverElement el) {
    if (el == HOVER_CLOSE) PostMessage(h, WM_CLOSE, 0, 0);
    else if (el == HOVER_MAXIMIZE) {
        if (zenMode) ToggleZenMode(h);
        else ShowWindow(h, IsZoomed(h) ? SW_RESTORE : SW_MAXIMIZE);
    }
    else if (el == HOVER_MINIMIZE) ShowWindow(h, SW_MINIMIZE);
    else if (el == HOVER_UNDO) { if (Sci(SCI_CANUNDO)) Sci(SCI_UNDO); }
    else if (el == HOVER_REDO) { if (Sci(SCI_CANREDO)) Sci(SCI_REDO); }
    else if (el == HOVER_ADD_TAB) CreateNewTab(h);
    else if (el >= HOVER_TAB_BASE && el < HOVER_TAB_CLOSE_BASE) SwitchToTab(h, el - HOVER_TAB_BASE);
    else if (el >= HOVER_TAB_CLOSE_BASE && el < HOVER_SETTINGS) CloseTab(h, el - HOVER_TAB_CLOSE_BASE);
    else if (el == HOVER_SEARCH) TriggerSearchDialog(h);
    else if (el == HOVER_SETTINGS) TriggerSettingsMenu(h);
    else if (el == HOVER_STATUS_EOL) {
        int eolMode = Sci(SCI_GETEOLMODE);
        int newMode = (eolMode == SC_EOL_CRLF) ? SC_EOL_LF : SC_EOL_CRLF;
        Sci(SCI_SETEOLMODE, newMode);
        Sci(SCI_CONVERTEOLS, newMode);
        SyncLineNumbers(true);
        SaveSession();
        UpdateUI(h);
    }
    else if (el == HOVER_STATUS_LANG) {
        TriggerLanguagePicker(h);
    }
    else if (el == HOVER_SEARCH_PREV) SearchPrev();
    else if (el == HOVER_SEARCH_NEXT) SearchNext();
    else if (el == HOVER_SEARCH_SELECT_ALL) SearchSelectAll();
    else if (el == HOVER_SEARCH_REPLACE_TOGGLE) {
        replaceVisible = !replaceVisible;
        RECT rc; GetClientRect(h, &rc);
        SendMessage(h, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
        if (replaceVisible && hwndReplaceEdit) {
            SetFocus(hwndReplaceEdit);
        } else {
            SetFocus(hwndSearchEdit);
        }
        UpdateUI(h);
    }
    else if (el == HOVER_SEARCH_CLOSE) {
        searchVisible = false;
        replaceVisible = false;
        ShowWindow(hwndSearchEdit, SW_HIDE);
        ShowWindow(hwndReplaceEdit, SW_HIDE);
        SetFocus(hwndScintilla);
        RECT rc; GetClientRect(h, &rc);
        SendMessage(h, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
        UpdateUI(h);
    }
    else if (el == HOVER_REPLACE_NEXT) SearchReplace();
    else if (el == HOVER_REPLACE_ALL) SearchReplaceAll();
    else if (el == HOVER_FILE_NAME) {
        bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        RunCurrentFile(h, isShift);
    }
    else if (el >= HOVER_PATH_PART_BASE) {
        size_t idx = el - HOVER_PATH_PART_BASE;
        if (idx >= 0 && idx < g_pathParts.size()) {
            ShellExecuteW(NULL, L"open", g_pathParts[idx].fullPath.c_str(), NULL, NULL, SW_SHOW);
        }
    }
}

void TriggerTabRename(HWND h, int index) {
    if (index < 0 || index >= tabs.size()) return;
    RECT rc; GetClientRect(h, &rc); RECT pad = GetPad(h);
    int totalW = 0;
    for (size_t i = 0; i < tabs.size(); ++i) totalW += GetTabWidth(i);
    int startX = pad.left + 70;
    int maxTabRight = rc.right - pad.right - 135;
    bool overflow = (startX + totalW > maxTabRight);
    int tabLimit = overflow ? (maxTabRight - 30) : maxTabRight;
    
    int curX = startX;
    for (size_t i = 0; i < tabs.size(); ++i) {
        int tabW = GetTabWidth(i);
        if (curX >= tabLimit) break;
        if (i == index) {
            tabRenameIndex = index;
            int renderW = min(tabW, tabLimit - curX);
            if (renderW > 25 && hwndTabRenameEdit) {
                SetWindowTextW(hwndTabRenameEdit, tabs[i].title.c_str());
                SetWindowPos(hwndTabRenameEdit, HWND_TOP, curX + 10, pad.top + 8, renderW - 35, 20, SWP_SHOWWINDOW);
                SetFocus(hwndTabRenameEdit);
                SendMessage(hwndTabRenameEdit, EM_SETSEL, 0, -1);
            }
            return;
        }
        curX += tabW;
    }
}

static WINDOWPLACEMENT g_wpPrev = { sizeof(g_wpPrev) };

void ToggleZenMode(HWND hwnd) {
    zenMode = !zenMode;
    if (zenMode) {
        zenTopVisible = false;
        zenBottomVisible = false;
        zenTopProgress = 0.0f;
        zenBottomProgress = 0.0f;
        zenTopAnimTargetProgress = 0.0f;
        zenBottomAnimTargetProgress = 0.0f;
    } else {
        zenTopVisible = true;
        zenBottomVisible = true;
        zenTopProgress = 1.0f;
        zenBottomProgress = 1.0f;
        zenTopAnimTargetProgress = 1.0f;
        zenBottomAnimTargetProgress = 1.0f;
        KillTimer(hwnd, 4);
        timeEndPeriod(1);
    }
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (zenMode) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(hwnd, &g_wpPrev) && GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &g_wpPrev);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    UpdateUI(hwnd);
}
