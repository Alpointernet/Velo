#include "globals.h"
#include "components/fonts.h"
#include "components/dialogs.h"
#include "components/editor.h"
#include "components/tabmanager.h"
#include "components/ui_drawing.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Define Global Variables
HWND hwndMain = NULL;
HWND hwndScintilla = NULL;
HWND hwndSearchEdit = NULL;
HWND hwndReplaceEdit = NULL;
HWND hwndVScroll = NULL;
HWND hwndHScroll = NULL;
HFONT hUIFont = NULL;
HFONT hIconFont = NULL;
HFONT hSmallFont = NULL;
bool searchVisible = false;
bool replaceVisible = false;
bool scrollbarsVisible = true;
bool vScrollHover = false;
bool vScrollDrag = false;
bool hScrollHover = false;
bool hScrollDrag = false;
int scrollDragStart = 0;
int scrollDragStartPos = 0;
int scrollDragMaxScroll = 0;
int scrollDragMaxTravel = 0;
int activeLineStart = -1;
int activeLineEnd = -1;
std::vector<Tab> tabs;
size_t activeTabIndex = 0;
HoverElement hoverElement = HOVER_NONE;
HoverElement pressedElement = HOVER_NONE;
WNDPROC oldSearchEditProc = NULL;
WNDPROC oldReplaceEditProc = NULL;
WNDPROC oldSciProc = NULL;

int currentMatchIndex = 0;
int totalMatchesCount = 0;
std::vector<std::pair<int, int>> searchMatches;

// Define Settings Globals
int editorFontSize = 12;
int editorTabWidth = 4;
bool autoSaveOnSwitch = false;
bool autoCloseBraces = true;
bool showIndentGuides = true;
bool showWhitespace = false;
bool caretStyleBlock = false;
bool isSavingSession = false;

