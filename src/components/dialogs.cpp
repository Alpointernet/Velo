#include "../globals.h"
#include "dialogs.h"
#include "editor.h"
#include "ui_drawing.h"
#include "tabmanager.h"
#include "theme.h"

LRESULT CALLBACK CustomDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CustomDialogData* data = (CustomDialogData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!data) {
        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            data = (CustomDialogData*)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)data);
        } else {
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }
    
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // Draw background
            FillRectColor(hdc, rc, 0x1F1A18);
            
            // Draw border
            HBRUSH hBrBrd = CreateSolidBrush(0x3C312C);
            FrameRect(hdc, &rc, hBrBrd);
            DeleteObject(hBrBrd);
            
            // Top accent line (orange, 3px height)
            RECT rcAccent = { 0, 0, rc.right, 3 };
            HBRUSH hBrAcc = CreateSolidBrush(0xFF8B52);
            FillRect(hdc, &rcAccent, hBrAcc);
            DeleteObject(hBrAcc);
            
            // Draw title text
            RECT rcTitle = { 20, 15, rc.right - 20, 40 };
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, 0xFFFFFF); // White
            HFONT oldFont = (HFONT)SelectObject(hdc, hUIFont ? hUIFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            DrawTextW(hdc, data->title.c_str(), -1, &rcTitle, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            
            // Draw message body
            RECT rcText = { 20, 50, rc.right - 20, rc.bottom - 60 };
            SetTextColor(hdc, 0xBFB2AB); // Light gray/brown text
            SelectObject(hdc, hSmallFont ? hSmallFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            DrawTextW(hdc, data->message.c_str(), -1, &rcText, DT_WORDBREAK | DT_LEFT);
            
            // Draw Buttons
            SelectObject(hdc, hUIFont ? hUIFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            for (int i = 0; i < (int)data->buttons.size(); ++i) {
                const auto& btn = data->buttons[i];
                bool hover = (data->hoveredButtonIndex == i);
                bool press = (data->pressedButtonIndex == i);
                
                COLORREF bgCol = 0x2B2521; // Dark default button bg
                COLORREF borderCol = 0x3C312C;
                COLORREF textCol = 0xBFB2AB;
                
                if (press) {
                    bgCol = 0x51443E;
                    textCol = 0xFFFFFF;
                } else if (hover) {
                    bgCol = 0x3C312C;
                    textCol = 0xFFFFFF;
                }
                
                // Highlight the primary button (index 0, e.g. "Yes" or "OK") with our theme accent color
                if (i == 0 && !hover && !press) {
                    borderCol = 0xFF8B52; // Orange accent border
                }
                
                FillRectColor(hdc, btn.rect, bgCol);
                
                HBRUSH hBr = CreateSolidBrush(borderCol);
                FrameRect(hdc, &btn.rect, hBr);
                DeleteObject(hBr);
                
                SetTextColor(hdc, textCol);
                DrawTextW(hdc, btn.label.c_str(), -1, (RECT*)&btn.rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            }
            
            SelectObject(hdc, oldFont);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_NCHITTEST: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            ScreenToClient(hwnd, &pt);
            
            // Check if pt is inside any button
            for (const auto& btn : data->buttons) {
                if (PtInRect(&btn.rect, pt)) {
                    return HTCLIENT;
                }
            }
            return HTCAPTION; // Allow dragging from anywhere else!
        }
        case WM_MOUSEMOVE: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            int hovered = -1;
            for (int i = 0; i < (int)data->buttons.size(); ++i) {
                if (PtInRect(&data->buttons[i].rect, pt)) {
                    hovered = i;
                    break;
                }
            }
            
            if (hovered != data->hoveredButtonIndex) {
                data->hoveredButtonIndex = hovered;
                InvalidateRect(hwnd, NULL, FALSE);
                
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
            return 0;
        }
        case WM_MOUSELEAVE: {
            if (data->hoveredButtonIndex != -1) {
                data->hoveredButtonIndex = -1;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            int pressed = -1;
            for (int i = 0; i < (int)data->buttons.size(); ++i) {
                if (PtInRect(&data->buttons[i].rect, pt)) {
                    pressed = i;
                    break;
                }
            }
            
            if (pressed != -1) {
                data->pressedButtonIndex = pressed;
                SetCapture(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (GetCapture() == hwnd) ReleaseCapture();
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            int clicked = -1;
            for (int i = 0; i < (int)data->buttons.size(); ++i) {
                if (PtInRect(&data->buttons[i].rect, pt)) {
                    clicked = i;
                    break;
                }
            }
            
            if (clicked == data->pressedButtonIndex && clicked != -1) {
                data->result = data->buttons[clicked].result;
                data->running = false;
            }
            
            data->pressedButtonIndex = -1;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_CLOSE: {
            data->result = IDCANCEL;
            data->running = false;
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int ShowCustomMessageBox(HWND hwndParent, const std::wstring& message, const std::wstring& title, UINT type) {
    static bool classRegistered = false;
    HINSTANCE hInst = GetModuleHandleW(NULL);
    if (!classRegistered) {
        WNDCLASSW wc = { 0 };
        wc.lpfnWndProc = CustomDialogProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.lpszClassName = L"CustomMessageBoxClass";
        RegisterClassW(&wc);
        classRegistered = true;
    }
    
    CustomDialogData data;
    data.message = message;
    data.title = title;
    data.type = type;
    data.hoveredButtonIndex = -1;
    data.pressedButtonIndex = -1;
    data.result = IDCANCEL;
    data.running = true;
    
    if (type == MB_YESNOCANCEL) {
        data.buttons = {
            { {}, L"Yes", IDYES },
            { {}, L"No", IDNO },
            { {}, L"Cancel", IDCANCEL }
        };
    } else if (type == MB_YESNO) {
        data.buttons = {
            { {}, L"Yes", IDYES },
            { {}, L"No", IDNO }
        };
    } else { // MB_OK
        data.buttons = {
            { {}, L"OK", IDOK }
        };
    }
    
    int dlgW = 400;
    
    // Calculate dynamic height based on text wrapping
    int textH = 30; // fallback
    HDC hdc = GetDC(hwndParent);
    if (hdc) {
        HFONT oldFont = (HFONT)SelectObject(hdc, hSmallFont ? hSmallFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
        RECT rcMeasure = { 0, 0, dlgW - 40, 0 };
        DrawTextW(hdc, message.c_str(), -1, &rcMeasure, DT_WORDBREAK | DT_CALCRECT);
        textH = rcMeasure.bottom - rcMeasure.top;
        SelectObject(hdc, oldFont);
        ReleaseDC(hwndParent, hdc);
    }
    
    int dlgH = 50 + textH + 60;
    if (dlgH < 150) dlgH = 150;
    
    // Setup button rects dynamically at the bottom of the dialog
    int btnW = 85, btnH = 28;
    int btnY = dlgH - 45;
    int numBtns = (int)data.buttons.size();
    for (int i = 0; i < numBtns; ++i) {
        int xStart = dlgW - 20 - (numBtns * btnW + (numBtns - 1) * 10);
        int btnX = xStart + i * (btnW + 10);
        data.buttons[i].rect = { btnX, btnY, btnX + btnW, btnY + btnH };
    }
    
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    if (hwndParent) {
        RECT rcParent;
        GetWindowRect(hwndParent, &rcParent);
        x = rcParent.left + (rcParent.right - rcParent.left - dlgW) / 2;
        y = rcParent.top + (rcParent.bottom - rcParent.top - dlgH) / 2;
    }
    
    HWND hwndDlg = CreateWindowExW(
        0,
        L"CustomMessageBoxClass",
        title.c_str(),
        WS_POPUP | WS_SYSMENU | WS_CLIPSIBLINGS,
        x, y, dlgW, dlgH,
        hwndParent, NULL, hInst, &data
    );
    
    if (!hwndDlg) return IDCANCEL;
    
    if (hwndParent) EnableWindow(hwndParent, FALSE);
    
    ShowWindow(hwndDlg, SW_SHOW);
    UpdateWindow(hwndDlg);
    
    MSG msg;
    while (data.running && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    DestroyWindow(hwndDlg);
    
    if (hwndParent) {
        EnableWindow(hwndParent, TRUE);
        SetFocus(hwndParent);
    }
    
    return data.result;
}

struct CustomPopupData {
    std::vector<PopupMenuItem> items;
    int hoveredIndex;
    int resultId;
    bool running;
};

LRESULT CALLBACK CustomPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CustomPopupData* data = (CustomPopupData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!data) {
        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            data = (CustomPopupData*)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)data);
        } else {
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }
    
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // Create double buffer
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
            
            // Draw background
            FillRectColor(memDC, rc, 0x1F1A18);
            
            // Draw border
            HBRUSH hBrBrd = CreateSolidBrush(0x3C312C);
            FrameRect(memDC, &rc, hBrBrd);
            DeleteObject(hBrBrd);
            
            // Top accent line (orange, 2px height)
            RECT rcAccent = { 0, 0, rc.right, 2 };
            HBRUSH hBrAcc = CreateSolidBrush(0xFF8B52);
            FillRect(memDC, &rcAccent, hBrAcc);
            DeleteObject(hBrAcc);
            
            SetBkMode(memDC, TRANSPARENT);
            HFONT oldFont = (HFONT)SelectObject(memDC, hUIFont ? hUIFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            
            int yOffset = 2; // start after top accent line
            for (int i = 0; i < (int)data->items.size(); ++i) {
                const auto& item = data->items[i];
                if (item.isSeparator) {
                    RECT rcSepLine = { 10, yOffset + 3, rc.right - 10, yOffset + 4 };
                    FillRectColor(memDC, rcSepLine, 0x3C312C);
                    yOffset += 8;
                } else {
                    RECT rcItem = { 1, yOffset, rc.right - 1, yOffset + 28 };
                    bool hover = (data->hoveredIndex == i);
                    
                    if (hover) {
                        FillRectColor(memDC, rcItem, 0x2B2521); // Hover bg matches theme
                    }
                    
                    // Draw checked indicator (orange square)
                    if (item.isChecked) {
                        RECT rcCheck = { 12, yOffset + 10, 20, yOffset + 18 };
                        HBRUSH hBrChk = CreateSolidBrush(0xFF8B52);
                        FillRect(memDC, &rcCheck, hBrChk);
                        DeleteObject(hBrChk);
                    }
                    
                    // Draw label text
                    RECT rcText = { 32, yOffset, rc.right - 80, yOffset + 28 };
                    SetTextColor(memDC, hover ? 0xFFFFFF : 0xBFB2AB);
                    DrawTextW(memDC, item.label.c_str(), -1, &rcText, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
                    
                    // Draw shortcut text
                    if (!item.shortcut.empty()) {
                        RECT rcShortcut = { rc.right - 90, yOffset, rc.right - 15, yOffset + 28 };
                        SetTextColor(memDC, hover ? 0xBFB2AB : 0x6E5E56);
                        DrawTextW(memDC, item.shortcut.c_str(), -1, &rcShortcut, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
                    }
                    
                    yOffset += 28;
                }
            }
            
            SelectObject(memDC, oldFont);
            
            // Copy from memory buffer to screen
            BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT rc; GetClientRect(hwnd, &rc);
            int hoverIdx = -1;
            if (PtInRect(&rc, pt)) {
                int yOffset = 2;
                for (int i = 0; i < (int)data->items.size(); ++i) {
                    int h = data->items[i].isSeparator ? 8 : 28;
                    if (pt.y >= yOffset && pt.y < yOffset + h && !data->items[i].isSeparator) {
                        hoverIdx = i;
                        break;
                    }
                    yOffset += h;
                }
            }
            if (hoverIdx != data->hoveredIndex) {
                data->hoveredIndex = hoverIdx;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT rc; GetClientRect(hwnd, &rc);
            if (PtInRect(&rc, pt)) {
                int yOffset = 2;
                for (const auto& item : data->items) {
                    int h = item.isSeparator ? 8 : 28;
                    if (pt.y >= yOffset && pt.y < yOffset + h && !item.isSeparator) {
                        data->resultId = item.id;
                        data->running = false;
                        break;
                    }
                    yOffset += h;
                }
            } else {
                data->running = false;
            }
            return 0;
        }
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            data->running = false;
            return 0;
        }
        case WM_ACTIVATEAPP: {
            if (!wParam) {
                data->running = false;
            }
            return 0;
        }
        case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int ShowCustomPopupMenu(HWND hwndParent, int x, int y, const std::vector<PopupMenuItem>& items, bool bottomAlign) {
    static bool classRegistered = false;
    HINSTANCE hInst = GetModuleHandleW(NULL);
    if (!classRegistered) {
        WNDCLASSW wc = { 0 };
        wc.lpfnWndProc = CustomPopupProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.lpszClassName = L"CustomPopupMenuClass";
        wc.style = CS_SAVEBITS | CS_HREDRAW | CS_VREDRAW;
        RegisterClassW(&wc);
        classRegistered = true;
    }
    
    CustomPopupData data;
    data.items = items;
    data.hoveredIndex = -1;
    data.resultId = 0;
    data.running = true;
    
    int totalH = 4; // top and bottom padding
    for (const auto& item : items) {
        totalH += item.isSeparator ? 8 : 28;
    }
    
    int menuW = 240;
    int menuH = totalH;
    int menuX = x;
    int menuY = bottomAlign ? (y - menuH) : y;
    
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (menuX + menuW > screenW) menuX = screenW - menuW;
    if (menuY + menuH > screenH) menuY = screenH - menuH;
    if (menuX < 0) menuX = 0;
    if (menuY < 0) menuY = 0;
    
    HWND hwndMenu = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"CustomPopupMenuClass",
        L"",
        WS_POPUP,
        menuX, menuY, menuW, menuH,
        hwndParent, NULL, hInst, &data
    );
    
    if (!hwndMenu) return 0;
    
    SetCapture(hwndMenu);
    
    ShowWindow(hwndMenu, SW_SHOW);
    UpdateWindow(hwndMenu);
    
    MSG msg;
    while (data.running && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    if (GetCapture() == hwndMenu) {
        ReleaseCapture();
    }
    
    DestroyWindow(hwndMenu);
    
    return data.resultId;
}

struct SettingsControlBtn {
    RECT rect;
    std::wstring label;
    int settingType; // 1: Auto-Close Braces, 2: Indent Guides, 3: Caret Line, 4: Whitespace, 5: Top Bar, 6: OK
    int value;       // parameter value
};

struct CustomSettingsData {
    bool tempAutoCloseBraces;
    bool tempShowIndentGuides;
    bool tempShowWhitespace;
    bool tempCaretStyleBlock;
    bool tempShowTopBar;
    int hoveredIndex;
    int pressedIndex;
    bool running;
    bool openThemeFile;
    std::vector<SettingsControlBtn> buttons;
};

LRESULT CALLBACK CustomSettingsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CustomSettingsData* data = (CustomSettingsData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!data) {
        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            data = (CustomSettingsData*)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)data);
        } else {
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }
    
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // Double buffer paint
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
            
            // Background
            FillRectColor(memDC, rc, 0x1F1A18);
            
            // Border
            HBRUSH hBrBrd = CreateSolidBrush(0x3C312C);
            FrameRect(memDC, &rc, hBrBrd);
            DeleteObject(hBrBrd);
            
            // Top Accent Orange Strip
            RECT rcAccent = { 0, 0, rc.right, 3 };
            HBRUSH hBrAcc = CreateSolidBrush(0xFF8B52);
            FillRect(memDC, &rcAccent, hBrAcc);
            DeleteObject(hBrAcc);
            
            // Title
            RECT rcTitle = { 20, 15, rc.right - 20, 40 };
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, 0xFFFFFF);
            HFONT oldFont = (HFONT)SelectObject(memDC, hUIFont ? hUIFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            DrawTextW(memDC, L"Settings", -1, &rcTitle, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            
            // Labels
            SelectObject(memDC, hSmallFont ? hSmallFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            SetTextColor(memDC, 0xBFB2AB);
            
            RECT rcLblBraces = { 20, 60, 200, 90 };
            DrawTextW(memDC, L"Auto-Close Braces", -1, &rcLblBraces, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            
            RECT rcLblGuides = { 20, 110, 200, 140 };
            DrawTextW(memDC, L"Indentation Guides", -1, &rcLblGuides, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            
            RECT rcLblWS = { 20, 160, 200, 190 };
            DrawTextW(memDC, L"Show Whitespace", -1, &rcLblWS, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

            RECT rcLblCaret = { 20, 210, 200, 240 };
            DrawTextW(memDC, L"Caret Cursor Style", -1, &rcLblCaret, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

            RECT rcLblTopBar = { 20, 260, 200, 290 };
            DrawTextW(memDC, L"File Top Bar", -1, &rcLblTopBar, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            
            RECT rcLblTheme = { 20, 310, 200, 340 };
            DrawTextW(memDC, L"Custom Theme", -1, &rcLblTheme, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            
            // Draw Control Buttons
            SelectObject(memDC, hUIFont ? hUIFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            for (int i = 0; i < (int)data->buttons.size(); ++i) {
                const auto& btn = data->buttons[i];
                bool hover = (data->hoveredIndex == i);
                bool press = (data->pressedIndex == i);
                
                // Determine if this button represents the active setting value
                bool active = false;
                if (btn.settingType == 1 && btn.value == (data->tempAutoCloseBraces ? 1 : 0)) active = true;
                else if (btn.settingType == 2 && btn.value == (data->tempShowIndentGuides ? 1 : 0)) active = true;
                else if (btn.settingType == 3 && btn.value == (data->tempShowWhitespace ? 1 : 0)) active = true;
                else if (btn.settingType == 4 && btn.value == (data->tempCaretStyleBlock ? 1 : 0)) active = true;
                else if (btn.settingType == 5 && btn.value == (data->tempShowTopBar ? 1 : 0)) active = true;
                
                COLORREF bgCol = 0x2B2521;
                COLORREF borderCol = active ? 0xFF8B52 : 0x3C312C;
                COLORREF textCol = active ? 0xFFFFFF : 0xBFB2AB;
                
                if (press) {
                    bgCol = 0x51443E;
                    textCol = 0xFFFFFF;
                } else if (hover) {
                    bgCol = 0x3C312C;
                    textCol = 0xFFFFFF;
                }
                
                FillRectColor(memDC, btn.rect, bgCol);
                
                HBRUSH hBr = CreateSolidBrush(borderCol);
                FrameRect(memDC, &btn.rect, hBr);
                DeleteObject(hBr);
                
                SetTextColor(memDC, textCol);
                DrawTextW(memDC, btn.label.c_str(), -1, (RECT*)&btn.rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            }
            
            SelectObject(memDC, oldFont);
            BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_NCHITTEST: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            ScreenToClient(hwnd, &pt);
            for (const auto& btn : data->buttons) {
                if (PtInRect(&btn.rect, pt)) return HTCLIENT;
            }
            return HTCAPTION; // Drag custom borderless window
        }
        case WM_MOUSEMOVE: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            int hovered = -1;
            for (int i = 0; i < (int)data->buttons.size(); ++i) {
                if (PtInRect(&data->buttons[i].rect, pt)) {
                    hovered = i;
                    break;
                }
            }
            if (hovered != data->hoveredIndex) {
                data->hoveredIndex = hovered;
                InvalidateRect(hwnd, NULL, FALSE);
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
            return 0;
        }
        case WM_MOUSELEAVE: {
            if (data->hoveredIndex != -1) {
                data->hoveredIndex = -1;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            int pressed = -1;
            for (int i = 0; i < (int)data->buttons.size(); ++i) {
                if (PtInRect(&data->buttons[i].rect, pt)) {
                    pressed = i;
                    break;
                }
            }
            if (pressed != -1) {
                data->pressedIndex = pressed;
                SetCapture(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (GetCapture() == hwnd) ReleaseCapture();
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            int clicked = -1;
            for (int i = 0; i < (int)data->buttons.size(); ++i) {
                if (PtInRect(&data->buttons[i].rect, pt)) {
                    clicked = i;
                    break;
                }
            }
            if (clicked == data->pressedIndex && clicked != -1) {
                const auto& btn = data->buttons[clicked];
                bool updated = false;
                if (btn.settingType == 1) { data->tempAutoCloseBraces = (btn.value == 1); autoCloseBraces = data->tempAutoCloseBraces; updated = true; }
                else if (btn.settingType == 2) { data->tempShowIndentGuides = (btn.value == 1); showIndentGuides = data->tempShowIndentGuides; updated = true; }
                else if (btn.settingType == 3) { data->tempShowWhitespace = (btn.value == 1); showWhitespace = data->tempShowWhitespace; updated = true; }
                else if (btn.settingType == 4) { data->tempCaretStyleBlock = (btn.value == 1); caretStyleBlock = data->tempCaretStyleBlock; updated = true; }
                else if (btn.settingType == 5) { data->tempShowTopBar = (btn.value == 1); showTopBar = data->tempShowTopBar; updated = true; }
                else if (btn.settingType == 7) { data->openThemeFile = true; data->running = false; } // Open Theme
                else if (btn.settingType == 6) data->running = false; // OK
                
                if (updated) {
                    if (hwndScintilla) {
                        StyleScintilla(hwndScintilla);
                    }
                    HWND hwndParent = GetParent(hwnd);
                    if (hwndParent) {
                        RECT rcP; GetClientRect(hwndParent, &rcP);
                        SendMessage(hwndParent, WM_SIZE, 0, MAKELPARAM(rcP.right, rcP.bottom));
                        UpdateUI(hwndParent);
                    }
                    SaveSession();
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            data->pressedIndex = -1;
            return 0;
        }
        case WM_CLOSE: {
            data->running = false;
            return 0;
        }
        case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowSettingsDialog(HWND hwndParent) {
    static bool classRegistered = false;
    HINSTANCE hInst = GetModuleHandleW(NULL);
    if (!classRegistered) {
        WNDCLASSW wc = { 0 };
        wc.lpfnWndProc = CustomSettingsProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.lpszClassName = L"CustomSettingsClass";
        RegisterClassW(&wc);
        classRegistered = true;
    }
    
    CustomSettingsData data;
    data.tempAutoCloseBraces = autoCloseBraces;
    data.tempShowIndentGuides = showIndentGuides;
    data.tempShowWhitespace = showWhitespace;
    data.tempCaretStyleBlock = caretStyleBlock;
    data.tempShowTopBar = showTopBar;
    data.hoveredIndex = -1;
    data.pressedIndex = -1;
    data.running = true;
    data.openThemeFile = false;
    
    data.buttons = {
        // Auto-Close Braces
        { { 210, 60, 290, 88 }, L"Enabled", 1, 1 },
        { { 300, 60, 380, 88 }, L"Disabled", 1, 0 },
        
        // Indent Guides
        { { 210, 110, 290, 138 }, L"Visible", 2, 1 },
        { { 300, 110, 380, 138 }, L"Hidden", 2, 0 },
        
        // Show Whitespace
        { { 210, 160, 290, 188 }, L"Visible", 3, 1 },
        { { 300, 160, 380, 188 }, L"Hidden", 3, 0 },

        // Caret Cursor Style
        { { 210, 210, 290, 238 }, L"Block", 4, 1 },
        { { 300, 210, 380, 238 }, L"Line", 4, 0 },

        // File Top Bar
        { { 210, 260, 290, 288 }, L"Visible", 5, 1 },
        { { 300, 260, 380, 288 }, L"Hidden", 5, 0 },
        
        // Custom Theme
        { { 210, 310, 380, 338 }, L"Open theme.json", 7, 0 },
        
        // OK Button
        { { 150, 370, 250, 405 }, L"OK", 6, 0 }
    };
    
    int dlgW = 400, dlgH = 430;
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    if (hwndParent) {
        RECT rcParent;
        GetWindowRect(hwndParent, &rcParent);
        x = rcParent.left + (rcParent.right - rcParent.left - dlgW) / 2;
        y = rcParent.top + (rcParent.bottom - rcParent.top - dlgH) / 2;
    }
    
    HWND hwndDlg = CreateWindowExW(
        0,
        L"CustomSettingsClass",
        L"Settings",
        WS_POPUP | WS_SYSMENU | WS_CLIPSIBLINGS,
        x, y, dlgW, dlgH,
        hwndParent, NULL, hInst, &data
    );
    
    if (!hwndDlg) return;
    
    if (hwndParent) EnableWindow(hwndParent, FALSE);
    
    ShowWindow(hwndDlg, SW_SHOW);
    UpdateWindow(hwndDlg);
    
    MSG msg;
    while (data.running && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    DestroyWindow(hwndDlg);
    
    if (hwndParent) {
        EnableWindow(hwndParent, TRUE);
        SetFocus(hwndParent);
    }
    
    autoCloseBraces = data.tempAutoCloseBraces;
    showIndentGuides = data.tempShowIndentGuides;
    showWhitespace = data.tempShowWhitespace;
    caretStyleBlock = data.tempCaretStyleBlock;
    showTopBar = data.tempShowTopBar;
    
    if (hwndScintilla) {
        StyleScintilla(hwndScintilla);
    }
    
    if (hwndParent) {
        RECT rcP; GetClientRect(hwndParent, &rcP);
        SendMessage(hwndParent, WM_SIZE, 0, MAKELPARAM(rcP.right, rcP.bottom));
        UpdateUI(hwndParent);
    }
    SaveSession();
    
    if (data.openThemeFile) {
        std::wstring themePath = GetThemePath();
        bool found = false;
        for (size_t i = 0; i < tabs.size(); ++i) {
            if (!_wcsicmp(tabs[i].filePath.c_str(), themePath.c_str())) {
                SwitchToTab(hwndParent, i);
                found = true;
                break;
            }
        }
        if (!found) {
            if (tabs[activeTabIndex].filePath.empty() && Sci(SCI_GETLENGTH) == 0 && !tabs[activeTabIndex].isModified) {
                LoadFileInActiveTab(hwndParent, themePath.c_str());
            } else {
                CreateNewTab(hwndParent, themePath);
                LoadFileInActiveTab(hwndParent, themePath.c_str());
            }
        }
    }
}
