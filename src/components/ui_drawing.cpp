#include "../globals.h"
#include "ui_drawing.h"
#include "editor.h"
#include "tabmanager.h"
#include "dialogs.h"

void FillRectColor(HDC hdc, const RECT& rc, COLORREF color) {
    SetBkColor(hdc, color);
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rc, NULL, 0, NULL);
}

RECT GetPad(HWND h) {
    if (!IsZoomed(h)) return { 1, 0, 1, 1 };
    RECT rc; GetWindowRect(h, &rc);
    MONITORINFO mi = { sizeof(mi) }; GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
    return { mi.rcWork.left - rc.left, mi.rcWork.top - rc.top, rc.right - mi.rcWork.right, rc.bottom - mi.rcWork.bottom };
}

void UpdateUI(HWND h) {
    RECT rc; GetClientRect(h, &rc); RECT pad = GetPad(h);
    int offset = searchVisible ? (replaceVisible ? 72 : 36) : 0;
    RECT rcTop = { 0, 0, rc.right, pad.top + 70 + EDITOR_TOP_MARGIN + offset }, rcStatus = { 0, rc.bottom - 24, rc.right, rc.bottom };
    InvalidateRect(h, &rcTop, FALSE); InvalidateRect(h, &rcStatus, FALSE);
    std::wstring title = (tabs[activeTabIndex].filePath.empty() ? L"Untitled" : tabs[activeTabIndex].filePath) + L" - Velo";
    SetWindowTextW(h, title.c_str());
}

void SyncScrollbars() {
    if (!hwndScintilla || !hwndVScroll || !hwndHScroll) return;
    RECT rcSci; GetClientRect(hwndScintilla, &rcSci);
    RECT pad = GetPad(hwndMain);
    int offset = 0;
    if (searchVisible) offset = replaceVisible ? 72 : 36;
    int sciX = pad.left, sciY = pad.top + 70 + offset + EDITOR_TOP_MARGIN;

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

    bool needV = (vTotal > vVis);
    bool needH = (Sci(SCI_GETSCROLLWIDTH) > hVis);

    if (needV) {
        int trackLen = rcSci.bottom - (needH ? CUSTOM_SB_SIZE : 0) - 4;
        int thumbLen = max(20, (int)((double)vVis / (maxVPos + vVis) * trackLen));
        int mScroll = maxVPos;
        int tPos = mScroll > 0 ? min((int)((double)vPos / mScroll * (trackLen - thumbLen)), trackLen - thumbLen) : 0;
        SetWindowPos(hwndVScroll, HWND_TOP, sciX + rcSci.right - CUSTOM_SB_SIZE - 2, sciY + 2 + tPos, CUSTOM_SB_SIZE, thumbLen, (scrollbarsVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOACTIVATE);
    } else ShowWindow(hwndVScroll, SW_HIDE);

    if (needH) {
        int hTotal = Sci(SCI_GETSCROLLWIDTH);
        int hPos = Sci(SCI_GETXOFFSET);
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
    COLORREF border = 0x003C312C; DwmSetWindowAttribute(hwnd, 34, &border, sizeof(border));
    if (HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32)) {
        if (auto SetMode = (int(WINAPI*)(int))GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135))) SetMode(1);
        if (auto AllowDark = (bool(WINAPI*)(HWND, bool))GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133))) AllowDark(hwnd, true);
        if (auto Flush = (void(WINAPI*)())GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136))) Flush();
    }
}