LRESULT CALLBACK SearchEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            SearchNext();
            return 0;
        } else if (wParam == VK_ESCAPE) {
            TriggerSearchDialog(hwndMain);
            return 0;
        } else if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
    } else if (msg == WM_CHAR && (wParam == VK_RETURN || wParam == 1)) {
        return 0; // Prevent system ding on Enter or Ctrl+A
    }
    return CallWindowProcW(oldSearchEditProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ReplaceEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            SearchReplace();
            return 0;
        } else if (wParam == VK_ESCAPE) {
            TriggerSearchDialog(hwndMain);
            return 0;
        } else if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
    } else if (msg == WM_CHAR && (wParam == VK_RETURN || wParam == 1)) {
        return 0; // Prevent system ding on Enter or Ctrl+A
    }
    return CallWindowProcW(oldReplaceEditProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ScrollbarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    bool isVert = (hwnd == hwndVScroll);
    bool &hover = isVert ? vScrollHover : hScrollHover, &drag = isVert ? vScrollDrag : hScrollDrag;
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);
            FillRectColor(hdc, rc, (drag || hover) ? 0xFF8B52 : 0x51443E);
            EndPaint(hwnd, &ps); return 0;
        }
        case WM_MOUSEMOVE: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            if (drag && scrollDragMaxTravel > 0) {
                POINT scrPt = pt; ClientToScreen(hwnd, &scrPt);
                int delta = (isVert ? scrPt.y : scrPt.x) - scrollDragStart;
                int newPos = max(0, min(scrollDragStartPos + (int)((double)delta / scrollDragMaxTravel * scrollDragMaxScroll), scrollDragMaxScroll));
                Sci(isVert ? SCI_SETFIRSTVISIBLELINE : SCI_SETXOFFSET, newPos);
            } else if (!drag) {
                if (!hover) { hover = true; InvalidateRect(hwnd, NULL, FALSE); TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&tme); }
            }
            return 0;
        }
        case WM_MOUSELEAVE: { hover = false; InvalidateRect(hwnd, NULL, FALSE); return 0; }
        case WM_LBUTTONDOWN: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            drag = true; hover = true;
            POINT scrPt = pt; ClientToScreen(hwnd, &scrPt);
            scrollDragStart = isVert ? scrPt.y : scrPt.x;
            scrollDragStartPos = isVert ? Sci(SCI_GETFIRSTVISIBLELINE) : Sci(SCI_GETXOFFSET);

            RECT rcSci; GetClientRect(hwndScintilla, &rcSci);
            int marginW = GetTotalMarginWidth();
            int vLineH = Sci(SCI_TEXTHEIGHT);
            int vVis = vLineH > 0 ? rcSci.bottom / vLineH : 1;
            int hVis = rcSci.right - marginW;

            int vTotal = Sci(SCI_GETLINECOUNT);
            if (isVert) {
                scrollDragMaxScroll = max(0, vTotal - (int)(vVis * 0.6));
            } else {
                scrollDragMaxScroll = max(0, Sci(SCI_GETSCROLLWIDTH) - hVis);
            }

            bool needH = (Sci(SCI_GETSCROLLWIDTH) > hVis);
            bool needV = (vTotal > vVis);
            int trackLen = isVert ? (rcSci.bottom - (needH ? CUSTOM_SB_SIZE : 0) - 4) : (hVis - (needV ? CUSTOM_SB_SIZE : 0) - 4);

            RECT rcThumb; GetClientRect(hwnd, &rcThumb);
            scrollDragMaxTravel = max(1, trackLen - (isVert ? rcThumb.bottom : rcThumb.right));

            SetCapture(hwnd); InvalidateRect(hwnd, NULL, FALSE); return 0;
        }
        case WM_LBUTTONUP: { if (drag) { drag = false; ReleaseCapture(); InvalidateRect(hwnd, NULL, FALSE); } return 0; }
        case WM_MOUSEWHEEL: { SendMessage(hwndScintilla, msg, wParam, lParam); return 0; }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SciSubProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_BACK && autoCloseBraces) {
        int sSel = Sci(SCI_GETSELECTIONSTART);
        int eSel = Sci(SCI_GETSELECTIONEND);
        if (sSel == eSel) {
            int pos = Sci(SCI_GETCURRENTPOS);
            if (pos > 0) {
                char prev = (char)Sci(SCI_GETCHARAT, pos - 1);
                char next = (char)Sci(SCI_GETCHARAT, pos);
                if ((prev == '(' && next == ')') ||
                    (prev == '{' && next == '}') ||
                    (prev == '[' && next == ']') ||
                    (prev == '"' && next == '"') ||
                    (prev == '\'' && next == '\'')) {
                    Sci(SCI_DELETERANGE, pos - 1, 2);
                    return 0;
                }
            }
        }
    }
    if (msg == WM_CHAR && autoCloseBraces) {
        wchar_t ch = (wchar_t)wParam;
        if (ch == '(' || ch == '{' || ch == '[' || ch == '"' || ch == '\'') {
            int sSel = Sci(SCI_GETSELECTIONSTART);
            int eSel = Sci(SCI_GETSELECTIONEND);
            if (sSel != eSel) {
                char openCh = (char)ch;
                char closeCh = (openCh == '(') ? ')' :
                               (openCh == '{') ? '}' :
                               (openCh == '[') ? ']' : openCh;
                int len = eSel - sSel;
                std::vector<char> buf(len + 1, 0);
                Sci(SCI_GETSELTEXT, 0, (LPARAM)buf.data());
                std::string wrapped = openCh + std::string(buf.data(), len) + closeCh;
                Sci(SCI_REPLACESEL, 0, (LPARAM)wrapped.c_str());
                Sci(SCI_SETSEL, sSel + 1, eSel + 1);
                return 0;
            }
        }
    }
    if (msg == WM_LBUTTONDOWN) {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        RECT rc; GetClientRect(hwnd, &rc);
        if (pt.x >= rc.right - CUSTOM_SB_SIZE && IsWindowVisible(hwndVScroll)) {
            RECT vThumb; GetWindowRect(hwndVScroll, &vThumb);
            POINT scrPt = pt; ClientToScreen(hwnd, &scrPt);
            int lineH = Sci(SCI_TEXTHEIGHT), page = lineH > 0 ? rc.bottom / lineH : 10;
            if (scrPt.y < vThumb.top) Sci(SCI_LINESCROLL, 0, -page);
            else if (scrPt.y > vThumb.bottom) Sci(SCI_LINESCROLL, 0, page);
            return 0; 
        }
        if (pt.y >= rc.bottom - CUSTOM_SB_SIZE && IsWindowVisible(hwndHScroll)) {
            int marginW = GetTotalMarginWidth();
            if (pt.x < marginW) return CallWindowProcW(oldSciProc, hwnd, msg, wParam, lParam);

            RECT hThumb; GetWindowRect(hwndHScroll, &hThumb);
            POINT scrPt = pt; ClientToScreen(hwnd, &scrPt);
            int delta = (rc.right - marginW) / 2;
            if (scrPt.x < hThumb.left) Sci(SCI_SETXOFFSET, max(0, (int)Sci(SCI_GETXOFFSET) - delta));
            else if (scrPt.x > hThumb.right) Sci(SCI_SETXOFFSET, Sci(SCI_GETXOFFSET) + delta);
            return 0; 
        }
    }
    return CallWindowProcW(oldSciProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SETFOCUS: {
            if (searchVisible && hwndSearchEdit && IsWindowVisible(hwndSearchEdit)) {
                SetFocus(hwndSearchEdit);
            } else if (hwndScintilla) {
                SetFocus(hwndScintilla);
            }
            break;
        }
        case WM_CREATE: {
            hwndMain = hwnd; ApplyDarkMode(hwnd);
            hUIFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Inter Medium");
            hIconFont = CreateFontW(11, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe MDL2 Assets");
            hSmallFont = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Inter Light");
            HMODULE hSci = LoadLibraryW(L"SciLexer.dll"); if (!hSci) hSci = LoadLibraryW(L"Scintilla.dll");
            if (!hSci) { ShowCustomMessageBox(hwnd, L"Failed to load Scintilla library", L"Error", MB_OK); return -1; }
            LoadLibraryW(L"lexilla.dll");

            hwndScintilla = CreateWindowExW(0, L"Scintilla", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            if (hwndScintilla) {
                oldSciProc = (WNDPROC)SetWindowLongPtrW(hwndScintilla, GWLP_WNDPROC, (LONG_PTR)SciSubProc);
                LoadSession(hwnd);
                StyleScintilla(hwndScintilla);
                Sci(SCI_SETTABWIDTH, editorTabWidth);
            }
            hwndSearchEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            if (hwndSearchEdit) { SendMessageW(hwndSearchEdit, WM_SETFONT, (WPARAM)hUIFont, TRUE); SendMessageW(hwndSearchEdit, 0x1501, TRUE, (LPARAM)L"Search..."); oldSearchEditProc = (WNDPROC)SetWindowLongPtrW(hwndSearchEdit, GWLP_WNDPROC, (LONG_PTR)SearchEditProc); }
            hwndReplaceEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            if (hwndReplaceEdit) { SendMessageW(hwndReplaceEdit, WM_SETFONT, (WPARAM)hUIFont, TRUE); SendMessageW(hwndReplaceEdit, 0x1501, TRUE, (LPARAM)L"Replace with..."); oldReplaceEditProc = (WNDPROC)SetWindowLongPtrW(hwndReplaceEdit, GWLP_WNDPROC, (LONG_PTR)ReplaceEditProc); }

            hwndVScroll = CreateWindowExW(0, L"DarkScrollbar", L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            hwndHScroll = CreateWindowExW(0, L"DarkScrollbar", L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            
            SetTimer(hwnd, 2, 100, NULL);
            break;
        }
        case WM_TIMER: {
            if (wParam == 1 || wParam == 2) {
                POINT pt; GetCursorPos(&pt); ScreenToClient(hwndScintilla, &pt);
                RECT rc; GetClientRect(hwndScintilla, &rc);
                bool nearV = (pt.x >= rc.right - 40 && pt.x < rc.right && pt.y >= 0 && pt.y < rc.bottom);
                bool nearH = (pt.x >= 0 && pt.x < rc.right && pt.y >= rc.bottom - 40 && pt.y < rc.bottom);
                
                if (wParam == 1 && !vScrollDrag && !vScrollHover && !hScrollDrag && !hScrollHover && !nearV && !nearH) {
                    scrollbarsVisible = false;
                    ShowWindow(hwndVScroll, SW_HIDE);
                    ShowWindow(hwndHScroll, SW_HIDE);
                    KillTimer(hwnd, 1);
                } else if (wParam == 2 && (nearV || nearH) && !scrollbarsVisible) {
                    ShowScrollbars(hwnd);
                }
            }
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            if (code == EN_CHANGE && (HWND)lParam == hwndSearchEdit) {
                UpdateSearchMatches();
            }
            if (id == IDM_FILE_NEW) CreateNewTab(hwnd);
            else if (id == IDM_FILE_OPEN) DoFileOpen(hwnd);
            else if (id == IDM_FILE_SAVE) DoFileSave(hwnd);
            else if (id == IDM_FILE_SAVE_AS) DoFileSaveAs(hwnd);
            else if (id == IDM_FILE_CLOSE_TAB) CloseTab(hwnd, activeTabIndex);
            else if (id == IDM_FILE_EXIT) PostMessage(hwnd, WM_CLOSE, 0, 0);
            else if (id == IDM_EDIT_UNDO) Sci(SCI_UNDO);
            else if (id == IDM_EDIT_REDO) Sci(SCI_REDO);
            else if (id == IDM_TOGGLE_WRAP) Sci(SCI_SETWRAPMODE, Sci(SCI_GETWRAPMODE) != SC_WRAP_NONE ? SC_WRAP_NONE : SC_WRAP_WORD);
            else if (id == IDM_TOGGLE_LINES) {
                if (Sci(SCI_GETMARGINWIDTHN, 0) > 0) {
                    Sci(SCI_SETMARGINWIDTHN, 0, 0);
                } else {
                    Sci(SCI_SETMARGINWIDTHN, 0, 40);
                    UpdateLineNumberWidth();
                }
                ShowScrollbars(hwnd);
            }
            else if (id == IDM_SETTINGS_DIALOG) {
                ShowSettingsDialog(hwnd);
            }
            break;
        }
        case WM_CTLCOLOREDIT: {
            if ((HWND)lParam == hwndSearchEdit || (HWND)lParam == hwndReplaceEdit) {
                SetTextColor((HDC)wParam, 0xD4D4D4); SetBkColor((HDC)wParam, 0x2B2521);
                static HBRUSH hbrBg = CreateSolidBrush(0x2B2521); return (INT_PTR)hbrBg;
            }
            break;
        }
        case WM_ERASEBKGND: return 1;
        case WM_NCCALCSIZE: return 0;
        case WM_NCHITTEST: {
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) }; ScreenToClient(hwnd, &pt);
            RECT rc; GetClientRect(hwnd, &rc);
            if (!IsZoomed(hwnd)) {
                int bs = 6; bool l = pt.x < bs, r = pt.x > rc.right - bs, t = pt.y < bs, b = pt.y > rc.bottom - bs;
                if (t && l) return HTTOPLEFT; if (t && r) return HTTOPRIGHT; if (b && l) return HTBOTTOMLEFT; if (b && r) return HTBOTTOMRIGHT;
                if (l) return HTLEFT; if (r) return HTRIGHT; if (t) return HTTOP; if (b) return HTBOTTOM;
            }
            RECT pad = GetPad(hwnd);
            return (pt.y >= pad.top && pt.y < pad.top + 35 && HitTest(hwnd, pt) == HOVER_NONE) ? HTCAPTION : HTCLIENT;
        }
        case WM_MOUSEMOVE: {
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
            HoverElement newHover = HitTest(hwnd, pt);
            if (newHover != hoverElement) { hoverElement = newHover; UpdateUI(hwnd); TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&tme); }
            break;
        }
        case WM_MOUSELEAVE: { hoverElement = HOVER_NONE; UpdateUI(hwnd); break; }
        case WM_LBUTTONDOWN: {
            POINT pt = { (int)(short)LOWORD(lParam), (short)HIWORD(lParam) }; pressedElement = HitTest(hwnd, pt);
            UpdateUI(hwnd); SetCapture(hwnd); break;
        }
        case WM_LBUTTONUP: {
            if (GetCapture() == hwnd) ReleaseCapture();
            POINT pt = { (int)(short)LOWORD(lParam), (short)HIWORD(lParam) }; HoverElement clicked = HitTest(hwnd, pt);
            if (clicked == pressedElement && pressedElement != HOVER_NONE) OnElementClicked(hwnd, pressedElement);
            pressedElement = HOVER_NONE;
            POINT pt2; GetCursorPos(&pt2); ScreenToClient(hwnd, &pt2);
            hoverElement = HitTest(hwnd, pt2);
            UpdateUI(hwnd); break;
        }
        case WM_MBUTTONUP: {
            POINT pt = { (int)(short)LOWORD(lParam), (short)HIWORD(lParam) };
            HoverElement clicked = HitTest(hwnd, pt);
            if (clicked >= HOVER_TAB_BASE && clicked < HOVER_TAB_CLOSE_BASE) {
                CloseTab(hwnd, clicked - HOVER_TAB_BASE);
            } else if (clicked >= HOVER_TAB_CLOSE_BASE && clicked < HOVER_SETTINGS) {
                CloseTab(hwnd, clicked - HOVER_TAB_CLOSE_BASE);
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc); RECT pad = GetPad(hwnd);
            HDC memDC = CreateCompatibleDC(hdc); HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
            PaintTopBar(hwnd, memDC, rc); PaintHeaderBar(hwnd, memDC, rc);
            int offset = 0;
            if (searchVisible) {
                PaintSearchBar(hwnd, memDC, rc);
                offset = replaceVisible ? 72 : 36;
            }
            RECT rcTopGap = { pad.left, pad.top + 70 + offset, rc.right - pad.right, pad.top + 70 + offset + EDITOR_TOP_MARGIN };
            FillRectColor(memDC, rcTopGap, 0x2B2521);
            POINT oldOrg; SetWindowOrgEx(memDC, 0, -(rc.bottom - pad.bottom - 24), &oldOrg);
            PaintStatusBar(hwnd, memDC, rc); SetWindowOrgEx(memDC, oldOrg.x, oldOrg.y, NULL);
            if (pad.left > 1) FillRectColor(memDC, { 0, pad.top + 70 + offset, pad.left, rc.bottom - pad.bottom - 24 }, 0x2B2521);
            if (pad.right > 1) FillRectColor(memDC, { rc.right - pad.right, pad.top + 70 + offset, rc.right, rc.bottom - pad.bottom - 24 }, 0x2B2521);
            if (pad.bottom > 1) FillRectColor(memDC, { 0, rc.bottom - pad.bottom, rc.right, rc.bottom }, 0x1F1A18);
            
            FillRectColor(memDC, { 0, 0, rc.right, 1 }, 0x3C312C);
            if (!IsZoomed(hwnd)) {
                FillRectColor(memDC, { 0, rc.bottom - 1, rc.right, rc.bottom }, 0x3C312C); 
                FillRectColor(memDC, { 0, 0, 1, rc.bottom }, 0x3C312C);              
                FillRectColor(memDC, { rc.right - 1, 0, rc.right, rc.bottom }, 0x3C312C); 
            }
            
            BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            SelectObject(memDC, oldBmp); DeleteObject(memBmp); DeleteDC(memDC); EndPaint(hwnd, &ps); return 0;
        }
        case WM_SIZE: {
            if (hwndScintilla) {
                RECT rc; GetClientRect(hwnd, &rc); RECT pad = GetPad(hwnd);
                int offset = 0;
                if (searchVisible) {
                    offset = replaceVisible ? 72 : 36;
                }
                int topH = pad.top + 70 + offset + EDITOR_TOP_MARGIN;
                int ew = rc.right - pad.left - pad.right;
                int eh = rc.bottom - topH - 24 - pad.bottom;
                SetWindowPos(hwndScintilla, NULL, pad.left, topH, ew, eh, SWP_NOZORDER);

                if (searchVisible) {
                    int searchY = pad.top + 70 + 10;
                    SetWindowPos(hwndSearchEdit, NULL, pad.left + 9, searchY, 330, 17, SWP_NOZORDER | SWP_SHOWWINDOW);
                    if (replaceVisible) {
                        int replaceY = pad.top + 70 + 36 + 10;
                        SetWindowPos(hwndReplaceEdit, NULL, pad.left + 9, replaceY, 330, 17, SWP_NOZORDER | SWP_SHOWWINDOW);
                    } else {
                        ShowWindow(hwndReplaceEdit, SW_HIDE);
                    }
                } else {
                    ShowWindow(hwndSearchEdit, SW_HIDE);
                    ShowWindow(hwndReplaceEdit, SW_HIDE);
                }
                SyncScrollbars();
                RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            }
            break;
        }
        case WM_NOTIFY: {
            if (isSavingSession) return 0;
            if (lParam && ((SCNotification*)lParam)->nmhdr.hwndFrom == hwndScintilla) {
                SCNotification* n = (SCNotification*)lParam;
                if (n->nmhdr.code == SCN_MODIFIED) {
                    if (n->modificationType & (0x01 | 0x02)) { // SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT
                        RecalculateScrollWidth();
                        if (searchVisible) UpdateSearchMatches();
                    }
                    if (activeTabIndex < tabs.size() && tabs[activeTabIndex].isModified != (Sci(SCI_GETMODIFY) != 0)) {
                        tabs[activeTabIndex].isModified = (Sci(SCI_GETMODIFY) != 0); UpdateUI(hwnd);
                    }
                    if (n->linesAdded != 0) {
                        int modLine = Sci(SCI_LINEFROMPOSITION, n->position), total = Sci(SCI_GETLINECOUNT);
                        for (int i = max(0, modLine); i < total; ++i) {
                            char buf[16]; sprintf_s(buf, "%d ", i + 1); 
                            Sci(SCI_MARGINSETTEXT, i, (LPARAM)buf);
                            Sci(SCI_MARGINSETSTYLE, i, (i >= activeLineStart && i <= activeLineEnd) ? 40 : STYLE_LINENUMBER);
                        }
                    }
                } else if (n->nmhdr.code == SCN_CHARADDED) {
                    char ch = (char)n->ch;
                    if (autoCloseBraces) {
                        int pos = Sci(SCI_GETCURRENTPOS);
                        char nextChar = (char)Sci(SCI_GETCHARAT, pos);
                        if ((ch == ')' || ch == '}' || ch == ']' || ch == '"' || ch == '\'') && ch == nextChar) {
                            Sci(SCI_DELETERANGE, pos, 1);
                        } else {
                            if (ch == '(') Sci(SCI_INSERTTEXT, pos, (LPARAM)")");
                            else if (ch == '{') Sci(SCI_INSERTTEXT, pos, (LPARAM)"}");
                            else if (ch == '[') Sci(SCI_INSERTTEXT, pos, (LPARAM)"]");
                            else if (ch == '"') Sci(SCI_INSERTTEXT, pos, (LPARAM)"\"");
                            else if (ch == '\'') Sci(SCI_INSERTTEXT, pos, (LPARAM)"'");
                        }
                    }
                    
                    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || 
                        ch == ',' || ch == '.' || ch == ';' || ch == ':' || 
                        ch == '!' || ch == '?' || ch == '(' || ch == ')' || 
                        ch == '[' || ch == ']' || ch == '{' || ch == '}' || 
                        ch == '"' || ch == '\'' || ch == '=' || ch == '+' || 
                        ch == '-' || ch == '*' || ch == '/' || ch == '<' || 
                        ch == '>' || ch == '&' || ch == '|' || ch == '^' || 
                        ch == '%') {
                        Sci(SCI_BEGINUNDOACTION);
                        Sci(SCI_ENDUNDOACTION);
                    }
                } else if (n->nmhdr.code == SCN_UPDATEUI) {
                    if (searchVisible) UpdateCurrentMatchIndex();
                    UpdateUI(hwnd); SyncLineNumbers(); ShowScrollbars(hwnd);
                }
            }
            break;
        }
        case WM_CLOSE: {
            SaveSession();
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY: {
            if (hUIFont) DeleteObject(hUIFont);
            if (hIconFont) DeleteObject(hIconFont);
            if (hSmallFont) DeleteObject(hSmallFont);
            PostQuitMessage(0); break;
        }
        default: return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nCmd) {
    LoadFonts();
    WNDCLASSW wc = { CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInst, LoadIconW(hInst, MAKEINTRESOURCEW(1)), LoadCursorW(NULL, (LPCWSTR)IDC_ARROW), NULL, NULL, L"VeloClass" };
    RegisterClassW(&wc);
    WNDCLASSW wcSb = { 0, ScrollbarProc, 0, 0, hInst, NULL, LoadCursorW(NULL, (LPCWSTR)IDC_ARROW), NULL, NULL, L"DarkScrollbar" };
    RegisterClassW(&wcSb);
    int width = 1400;
    int height = 800;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    HWND hwnd = CreateWindowExW(0, L"VeloClass", L"Untitled - Velo", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, width, height, NULL, NULL, hInst, NULL);
    if (!hwnd) {
        UnloadFonts();
        return 0;
    }
    ShowWindow(hwnd, nCmd);
    if (hwndScintilla) {
        SetFocus(hwndScintilla);
    }
    MSG msg = { };
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000)) {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            switch (msg.wParam) {
                case 'N': CreateNewTab(hwnd); continue;
                case 'O': DoFileOpen(hwnd); continue;
                case 'S': DoFileSave(hwnd); continue;
                case 'W': CloseTab(hwnd, activeTabIndex); continue;
                case 'Z': Sci(SCI_UNDO); continue;
                case 'Y': Sci(SCI_REDO); continue;
                case 'F': TriggerSearchDialog(hwnd); continue;
                case VK_TAB: SwitchToTab(hwnd, shift ? (activeTabIndex > 0 ? activeTabIndex - 1 : 0) : (activeTabIndex + 1 < tabs.size() ? activeTabIndex + 1 : activeTabIndex)); continue;
            }
        }
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    UnloadFonts();
    return 0;
}