RECT GetEolRect(HWND h, HDC hdc, const RECT& rc) {
    RECT pad = GetPad(h);
    int pos = hwndScintilla ? Sci(SCI_GETCURRENTPOS) : 0;
    int line = hwndScintilla ? Sci(SCI_LINEFROMPOSITION, pos) + 1 : 1;
    int col = hwndScintilla ? Sci(SCI_GETCOLUMN, pos) + 1 : 1;
    int eolMode = hwndScintilla ? Sci(SCI_GETEOLMODE) : 0;
    const wchar_t* eol = (eolMode == SC_EOL_CRLF) ? L"CRLF" : ((eolMode == SC_EOL_CR) ? L"CR" : L"LF");
    const wchar_t* lang = L"Plain Text";
    if (activeTabIndex < tabs.size()) {
        std::wstring ext = tabs[activeTabIndex].filePath; size_t dot = ext.find_last_of(L'.');
        if (dot != std::wstring::npos) {
            std::wstring e = ext.substr(dot + 1);
            if (!_wcsicmp(e.c_str(), L"cpp") || !_wcsicmp(e.c_str(), L"h") || !_wcsicmp(e.c_str(), L"hpp") || !_wcsicmp(e.c_str(), L"c")) lang = L"C++";
            else if (!_wcsicmp(e.c_str(), L"py")) lang = L"Python";
            else if (!_wcsicmp(e.c_str(), L"js") || !_wcsicmp(e.c_str(), L"ts")) lang = L"JavaScript";
            else if (!_wcsicmp(e.c_str(), L"html") || !_wcsicmp(e.c_str(), L"htm") || !_wcsicmp(e.c_str(), L"xml")) lang = L"HTML";
            else if (!_wcsicmp(e.c_str(), L"json")) lang = L"JSON";
        }
    }
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

HoverElement HitTest(HWND h, POINT pt) {
    RECT rc; GetClientRect(h, &rc); RECT pad = GetPad(h);
    if (pt.y >= pad.top && pt.y < pad.top + 35) {
        if (pt.x >= rc.right - pad.right - 45 && pt.x < rc.right - pad.right) return HOVER_CLOSE;
        if (pt.x >= rc.right - pad.right - 90 && pt.x < rc.right - pad.right - 45) return HOVER_MAXIMIZE;
        if (pt.x >= rc.right - pad.right - 135 && pt.x < rc.right - pad.right - 90) return HOVER_MINIMIZE;
        if (pt.x >= pad.left + 10 && pt.x < pad.left + 35) return Sci(SCI_CANUNDO) ? HOVER_UNDO : HOVER_NONE;
        if (pt.x >= pad.left + 35 && pt.x < pad.left + 60) return Sci(SCI_CANREDO) ? HOVER_REDO : HOVER_NONE;
        
        int totalW = 0;
        for (size_t i = 0; i < tabs.size(); ++i) {
            totalW += GetTabWidth(i);
        }
        int startX = pad.left + 70;
        int maxTabRight = rc.right - pad.right - 135;
        bool overflow = (startX + totalW > maxTabRight);
        int tabLimit = overflow ? (maxTabRight - 30) : maxTabRight;
        
        int curX = startX;
        for (size_t i = 0; i < tabs.size(); ++i) {
            int tabW = GetTabWidth(i);
            if (curX >= tabLimit) break;
            if (pt.x >= curX && pt.x < curX + tabW) {
                if (pt.x >= tabLimit) break;
                if (pt.x >= curX + tabW - 25 && pt.x < curX + tabW - 5 && pt.y >= pad.top + 8 && pt.y < pad.top + 28) return (HoverElement)(HOVER_TAB_CLOSE_BASE + i);
                return (HoverElement)(HOVER_TAB_BASE + i);
            }
            curX += tabW;
        }
        int addTabX = overflow ? tabLimit : (startX + totalW);
        if (pt.x >= addTabX && pt.x < addTabX + 30) return HOVER_ADD_TAB;
    }
    if (pt.y >= rc.bottom - 24 && pt.y < rc.bottom) {
        if (pt.x >= pad.left && pt.x < pad.left + 30) return HOVER_SETTINGS;
        if (pt.x >= pad.left + 30 && pt.x < pad.left + 60) return HOVER_SEARCH;
        
        if (hwndScintilla) {
            HDC hdc = GetDC(h);
            RECT rcEol = GetEolRect(h, hdc, rc);
            ReleaseDC(h, hdc);
            
            int sbTop = rc.bottom - pad.bottom - 24;
            if (pt.x >= rcEol.left && pt.x < rcEol.right && pt.y >= sbTop && pt.y < sbTop + 24) {
                return HOVER_STATUS_EOL;
            }
        }
    }
    if (searchVisible && pt.y >= pad.top + 70 && pt.y < pad.top + 70 + (replaceVisible ? 72 : 36)) {
        int relY = pt.y - (pad.top + 70);
        if (relY < 36) {
            if (pt.x >= rc.right - pad.right - 275 && pt.x < rc.right - pad.right - 251 && pt.y >= pad.top + 70 + 6 && pt.y < pad.top + 70 + 30) return HOVER_SEARCH_PREV;
            if (pt.x >= rc.right - pad.right - 246 && pt.x < rc.right - pad.right - 222 && pt.y >= pad.top + 70 + 6 && pt.y < pad.top + 70 + 30) return HOVER_SEARCH_NEXT;
            if (pt.x >= rc.right - pad.right - 212 && pt.x < rc.right - pad.right - 132 && pt.y >= pad.top + 70 + 6 && pt.y < pad.top + 70 + 30) return HOVER_SEARCH_SELECT_ALL;
            if (pt.x >= rc.right - pad.right - 122 && pt.x < rc.right - pad.right - 42 && pt.y >= pad.top + 70 + 6 && pt.y < pad.top + 70 + 30) return HOVER_SEARCH_REPLACE_TOGGLE;
            if (pt.x >= rc.right - pad.right - 32 && pt.x < rc.right - pad.right - 8 && pt.y >= pad.top + 70 + 6 && pt.y < pad.top + 70 + 30) return HOVER_SEARCH_CLOSE;
        } else {
            if (pt.x >= pad.left + 350 && pt.x < pad.left + 430 && pt.y >= pad.top + 70 + 36 + 6 && pt.y < pad.top + 70 + 36 + 30) return HOVER_REPLACE_NEXT;
            if (pt.x >= pad.left + 440 && pt.x < pad.left + 530 && pt.y >= pad.top + 70 + 36 + 6 && pt.y < pad.top + 70 + 36 + 30) return HOVER_REPLACE_ALL;
        }
    }
    return HOVER_NONE;
}

void DrawBtn(HDC hdc, RECT rc, const wchar_t* text, bool hover, bool press, bool isClose, HFONT font, bool disabled, bool toggled, bool elevated) {
    COLORREF textCol = disabled ? 0x443630 : 0xD4D4D4;
    COLORREF bgCol = elevated ? 0x322A26 : 0;
    bool hasBg = elevated;
    if (!disabled) {
        if (isClose && hover) { bgCol = press ? 0xF1707A : 0xE81123; textCol = 0xFFFFFF; hasBg = true; }
        else if (toggled) { bgCol = hover ? 0x51443E : 0x3C312C; textCol = 0xFF8B52; hasBg = true; }
        else if (press) { bgCol = 0x51443E; if (!isClose) textCol = 0xFFFFFF; hasBg = true; }
        else if (hover) { bgCol = 0x3C312C; if (!isClose) textCol = 0xFFFFFF; hasBg = true; }
    } else {
        hasBg = false;
    }
    if (hasBg) FillRectColor(hdc, rc, bgCol);
    HFONT oldFont = font ? (HFONT)SelectObject(hdc, font) : NULL; int oldBk = SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, textCol);
    DrawTextW(hdc, text, -1, &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    SetBkMode(hdc, oldBk); if (oldFont) SelectObject(hdc, oldFont);
}

void PaintTopBar(HWND h, HDC hdc, const RECT& rc) {
    RECT pad = GetPad(h);
    FillRectColor(hdc, { 0, 0, rc.right, pad.top + 35 }, 0x1F1A18);
    
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
    for (size_t i = 0; i < tabs.size(); ++i) {
        int tabW = GetTabWidth(i);
        bool active = (i == activeTabIndex), hover = (hoverElement == HOVER_TAB_BASE + i || hoverElement == HOVER_TAB_CLOSE_BASE + i);
        
        FillRectColor(hdc, { curX, pad.top, curX + tabW, pad.top + 35 }, active ? 0x2B2521 : (hover ? 0x2C2825 : 0x1F1A18));
        FillRectColor(hdc, { curX + tabW - 1, pad.top, curX + tabW, pad.top + 35 }, 0x1F1A18);
        if (active) {
            FillRectColor(hdc, { curX, pad.top, curX + tabW - 1, pad.top + 2 }, 0xFF8B52);
            activeTabLeft = curX; activeTabRight = curX + tabW - 1;
        }
        
        RECT rcText = { curX + 10, pad.top, curX + tabW - 25, pad.top + 35 };
        SetTextColor(hdc, active ? 0xFFFFFF : 0xBFB2AB); SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc, tabs[i].title.c_str(), -1, &rcText, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        
        RECT rcClose = { curX + tabW - 22, pad.top + 8, curX + tabW - 6, pad.top + 28 };
        bool cHover = (hoverElement == HOVER_TAB_CLOSE_BASE + i), cPress = (pressedElement == HOVER_TAB_CLOSE_BASE + i);
        if (tabs[i].isModified && !cHover) {
            FillRectColor(hdc, { curX + tabW - 16, pad.top + 14, curX + tabW - 10, pad.top + 20 }, 0xBFB2AB);
        } else {
            DrawBtn(hdc, rcClose, L"\uE711", cHover, cPress, true, hIconFont, false, false, false);
        }
        curX += tabW;
    }
    
    // Clear clipping region so we can draw other components normally
    SelectClipRgn(hdc, NULL);
    DeleteObject(hRgn);
    
    if (activeTabRight > activeTabLeft) {
        // Only paint active tab lines if the active tab is at least partially visible
        if (activeTabLeft < tabLimit) {
            int rightLineLimit = min(activeTabRight, tabLimit);
            FillRectColor(hdc, { pad.left, pad.top + 35, activeTabLeft, pad.top + 36 }, 0x3C312C);
            FillRectColor(hdc, { rightLineLimit, pad.top + 35, rc.right - pad.right, pad.top + 36 }, 0x3C312C);
            FillRectColor(hdc, { activeTabLeft, pad.top + 35, rightLineLimit, pad.top + 36 }, 0x2B2521);
            FillRectColor(hdc, { activeTabLeft - 1, pad.top, activeTabLeft, pad.top + 36 }, 0x3C312C);
            if (activeTabRight < tabLimit) {
                FillRectColor(hdc, { activeTabRight, pad.top, activeTabRight + 1, pad.top + 36 }, 0x3C312C);
            }
        } else {
            FillRectColor(hdc, { pad.left, pad.top + 35, rc.right - pad.right, pad.top + 36 }, 0x3C312C);
        }
    } else {
        FillRectColor(hdc, { pad.left, pad.top + 35, rc.right - pad.right, pad.top + 36 }, 0x3C312C);
    }
    
    int addTabX = overflow ? tabLimit : (startX + totalW);
    DrawBtn(hdc, { addTabX, pad.top, addTabX + 30, pad.top + 35 }, L"\uE710", hoverElement == HOVER_ADD_TAB, pressedElement == HOVER_ADD_TAB, false, hIconFont, false, false, false);
    
    int btnX = rc.right - pad.right - 135;
    DrawBtn(hdc, { btnX, pad.top, btnX + 45, pad.top + 35 }, L"\uE921", hoverElement == HOVER_MINIMIZE, pressedElement == HOVER_MINIMIZE, false, hIconFont, false, false, false);
    DrawBtn(hdc, { btnX + 45, pad.top, btnX + 90, pad.top + 35 }, IsZoomed(h) ? L"\uE923" : L"\uE922", hoverElement == HOVER_MAXIMIZE, pressedElement == HOVER_MAXIMIZE, false, hIconFont, false, false, false);
    DrawBtn(hdc, { btnX + 90, pad.top, btnX + 135, pad.top + 35 }, L"\uE8BB", hoverElement == HOVER_CLOSE, pressedElement == HOVER_CLOSE, true, hIconFont, false, false, false);
    if (oldFont) SelectObject(hdc, oldFont);
}

void PaintHeaderBar(HWND h, HDC hdc, const RECT& rc) {
    RECT pad = GetPad(h);
    FillRectColor(hdc, { 0, pad.top + 36, rc.right, pad.top + 70 }, 0x2B2521);
    FillRectColor(hdc, { pad.left, pad.top + 69, rc.right - pad.right, pad.top + 70 }, 0x3C312C);
    std::wstring pathStr = (activeTabIndex < tabs.size()) ? tabs[activeTabIndex].filePath : L"", fileName = pathStr.empty() ? L"Untitled" : GetFileName(pathStr), parentDir = L"";
    size_t lastSlash = pathStr.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) parentDir = pathStr.substr(0, lastSlash + 1);
    SetBkMode(hdc, TRANSPARENT); HFONT oldFont = hUIFont ? (HFONT)SelectObject(hdc, hUIFont) : NULL;
    RECT rcMeasure = { 0 }; DrawTextW(hdc, fileName.c_str(), -1, &rcMeasure, DT_CALCRECT | DT_SINGLELINE);
    int fileW = rcMeasure.right - rcMeasure.left;
    RECT rcFile = { pad.left + 15, pad.top + 35, pad.left + 15 + fileW, pad.top + 70 };
    SetTextColor(hdc, 0xFFFFFF); DrawTextW(hdc, fileName.c_str(), -1, &rcFile, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    if (!parentDir.empty()) {
        RECT rcParent = { pad.left + 15 + fileW + 10, pad.top + 35, rc.right - pad.right - 320, pad.top + 70 };
        SetTextColor(hdc, 0x858585); DrawTextW(hdc, parentDir.c_str(), -1, &rcParent, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
    }
    if (oldFont) SelectObject(hdc, oldFont);
}

void PaintStatusBar(HWND h, HDC hdc, const RECT& rc) {
    RECT pad = GetPad(h);
    FillRectColor(hdc, { 0, 0, rc.right, 24 }, 0x1F1A18);
    FillRectColor(hdc, { pad.left, 0, rc.right - pad.right, 1 }, 0x3C312C);
    DrawBtn(hdc, { pad.left, 0, pad.left + 30, 24 }, L"\uE713", hoverElement == HOVER_SETTINGS, pressedElement == HOVER_SETTINGS, false, hIconFont, false, false, false);
    DrawBtn(hdc, { pad.left + 30, 0, pad.left + 60, 24 }, L"\uE721", hoverElement == HOVER_SEARCH, pressedElement == HOVER_SEARCH, false, hIconFont, false, false, false);
    if (searchVisible) FillRectColor(hdc, { pad.left + 30, 22, pad.left + 60, 24 }, 0xFF8B52);
    int rightLimit = rc.right - pad.right - 10;
    int pos = hwndScintilla ? Sci(SCI_GETCURRENTPOS) : 0;
    int line = hwndScintilla ? Sci(SCI_LINEFROMPOSITION, pos) + 1 : 1;
    int col = hwndScintilla ? Sci(SCI_GETCOLUMN, pos) + 1 : 1;
    int eolMode = hwndScintilla ? Sci(SCI_GETEOLMODE) : 0;
    const wchar_t* eol = (eolMode == SC_EOL_CRLF) ? L"CRLF" : ((eolMode == SC_EOL_CR) ? L"CR" : L"LF");
    const wchar_t* lang = L"Plain Text";
    if (activeTabIndex < tabs.size()) {
        std::wstring ext = tabs[activeTabIndex].filePath; size_t dot = ext.find_last_of(L'.');
        if (dot != std::wstring::npos) {
            std::wstring e = ext.substr(dot + 1);
            if (!_wcsicmp(e.c_str(), L"cpp") || !_wcsicmp(e.c_str(), L"h") || !_wcsicmp(e.c_str(), L"hpp") || !_wcsicmp(e.c_str(), L"c")) lang = L"C++";
            else if (!_wcsicmp(e.c_str(), L"py")) lang = L"Python";
            else if (!_wcsicmp(e.c_str(), L"js") || !_wcsicmp(e.c_str(), L"ts")) lang = L"JavaScript";
            else if (!_wcsicmp(e.c_str(), L"html") || !_wcsicmp(e.c_str(), L"htm") || !_wcsicmp(e.c_str(), L"xml")) lang = L"HTML";
            else if (!_wcsicmp(e.c_str(), L"json")) lang = L"JSON";
        }
    }
    HFONT oldFont = hSmallFont ? (HFONT)SelectObject(hdc, hSmallFont) : NULL;
    SetBkMode(hdc, TRANSPARENT);
    
    // Language
    SetTextColor(hdc, 0xBFB2AB);
    RECT rcLang = { 0, 0, rightLimit, 24 };
    DrawTextW(hdc, lang, -1, &rcLang, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    
    RECT rcLangMeasure = { 0 }; DrawTextW(hdc, lang, -1, &rcLangMeasure, DT_CALCRECT | DT_SINGLELINE);
    int wLang = rcLangMeasure.right - rcLangMeasure.left;
    rightLimit -= wLang;
    
    // Divider
    SetTextColor(hdc, 0x51443E);
    RECT rcDiv1 = { 0, 0, rightLimit, 24 };
    DrawTextW(hdc, L"   |   ", -1, &rcDiv1, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    
    RECT rcDivMeasure = { 0 }; DrawTextW(hdc, L"   |   ", -1, &rcDivMeasure, DT_CALCRECT | DT_SINGLELINE);
    int wDiv = rcDivMeasure.right - rcDivMeasure.left;
    rightLimit -= wDiv;
    
    // EOL Format (Highlights on hover!)
    bool isEolHovered = (hoverElement == HOVER_STATUS_EOL);
    SetTextColor(hdc, isEolHovered ? 0xFF8B52 : 0xBFB2AB);
    RECT rcEol = { 0, 0, rightLimit, 24 };
    DrawTextW(hdc, eol, -1, &rcEol, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    
    RECT rcEolMeasure = { 0 }; DrawTextW(hdc, eol, -1, &rcEolMeasure, DT_CALCRECT | DT_SINGLELINE);
    int wEol = rcEolMeasure.right - rcEolMeasure.left;
    rightLimit -= wEol;
    
    // Divider
    SetTextColor(hdc, 0x51443E);
    RECT rcDiv2 = { 0, 0, rightLimit, 24 };
    DrawTextW(hdc, L"   |   ", -1, &rcDiv2, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    rightLimit -= wDiv;
    
    // Encoding
    SetTextColor(hdc, 0xBFB2AB);
    RECT rcEnc = { 0, 0, rightLimit, 24 };
    DrawTextW(hdc, L"UTF-8", -1, &rcEnc, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    
    RECT rcEncMeasure = { 0 }; DrawTextW(hdc, L"UTF-8", -1, &rcEncMeasure, DT_CALCRECT | DT_SINGLELINE);
    int wEnc = rcEncMeasure.right - rcEncMeasure.left;
    rightLimit -= wEnc;
    
    // Divider
    SetTextColor(hdc, 0x51443E);
    RECT rcDiv3 = { 0, 0, rightLimit, 24 };
    DrawTextW(hdc, L"   |   ", -1, &rcDiv3, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    rightLimit -= wDiv;
    
    // Position Info
    SetTextColor(hdc, 0xBFB2AB);
    wchar_t posInfo[128]; swprintf_s(posInfo, L"Ln %d, Col %d", line, col);
    RECT rcPos = { 0, 0, rightLimit, 24 };
    DrawTextW(hdc, posInfo, -1, &rcPos, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    
    SelectObject(hdc, oldFont);
}

void TriggerSettingsMenu(HWND h) {
    std::vector<PopupMenuItem> items = {
        { L"New Tab", IDM_FILE_NEW, false, false, L"Ctrl+N" },
        { L"Open File...", IDM_FILE_OPEN, false, false, L"Ctrl+O" },
        { L"Save File", IDM_FILE_SAVE, false, false, L"Ctrl+S" },
        { L"Save File As...", IDM_FILE_SAVE_AS, false, false, L"" },
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

void PaintSearchBar(HWND h, HDC hdc, const RECT& rc) {
    RECT pad = GetPad(h);
    int topY = pad.top + 70;
    int height = replaceVisible ? 72 : 36;
    
    // Background
    FillRectColor(hdc, { 0, topY, rc.right, topY + height }, 0x1F1A18);
    
    // Bottom border
    FillRectColor(hdc, { pad.left, topY + height - 1, rc.right - pad.right, topY + height }, 0x3C312C);
    
    // Draw Border around Edit Box
    RECT rcSearchBorder = { pad.left + 8, topY + 7, pad.left + 8 + 332, topY + 7 + 22 };
    HBRUSH hBr = CreateSolidBrush(0x51443E);
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
        SetTextColor(hdc, 0x858585); // Muted gray
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
        // Draw Border around Replace Edit Box
        RECT rcReplaceBorder = { pad.left + 8, topY + 36 + 7, pad.left + 8 + 332, topY + 36 + 7 + 22 };
        HBRUSH hBrRep = CreateSolidBrush(0x51443E);
        FrameRect(hdc, &rcReplaceBorder, hBrRep);
        DeleteObject(hBrRep);
        
        // Replace Next Button
        RECT rcRepNext = { pad.left + 350, topY + 36 + 6, pad.left + 430, topY + 36 + 30 };
        DrawBtn(hdc, rcRepNext, L"Replace", hoverElement == HOVER_REPLACE_NEXT, pressedElement == HOVER_REPLACE_NEXT, false, hUIFont);
        
        // Replace All Button
        RECT rcRepAll = { pad.left + 440, topY + 36 + 6, pad.left + 530, topY + 36 + 30 };
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
    else if (el == HOVER_MAXIMIZE) ShowWindow(h, IsZoomed(h) ? SW_RESTORE : SW_MAXIMIZE);
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
}
