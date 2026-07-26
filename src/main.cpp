#include "globals.h"
#include "components/fonts.h"
#include "components/dialogs.h"
#include "components/editor.h"
#include "components/tabmanager.h"
#include "components/ui_drawing.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WM_USER_DEFERRED_INIT (WM_USER + 100)

// Define Global Variables
HWND hwndMain = NULL;
HWND hwndScintilla = NULL;
HWND hwndSearchEdit = NULL;
HWND hwndReplaceEdit = NULL;
HWND hwndVScroll = NULL;
HWND hwndHScroll = NULL;
HFONT hUIFont = NULL;
HFONT hIconFont = NULL;
HFONT hWindowIconFont = NULL;
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
int dragGrabOffset = 0;
WNDPROC oldSearchEditProc = NULL;
WNDPROC oldReplaceEditProc = NULL;
WNDPROC oldSciProc = NULL;
WNDPROC oldTabRenameEditProc = NULL;
HWND hwndTabRenameEdit = NULL;
int tabRenameIndex = -1;

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
bool showTopBar = true;
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

LRESULT CALLBACK TabRenameEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            if (tabRenameIndex >= 0 && tabRenameIndex < tabs.size()) {
                int len = GetWindowTextLengthW(hwnd);
                if (len > 0) {
                    std::vector<wchar_t> buf(len + 1);
                    GetWindowTextW(hwnd, buf.data(), len + 1);
                    std::wstring newName(buf.data());
                    
                    std::wstring oldPath = tabs[tabRenameIndex].filePath;
                    if (!oldPath.empty()) {
                        std::wstring parentDir = oldPath.substr(0, oldPath.find_last_of(L"\\/") + 1);
                        std::wstring newPath = parentDir + newName;
                        if (MoveFileW(oldPath.c_str(), newPath.c_str())) {
                            tabs[tabRenameIndex].filePath = newPath;
                            tabs[tabRenameIndex].title = newName;
                        } else {
                            ShowCustomMessageBox(hwndMain, L"Failed to rename file on disk.", L"Error", MB_OK);
                        }
                    } else {
                        tabs[tabRenameIndex].title = newName;
                    }
                    if (tabRenameIndex == activeTabIndex) StyleScintilla(hwndScintilla);
                    UpdateUI(hwndMain);
                }
            }
            ShowWindow(hwnd, SW_HIDE);
            SetFocus(hwndScintilla);
            return 0;
        } else if (wParam == VK_ESCAPE) {
            ShowWindow(hwnd, SW_HIDE);
            SetFocus(hwndScintilla);
            return 0;
        } else if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
    } else if (msg == WM_CHAR && (wParam == VK_RETURN || wParam == 1)) {
        return 0; // Prevent system ding
    } else if (msg == WM_KILLFOCUS) {
        ShowWindow(hwnd, SW_HIDE);
    }
    return CallWindowProcW(oldTabRenameEditProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ScrollbarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    bool isVert = (hwnd == hwndVScroll);
    bool &hover = isVert ? vScrollHover : hScrollHover, &drag = isVert ? vScrollDrag : hScrollDrag;
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);
            FillRectColor(hdc, rc, (drag || hover) ? theme.accent : theme.border);
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
                    Sci(SCI_DELETERANGE, pos, 1);
                }
            }
        }
    }
    if (msg == WM_CHAR) {
        if (wParam < 32 && wParam != VK_RETURN && wParam != VK_TAB && wParam != VK_BACK) return 0;
        
        if (autoCloseBraces) {
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
        case WM_COPYDATA: {
            COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lParam;
            if (cds && cds->dwData == 0x56454C4F && cds->lpData && cds->cbData > 0) {
                std::wstring filePath((const wchar_t*)cds->lpData, cds->cbData / sizeof(wchar_t));
                // Remove any trailing null characters
                while (!filePath.empty() && filePath.back() == L'\0') filePath.pop_back();
                if (!filePath.empty()) {
                    wchar_t fullPath[MAX_PATH];
                    if (GetFullPathNameW(filePath.c_str(), MAX_PATH, fullPath, NULL) != 0) {
                        filePath = fullPath;
                    }
                    // Check if already open
                    int existingIndex = -1;
                    for (size_t t = 0; t < tabs.size(); ++t) {
                        if (!_wcsicmp(tabs[t].filePath.c_str(), filePath.c_str())) {
                            existingIndex = (int)t;
                            break;
                        }
                    }
                    if (existingIndex >= 0) {
                        SwitchToTab(hwnd, existingIndex);
                    } else {
                        if (tabs.size() == 1 && tabs[0].filePath.empty() && Sci(SCI_GETLENGTH) == 0 && !tabs[0].isModified) {
                            LoadFileInActiveTab(hwnd, filePath.c_str());
                        } else {
                            CreateNewTab(hwnd, filePath);
                            LoadFileInActiveTab(hwnd, filePath.c_str());
                        }
                    }
                }
            }
            return TRUE;
        }
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
            hUIFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Inter Medium");
            hIconFont = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe MDL2 Assets");
            hWindowIconFont = CreateFontW(10, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe MDL2 Assets");
            hSmallFont = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Inter Medium");
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
            
            hwndTabRenameEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            if (hwndTabRenameEdit) { SendMessageW(hwndTabRenameEdit, WM_SETFONT, (WPARAM)hUIFont, TRUE); oldTabRenameEditProc = (WNDPROC)SetWindowLongPtrW(hwndTabRenameEdit, GWLP_WNDPROC, (LONG_PTR)TabRenameEditProc); }
            
            hwndVScroll = CreateWindowExW(0, L"DarkScrollbar", L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            hwndHScroll = CreateWindowExW(0, L"DarkScrollbar", L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            
            SetTimer(hwnd, 2, 100, NULL);
            break;
        }
        case WM_TIMER: {
            if (wParam == 3) {
                if (CheckThemeUpdate() > 0) {
                    if (hwndScintilla) ApplySyntax();
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                
                static bool checkingFileChanges = false;
                if (!checkingFileChanges) {
                    checkingFileChanges = true;
                    std::wstring themePath = GetThemePath();
                    for (size_t i = 0; i < tabs.size(); ++i) {
                        if (tabs[i].filePath.empty()) continue;
                        FILETIME diskTime = GetFileLastWriteTime(tabs[i].filePath);
                        if (diskTime.dwLowDateTime == 0 && diskTime.dwHighDateTime == 0) continue; // File deleted or inaccessible
                        
                        if (CompareFileTime(&diskTime, &tabs[i].lastWriteTime) > 0) {
                            // Update the timestamp BEFORE showing the dialog to prevent reentrancy
                            tabs[i].lastWriteTime = diskTime;
                            bool isTheme = !_wcsicmp(tabs[i].filePath.c_str(), themePath.c_str());
                            if (isTheme || ShowCustomMessageBox(hwnd, L"This file has been modified outside the editor. Do you want to reload it?", L"File Modified", MB_YESNO) == IDYES) {
                                if (i == activeTabIndex && hwndScintilla) {
                                    int pos = Sci(SCI_GETCURRENTPOS);
                                    LoadFileInActiveTab(hwnd, tabs[i].filePath.c_str());
                                    Sci(SCI_GOTOPOS, pos);
                                } else {
                                    // Load into inactive tab
                                    size_t oldActive = activeTabIndex;
                                    SwitchToTab(hwnd, i);
                                    LoadFileInActiveTab(hwnd, tabs[i].filePath.c_str());
                                    SwitchToTab(hwnd, oldActive);
                                }
                            }
                        }
                    }
                    checkingFileChanges = false;
                }
            } else if (wParam == 1 || wParam == 2) {
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
            else if (id == IDM_FILE_SAVE) {
                DoFileSave(hwnd);
            }
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
            else if (id == IDM_TOGGLE_TOPBAR) {
                showTopBar = !showTopBar;
                SaveSession();
                RECT rc; GetClientRect(hwnd, &rc);
                SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
                UpdateUI(hwnd);
            }
            break;
        }
        case WM_CTLCOLOREDIT: {
            if ((HWND)lParam == hwndSearchEdit || (HWND)lParam == hwndReplaceEdit || (HWND)lParam == hwndTabRenameEdit) {
                SetTextColor((HDC)wParam, theme.textNormal); SetBkColor((HDC)wParam, theme.bg);
                static HBRUSH hbrBg = NULL;
                if (hbrBg) DeleteObject(hbrBg);
                hbrBg = CreateSolidBrush(theme.bg); return (INT_PTR)hbrBg;
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
            if (pt.y >= pad.top && pt.y < pad.top + 35 && HitTest(hwnd, pt) == HOVER_NONE) return HTCAPTION;
            if (pt.y >= rc.bottom - pad.bottom - 24 && pt.y < rc.bottom && HitTest(hwnd, pt) == HOVER_NONE) return HTCAPTION;
            return HTCLIENT;
        }
        case WM_MOUSEMOVE: {
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
            HoverElement newHover = HitTest(hwnd, pt);
            
            if (GetCapture() == hwnd && pressedElement >= HOVER_TAB_BASE && pressedElement < HOVER_TAB_CLOSE_BASE) {
                int dragIdx = pressedElement - HOVER_TAB_BASE;
                int dragW = GetTabWidth(dragIdx);
                int dragVisualLeft = pt.x - dragGrabOffset;
                int dragVisualRight = dragVisualLeft + dragW;
                
                // Compute tab positions
                RECT rc2; GetClientRect(hwnd, &rc2); RECT pad2 = GetPad(hwnd);
                int sx = pad2.left + 70, cx = sx;
                // Find the logical left edge of the dragged tab's current slot
                for (int t = 0; t < dragIdx; ++t) cx += GetTabWidth(t);
                
                // Check swap with right neighbor: trigger when dragged tab's right edge passes neighbor's center
                if (dragIdx + 1 < (int)tabs.size()) {
                    int rightLeft = cx + dragW;
                    int rightW = GetTabWidth(dragIdx + 1);
                    int rightMid = rightLeft + rightW / 2;
                    if (dragVisualRight > rightMid) {
                        std::swap(tabs[dragIdx], tabs[dragIdx + 1]);
                        if (activeTabIndex == (size_t)dragIdx) activeTabIndex = dragIdx + 1;
                        else if (activeTabIndex == (size_t)(dragIdx + 1)) activeTabIndex = dragIdx;
                        pressedElement = (HoverElement)(HOVER_TAB_BASE + dragIdx + 1);
                    }
                }
                // Check swap with left neighbor: trigger when dragged tab's left edge passes neighbor's center
                if (dragIdx > 0) {
                    int leftW = GetTabWidth(dragIdx - 1);
                    int leftLeft = cx - leftW;
                    int leftMid = leftLeft + leftW / 2;
                    if (dragVisualLeft < leftMid) {
                        std::swap(tabs[dragIdx], tabs[dragIdx - 1]);
                        if (activeTabIndex == (size_t)dragIdx) activeTabIndex = dragIdx - 1;
                        else if (activeTabIndex == (size_t)(dragIdx - 1)) activeTabIndex = dragIdx;
                        pressedElement = (HoverElement)(HOVER_TAB_BASE + dragIdx - 1);
                    }
                }
                UpdateUI(hwnd);
            }
            
            if (newHover != hoverElement) { hoverElement = newHover; UpdateUI(hwnd); TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&tme); }
            break;
        }
        case WM_MOUSELEAVE: { hoverElement = HOVER_NONE; UpdateUI(hwnd); break; }
        case WM_LBUTTONDOWN: {
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) }; pressedElement = HitTest(hwnd, pt);
            if (pressedElement >= HOVER_TAB_BASE && pressedElement < HOVER_TAB_CLOSE_BASE) {
                RECT rc2; GetClientRect(hwnd, &rc2); RECT pad2 = GetPad(hwnd);
                int sx = pad2.left + 70, cx = sx;
                int tabIdx = pressedElement - HOVER_TAB_BASE;
                for (int t = 0; t < tabIdx; ++t) cx += GetTabWidth(t);
                dragGrabOffset = pt.x - cx;
            }
            UpdateUI(hwnd); SetCapture(hwnd); break;
        }
        case WM_LBUTTONDBLCLK: {
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
            HoverElement clicked = HitTest(hwnd, pt);
            if (clicked >= HOVER_TAB_BASE && clicked < HOVER_TAB_CLOSE_BASE) {
                TriggerTabRename(hwnd, clicked - HOVER_TAB_BASE);
            }
            break;
        }
        case WM_LBUTTONUP: {
            if (GetCapture() == hwnd) ReleaseCapture();
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) }; HoverElement clicked = HitTest(hwnd, pt);
            if (clicked == pressedElement && pressedElement != HOVER_NONE) OnElementClicked(hwnd, pressedElement);
            pressedElement = HOVER_NONE;
            POINT pt2; GetCursorPos(&pt2); ScreenToClient(hwnd, &pt2);
            hoverElement = HitTest(hwnd, pt2);
            UpdateUI(hwnd); break;
        }
        case WM_MBUTTONUP: {
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
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
            PaintTopBar(hwnd, memDC, rc);
            if (showTopBar) PaintHeaderBar(hwnd, memDC, rc);
            int topBarH = showTopBar ? 70 : 36;
            int offset = 0;
            if (searchVisible) {
                PaintSearchBar(hwnd, memDC, rc);
                bool inlineReplace = (rc.right - pad.right - pad.left > 1230);
                offset = replaceVisible ? (inlineReplace ? 36 : 72) : 36;
            }
            RECT rcTopGap = { pad.left, pad.top + topBarH + offset, rc.right - pad.right, pad.top + topBarH + offset + EDITOR_TOP_MARGIN };
            FillRectColor(memDC, rcTopGap, theme.editorBg);
            POINT oldOrg; SetWindowOrgEx(memDC, 0, -(rc.bottom - pad.bottom - 24), &oldOrg);
            PaintStatusBar(hwnd, memDC, rc); SetWindowOrgEx(memDC, oldOrg.x, oldOrg.y, NULL);
            if (pad.left > 1) FillRectColor(memDC, { 0, pad.top + topBarH + offset, pad.left, rc.bottom - pad.bottom - 24 }, theme.editorBg);
            if (pad.right > 1) FillRectColor(memDC, { rc.right - pad.right, pad.top + topBarH + offset, rc.right, rc.bottom - pad.bottom - 24 }, theme.editorBg);
            if (pad.bottom > 1) FillRectColor(memDC, { 0, rc.bottom - pad.bottom, rc.right, rc.bottom }, theme.tabBg);
            
            FillRectColor(memDC, { 0, 0, rc.right, 1 }, theme.border);
            if (!IsZoomed(hwnd)) {
                FillRectColor(memDC, { 0, rc.bottom - 1, rc.right, rc.bottom }, theme.border); 
                FillRectColor(memDC, { 0, 0, 1, rc.bottom }, theme.border);              
                FillRectColor(memDC, { rc.right - 1, 0, rc.right, rc.bottom }, theme.border); 
            }
            
            BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            SelectObject(memDC, oldBmp); DeleteObject(memBmp); DeleteDC(memDC); EndPaint(hwnd, &ps); return 0;
        }
        case WM_SIZE: {
            if (hwndScintilla) {
                RECT rc; GetClientRect(hwnd, &rc); RECT pad = GetPad(hwnd);
                bool inlineReplace = (rc.right - pad.right - pad.left > 1230);
                int offset = 0;
                if (searchVisible) {
                    offset = replaceVisible ? (inlineReplace ? 36 : 72) : 36;
                }
                int topBarH = showTopBar ? 70 : 36;
                int topH = pad.top + topBarH + offset + EDITOR_TOP_MARGIN;
                int ew = rc.right - pad.left - pad.right;
                int eh = rc.bottom - topH - 24 - pad.bottom;
                SetWindowPos(hwndScintilla, NULL, pad.left, topH, ew, eh, SWP_NOZORDER);

                if (searchVisible) {
                    int searchY = pad.top + topBarH + 10;
                    SetWindowPos(hwndSearchEdit, NULL, pad.left + 9, searchY, 330, 17, SWP_NOZORDER | SWP_SHOWWINDOW);
                    if (replaceVisible) {
                        int replaceY = pad.top + topBarH + (inlineReplace ? 0 : 36) + 10;
                        int replaceX = pad.left + 9 + (inlineReplace ? 412 : 0);
                        SetWindowPos(hwndReplaceEdit, NULL, replaceX, replaceY, 330, 17, SWP_NOZORDER | SWP_SHOWWINDOW);
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
                        UpdateCustomIndicators(hwndScintilla);
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
                } else if (n->nmhdr.code == SCN_HOTSPOTCLICK || n->nmhdr.code == SCN_INDICATORCLICK) {
                    int pos = n->position;
                    int style = Sci(SCI_GETSTYLEAT, pos);
                    bool isUrl = false;
                    int startPos = pos;
                    int endPos = pos;
                    int docLen = Sci(SCI_GETLENGTH);
                    
                    if (Sci(SCI_INDICATORALLONFOR, pos) & (1 << INDICATOR_URL)) {
                        isUrl = true;
                        while (startPos > 0 && (Sci(SCI_INDICATORALLONFOR, startPos - 1) & (1 << INDICATOR_URL))) startPos--;
                        while (endPos < docLen - 1 && (Sci(SCI_INDICATORALLONFOR, endPos + 1) & (1 << INDICATOR_URL))) endPos++;
                    } else if (style == 18) { // SCE_MARKDOWN_LINK
                        isUrl = true;
                        while (startPos > 0 && Sci(SCI_GETSTYLEAT, startPos - 1) == 18) startPos--;
                        while (endPos < docLen - 1 && Sci(SCI_GETSTYLEAT, endPos + 1) == 18) endPos++;
                    }
                    
                    if (isUrl) {
                        std::string linkText;
                        for (int i = startPos; i <= endPos; i++) {
                            linkText += (char)Sci(SCI_GETCHARAT, i);
                        }
                        
                        size_t parenStart = linkText.rfind('(');
                        size_t parenEnd = linkText.rfind(')');
                        if (parenStart != std::string::npos && parenEnd != std::string::npos && parenStart < parenEnd) {
                            std::string url = linkText.substr(parenStart + 1, parenEnd - parenStart - 1);
                            ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        } else {
                            // Raw URL
                            ShellExecuteA(NULL, "open", linkText.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        }
                    }
                } else if (n->nmhdr.code == SCN_UPDATEUI) {
                    bool isScroll = (n->updated & SC_UPDATE_V_SCROLL) || (n->updated & SC_UPDATE_H_SCROLL);
                    bool isContent = (n->updated & SC_UPDATE_CONTENT);
                    bool isSelection = (n->updated & SC_UPDATE_SELECTION);
                    
                    if (isScroll) {
                        UpdateCustomIndicators(hwndScintilla);
                        ShowScrollbars(hwnd);
                    }
                    if (isContent || isSelection) {
                        UpdateBraceMatch();
                        SyncLineNumbers();
                    }
                    if (isContent || isSelection || isScroll) {
                        if (searchVisible) UpdateCurrentMatchIndex();
                        UpdateUI(hwnd);
                    }
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
            if (hWindowIconFont) DeleteObject(hWindowIconFont);
            if (hSmallFont) DeleteObject(hSmallFont);
            PostQuitMessage(0); break;
        }
        default: return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void CleanupUserChoiceAssociations() {
    const wchar_t* extensions[] = {
        L".txt", L".log", L".md", L".markdown", L".json", L".jsonc", L".xml", L".html", L".htm", L".css", L".scss",
        L".ini", L".cfg", L".conf", L".config", L".env", L".yaml", L".yml", L".toml", L".c", L".cpp", L".cc", L".h",
        L".hpp", L".cs", L".java", L".js", L".jsx", L".ts", L".tsx", L".py", L".rb", L".go", L".rs", L".sql", L".ahk",
        L".rc", L".bat", L".cmd", L".ps1", L".sh", L".iss"
    };

    // Get the directory of the running executable so we can point DefaultIcon to our icons
    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of(L'\\');
    if (lastSlash != std::wstring::npos) exeDir = exeDir.substr(0, lastSlash + 1);


    // Set DefaultIcon on Applications\Velo.exe so that ANY file type associated
    // via Windows "Open with → Velo" shows _page.ico instead of the Velo app icon.
    // Only write if the value is missing or incorrect to avoid triggering an icon refresh.
    {
        std::wstring appProgIdKey = L"Software\\Classes\\Applications\\Velo.exe";
        std::wstring pageIcon = exeDir + L"icon\\papirus\\_page.ico";
        bool needsWrite = true;

        // Check if DefaultIcon already has the correct value
        HKEY hAppRead;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, (appProgIdKey + L"\\DefaultIcon").c_str(), 0, KEY_READ, &hAppRead) == ERROR_SUCCESS) {
            wchar_t curVal[MAX_PATH] = {0};
            DWORD curSize = sizeof(curVal);
            DWORD curType = 0;
            if (RegQueryValueExW(hAppRead, NULL, NULL, &curType, (LPBYTE)curVal, &curSize) == ERROR_SUCCESS) {
                if (_wcsicmp(curVal, pageIcon.c_str()) == 0) {
                    needsWrite = false;
                }
            }
            RegCloseKey(hAppRead);
        }

        if (needsWrite) {
            HKEY hAppKey;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, appProgIdKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
                                KEY_WRITE, NULL, &hAppKey, NULL) == ERROR_SUCCESS) {
                HKEY hDefIcon;
                if (RegCreateKeyExW(hAppKey, L"DefaultIcon", 0, NULL, REG_OPTION_NON_VOLATILE,
                                    KEY_WRITE, NULL, &hDefIcon, NULL) == ERROR_SUCCESS) {
                    RegSetValueExW(hDefIcon, NULL, 0, REG_SZ, (const BYTE*)pageIcon.c_str(),
                                  (DWORD)((pageIcon.length() + 1) * sizeof(wchar_t)));
                    RegCloseKey(hDefIcon);
                }
                RegCloseKey(hAppKey);
            }
        }
    }
    for (const wchar_t* ext : extensions) {
        // Determine the correct icon for this extension
        std::wstring iconFile;
        if (wcscmp(ext, L".txt") == 0 || wcscmp(ext, L".log") == 0)
            iconFile = L"icon\\papirus\\txt.ico";
        else if (wcscmp(ext, L".md") == 0 || wcscmp(ext, L".markdown") == 0)
            iconFile = L"icon\\papirus\\md.ico";
        else if (wcscmp(ext, L".json") == 0 || wcscmp(ext, L".jsonc") == 0)
            iconFile = L"icon\\papirus\\json.ico";
        else if (wcscmp(ext, L".ini") == 0 || wcscmp(ext, L".conf") == 0 || wcscmp(ext, L".env") == 0)
            iconFile = L"icon\\papirus\\ini.ico";
        else if (wcscmp(ext, L".cfg") == 0 || wcscmp(ext, L".config") == 0)
            iconFile = L"icon\\papirus\\cfg.ico";
        else if (wcscmp(ext, L".toml") == 0)
            iconFile = L"icon\\papirus\\toml.ico";
        else if (wcscmp(ext, L".yaml") == 0)
            iconFile = L"icon\\papirus\\yaml.ico";
        else if (wcscmp(ext, L".yml") == 0)
            iconFile = L"icon\\papirus\\yml.ico";
        else if (wcscmp(ext, L".go") == 0)
            iconFile = L"icon\\papirus\\go.ico";
        else if (wcscmp(ext, L".rs") == 0)
            iconFile = L"icon\\papirus\\rs.ico";
        else if (wcscmp(ext, L".py") == 0)
            iconFile = L"icon\\papirus\\py.ico";
        else if (wcscmp(ext, L".rb") == 0)
            iconFile = L"icon\\papirus\\rb.ico";
        else if (wcscmp(ext, L".java") == 0)
            iconFile = L"icon\\papirus\\java.ico";
        else if (wcscmp(ext, L".c") == 0)
            iconFile = L"icon\\papirus\\c.ico";
        else if (wcscmp(ext, L".cpp") == 0 || wcscmp(ext, L".cc") == 0 || wcscmp(ext, L".cs") == 0 ||
                 wcscmp(ext, L".ahk") == 0 || wcscmp(ext, L".rc") == 0)
            iconFile = L"icon\\papirus\\cpp.ico";
        else if (wcscmp(ext, L".hpp") == 0)
            iconFile = L"icon\\papirus\\hpp.ico";
        else if (wcscmp(ext, L".h") == 0)
            iconFile = L"icon\\papirus\\h.ico";
        else if (wcscmp(ext, L".html") == 0 || wcscmp(ext, L".htm") == 0)
            iconFile = L"icon\\papirus\\html.ico";
        else if (wcscmp(ext, L".js") == 0 || wcscmp(ext, L".jsx") == 0 || wcscmp(ext, L".ts") == 0 || wcscmp(ext, L".tsx") == 0)
            iconFile = L"icon\\papirus\\js.ico";
        else if (wcscmp(ext, L".css") == 0)
            iconFile = L"icon\\papirus\\css.ico";
        else if (wcscmp(ext, L".scss") == 0)
            iconFile = L"icon\\papirus\\scss.ico";
        else if (wcscmp(ext, L".sql") == 0)
            iconFile = L"icon\\papirus\\sql.ico";
        else if (wcscmp(ext, L".xml") == 0)
            iconFile = L"icon\\papirus\\xml.ico";
        else if (wcscmp(ext, L".iss") == 0)
            iconFile = L"icon\\papirus\\_page.ico";
        else if (wcscmp(ext, L".bat") == 0)
            iconFile = L"icon\\papirus\\bat.ico";
        else if (wcscmp(ext, L".ps1") == 0)
            iconFile = L"icon\\papirus\\ps1.ico";
        else
            iconFile = L"icon\\papirus\\_page.ico";

        std::wstring iconPath = exeDir + iconFile;
        std::wstring progId = std::wstring(L"Velo") + ext;

        // Ensure the per-extension ProgId exists with a DefaultIcon pointing to the correct .ico
        // This way, even if the user manually associated the file via "Open with", the icon shows correctly
        std::wstring progIdKey = std::wstring(L"Software\\Classes\\") + progId;
        HKEY hProgId;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, progIdKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
                            KEY_WRITE, NULL, &hProgId, NULL) == ERROR_SUCCESS) {
            std::wstring extName = ext + 1; // skip the dot
            for (auto& ch : extName) ch = towupper(ch);
            std::wstring displayName = extName + L" Document";
            RegSetValueExW(hProgId, NULL, 0, REG_SZ, (const BYTE*)displayName.c_str(),
                          (DWORD)((displayName.length() + 1) * sizeof(wchar_t)));
            HKEY hIcon;
            if (RegCreateKeyExW(hProgId, L"DefaultIcon", 0, NULL, REG_OPTION_NON_VOLATILE,
                                KEY_WRITE, NULL, &hIcon, NULL) == ERROR_SUCCESS) {
                RegSetValueExW(hIcon, NULL, 0, REG_SZ, (const BYTE*)iconPath.c_str(),
                              (DWORD)((iconPath.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hIcon);
            }
            HKEY hCmd;
            if (RegCreateKeyExW(hProgId, L"shell\\open\\command", 0, NULL, REG_OPTION_NON_VOLATILE,
                                KEY_WRITE, NULL, &hCmd, NULL) == ERROR_SUCCESS) {
                std::wstring cmd = L"\"" + exeDir.substr(0, exeDir.size() - 1) + L"\" \"%1\"";
                RegSetValueExW(hCmd, NULL, 0, REG_SZ, (const BYTE*)cmd.c_str(),
                              (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hCmd);
            }
            RegCloseKey(hProgId);
        }

        std::wstring subKey = std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\") + ext + L"\\UserChoice";
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey.c_str(), 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            wchar_t value[256] = {0};
            DWORD valueSize = sizeof(value);
            DWORD type = 0;
            if (RegQueryValueExW(hKey, L"ProgId", NULL, &type, (LPBYTE)value, &valueSize) == ERROR_SUCCESS) {
                if (wcscmp(value, L"Velo.Document") == 0) {
                    RegCloseKey(hKey);
                    hKey = NULL;
                    
                    // 1. Create/Open HKCU\Software\Classes\<ext> and set default value to Velo<ext>
                    std::wstring classesKey = std::wstring(L"Software\\Classes\\") + ext;
                    HKEY hClasses;
                    if (RegCreateKeyExW(HKEY_CURRENT_USER, classesKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hClasses, NULL) == ERROR_SUCCESS) {
                        RegSetValueExW(hClasses, NULL, 0, REG_SZ, (const BYTE*)progId.c_str(), (DWORD)((progId.length() + 1) * sizeof(wchar_t)));
                        RegCloseKey(hClasses);
                    }
                    
                    // 2. Delete UserChoice key
                    std::wstring parentKey = std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\") + ext;
                    HKEY hParent;
                    if (RegOpenKeyExW(HKEY_CURRENT_USER, parentKey.c_str(), 0, KEY_WRITE, &hParent) == ERROR_SUCCESS) {
                        RegDeleteKeyW(hParent, L"UserChoice");
                        RegCloseKey(hParent);
                    }
                }
            }
            if (hKey) {
                RegCloseKey(hKey);
            }
        }
    }

    // Scan ALL FileExts for any UserChoice that references a Velo* ProgId
    // and ensure that ProgId has a DefaultIcon set. This covers extensions
    // that the user associated with Velo via "Open with" that aren't in our known list.
    {
        std::wstring pageIconPath = exeDir + L"icon\\papirus\\_page.ico";
        HKEY hFileExts;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts", 0, KEY_READ, &hFileExts) == ERROR_SUCCESS) {
            DWORD subCount = 0;
            RegQueryInfoKeyW(hFileExts, NULL, NULL, NULL, &subCount, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

            for (DWORD i = 0; i < subCount; ++i) {
                wchar_t extBuf[256] = {0};
                DWORD extLen = 256;
                if (RegEnumKeyExW(hFileExts, i, extBuf, &extLen, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
                    continue;

                std::wstring ext(extBuf);
                if (ext.length() == 0 || ext[0] != L'.') continue;

                std::wstring ucKey = ext + L"\\UserChoice";
                HKEY hUC;
                if (RegOpenKeyExW(hFileExts, ucKey.c_str(), 0, KEY_READ, &hUC) != ERROR_SUCCESS)
                    continue;

                wchar_t progIdBuf[256] = {0};
                DWORD progIdSize = sizeof(progIdBuf);
                DWORD type = 0;
                HRESULT hr = RegQueryValueExW(hUC, L"ProgId", NULL, &type, (LPBYTE)progIdBuf, &progIdSize);
                RegCloseKey(hUC);
                if (hr != ERROR_SUCCESS) continue;

                bool isVeloProgId = (wcsncmp(progIdBuf, L"Velo", 4) == 0);
                bool isAppProgId = (wcscmp(progIdBuf, L"Applications\\Velo.exe") == 0);
                if (!isVeloProgId && !isAppProgId) continue;

                // For Applications\Velo.exe, create a proper per-extension ProgId
                // so we can set extension-specific icons and display names.
                // Only create if it doesn't already exist with the correct icon.
                if (isAppProgId) {
                    std::wstring extStr(extBuf);
                    std::wstring perExtProgId = L"Velo" + extStr;
                    std::wstring perExtProgIdKey = std::wstring(L"Software\\Classes\\") + perExtProgId;

                    std::wstring pageIconPath2 = exeDir + L"icon\\papirus\\_page.ico";

                    // Check if per-extension ProgId already exists with correct DefaultIcon
                    bool alreadyCorrect = false;
                    HKEY hCheckProgId;
                    if (RegOpenKeyExW(HKEY_CURRENT_USER, (perExtProgIdKey + L"\\DefaultIcon").c_str(), 0, KEY_READ, &hCheckProgId) == ERROR_SUCCESS) {
                        wchar_t curIcon[MAX_PATH] = {0};
                        DWORD curSize = sizeof(curIcon);
                        DWORD curType = 0;
                        if (RegQueryValueExW(hCheckProgId, NULL, NULL, &curType, (LPBYTE)curIcon, &curSize) == ERROR_SUCCESS) {
                            if (_wcsicmp(curIcon, pageIconPath2.c_str()) == 0) {
                                alreadyCorrect = true;
                            }
                        }
                        RegCloseKey(hCheckProgId);
                    }

                    if (!alreadyCorrect) {
                        // Build display name from extension
                        std::wstring rawExt = (extStr.length() > 1) ? extStr.substr(1) : extStr;
                        for (auto& ch : rawExt) ch = towupper(ch);
                        std::wstring displayName = rawExt + L" Document";

                        HKEY hPerExt;
                        if (RegCreateKeyExW(HKEY_CURRENT_USER, perExtProgIdKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
                                            KEY_WRITE, NULL, &hPerExt, NULL) == ERROR_SUCCESS) {
                            RegSetValueExW(hPerExt, NULL, 0, REG_SZ, (const BYTE*)displayName.c_str(),
                                          (DWORD)((displayName.length() + 1) * sizeof(wchar_t)));
                            HKEY hIcon;
                            if (RegCreateKeyExW(hPerExt, L"DefaultIcon", 0, NULL, REG_OPTION_NON_VOLATILE,
                                                KEY_WRITE, NULL, &hIcon, NULL) == ERROR_SUCCESS) {
                                RegSetValueExW(hIcon, NULL, 0, REG_SZ, (const BYTE*)pageIconPath2.c_str(),
                                              (DWORD)((pageIconPath2.length() + 1) * sizeof(wchar_t)));
                                RegCloseKey(hIcon);
                            }
                            HKEY hCmd;
                            if (RegCreateKeyExW(hPerExt, L"shell\\open\\command", 0, NULL, REG_OPTION_NON_VOLATILE,
                                                KEY_WRITE, NULL, &hCmd, NULL) == ERROR_SUCCESS) {
                                std::wstring cmd = L"\"" + exeDir.substr(0, exeDir.size() - 1) + L"\" \"%1\"";
                                RegSetValueExW(hCmd, NULL, 0, REG_SZ, (const BYTE*)cmd.c_str(),
                                              (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
                                RegCloseKey(hCmd);
                            }
                            RegCloseKey(hPerExt);
                        }

                        // Point HKCU\Software\Classes\.ext to our per-extension ProgId
                        std::wstring classesKey = std::wstring(L"Software\\Classes\\") + extStr;
                        HKEY hClasses;
                        if (RegCreateKeyExW(HKEY_CURRENT_USER, classesKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
                                            KEY_WRITE, NULL, &hClasses, NULL) == ERROR_SUCCESS) {
                            RegSetValueExW(hClasses, NULL, 0, REG_SZ, (const BYTE*)perExtProgId.c_str(),
                                          (DWORD)((perExtProgId.length() + 1) * sizeof(wchar_t)));
                            RegCloseKey(hClasses);
                        }
                    }
                    continue;
                }

                // This UserChoice references a Velo* ProgId — make sure it has a proper DefaultIcon
                // and a correctly-cased display name
                std::wstring progIdKey = std::wstring(L"Software\\Classes\\") + progIdBuf;
                HKEY hProgId;

                // Build the proper display name: strip "Velo" prefix and leading dot, then uppercase
                // e.g. "Velo.css" -> "css" -> "CSS" -> "CSS Document"
                std::wstring rawExt(progIdBuf + 4); // skip "Velo" prefix
                if (!rawExt.empty() && rawExt[0] == L'.') rawExt = rawExt.substr(1);
                for (auto& ch : rawExt) ch = towupper(ch);
                std::wstring displayName = rawExt + L" Document";

                if (RegOpenKeyExW(HKEY_CURRENT_USER, progIdKey.c_str(), 0, KEY_READ, &hProgId) == ERROR_SUCCESS) {
                    // Check if DefaultIcon exists and what it points to
                    HKEY hIcon = NULL;
                    bool hasIcon = (RegOpenKeyExW(hProgId, L"DefaultIcon", 0, KEY_READ, &hIcon) == ERROR_SUCCESS);
                    bool needsIconFix = false;
                    if (hasIcon) {
                        // Read the current icon value — if it points to Velo.exe, it's the app icon (wrong)
                        wchar_t iconVal[MAX_PATH] = {0};
                        DWORD iconValSize = sizeof(iconVal);
                        DWORD iconType = 0;
                        if (RegQueryValueExW(hIcon, NULL, NULL, &iconType, (LPBYTE)iconVal, &iconValSize) == ERROR_SUCCESS) {
                            std::wstring iconStr(iconVal);
                            // Check if the icon path contains "Velo.exe" (app icon) rather than a .ico file
                            if (iconStr.find(L"Velo.exe") != std::wstring::npos ||
                                iconStr.find(L"velo.exe") != std::wstring::npos) {
                                needsIconFix = true;
                            }
                        }
                        RegCloseKey(hIcon);
                    }
                    RegCloseKey(hProgId);

                    if (!hasIcon || needsIconFix) {
                        // ProgId exists but has no DefaultIcon or it points to the app exe — set it to _page.ico
                        if (RegCreateKeyExW(HKEY_CURRENT_USER, progIdKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
                                            KEY_WRITE, NULL, &hProgId, NULL) == ERROR_SUCCESS) {
                            // Fix display name
                            RegSetValueExW(hProgId, NULL, 0, REG_SZ, (const BYTE*)displayName.c_str(),
                                          (DWORD)((displayName.length() + 1) * sizeof(wchar_t)));
                            HKEY hIconW;
                            if (RegCreateKeyExW(hProgId, L"DefaultIcon", 0, NULL, REG_OPTION_NON_VOLATILE,
                                                KEY_WRITE, NULL, &hIconW, NULL) == ERROR_SUCCESS) {
                                RegSetValueExW(hIconW, NULL, 0, REG_SZ, (const BYTE*)pageIconPath.c_str(),
                                              (DWORD)((pageIconPath.length() + 1) * sizeof(wchar_t)));
                                RegCloseKey(hIconW);
                            }
                            RegCloseKey(hProgId);
                        }
                    }
                } else {
                    // ProgId doesn't exist in HKCU at all — create it with _page.ico
                    if (RegCreateKeyExW(HKEY_CURRENT_USER, progIdKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
                                        KEY_WRITE, NULL, &hProgId, NULL) == ERROR_SUCCESS) {
                        RegSetValueExW(hProgId, NULL, 0, REG_SZ, (const BYTE*)displayName.c_str(),
                                      (DWORD)((displayName.length() + 1) * sizeof(wchar_t)));
                        HKEY hIcon;
                        if (RegCreateKeyExW(hProgId, L"DefaultIcon", 0, NULL, REG_OPTION_NON_VOLATILE,
                                            KEY_WRITE, NULL, &hIcon, NULL) == ERROR_SUCCESS) {
                            RegSetValueExW(hIcon, NULL, 0, REG_SZ, (const BYTE*)pageIconPath.c_str(),
                                          (DWORD)((pageIconPath.length() + 1) * sizeof(wchar_t)));
                            RegCloseKey(hIcon);
                        }
                        HKEY hCmd;
                        if (RegCreateKeyExW(hProgId, L"shell\\open\\command", 0, NULL, REG_OPTION_NON_VOLATILE,
                                            KEY_WRITE, NULL, &hCmd, NULL) == ERROR_SUCCESS) {
                            std::wstring cmd = L"\"" + exeDir.substr(0, exeDir.size() - 1) + L"\" \"%1\"";
                            RegSetValueExW(hCmd, NULL, 0, REG_SZ, (const BYTE*)cmd.c_str(),
                                          (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
                            RegCloseKey(hCmd);
                        }
                        RegCloseKey(hProgId);
                    }
                }
            }
            RegCloseKey(hFileExts);
        }
    }

    // Note: We intentionally do NOT call SHChangeNotify(SHCNE_ASSOCCHANGED) here
    // because it causes visible Explorer flicker on every launch. The registry
    // writes above are sufficient — icons will update naturally on next Explorer
    // refresh, reboot, or when the installer runs (which has its own SHChangeNotify).
}

static std::wstring ToLowerW(const std::wstring& s) {
    std::wstring r = s;
    for (auto& c : r) c = towlower(c);
    return r;
}

static std::wstring ToUpperW(const std::wstring& s) {
    std::wstring r = s;
    for (auto& c : r) c = towupper(c);
    return r;
}

void RunCurrentFile(HWND hwnd) {
    if (activeTabIndex >= tabs.size()) return;

    Tab& tab = tabs[activeTabIndex];

    if (tab.filePath.empty()) {
        ShowCustomMessageBox(hwnd, L"This file must be saved to disk before it can be run.", L"Run File", MB_OK | MB_ICONWARNING);
        return;
    }

    if (tab.isModified) {
        DoFileSave(hwnd);
    }

    std::wstring dir = tab.filePath.substr(0, tab.filePath.find_last_of(L"\\/"));

    HINSTANCE result = ShellExecuteW(NULL, L"open", tab.filePath.c_str(), NULL, dir.c_str(), SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(result) <= 32) {
        ShowCustomMessageBox(hwnd, L"Failed to open this file. No application is associated with this file type.", L"Run Error", MB_OK | MB_ICONERROR);
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nCmd) {
    // Single-instance check: if Velo is already running, send file path(s) to it and exit
    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"VeloSingleInstanceMutex");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is running — find its window and send file paths via WM_COPYDATA
        HWND hwndExisting = FindWindowW(L"VeloClass", NULL);
        if (hwndExisting) {
            int argc = 0;
            LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            if (argv) {
                for (int i = 1; i < argc; ++i) {
                    std::wstring filePath = argv[i];
                    if (filePath.empty()) continue;
                    wchar_t fullPath[MAX_PATH];
                    if (GetFullPathNameW(filePath.c_str(), MAX_PATH, fullPath, NULL) != 0) {
                        filePath = fullPath;
                    }
                    COPYDATASTRUCT cds = {};
                    cds.dwData = 0x56454C4F; // 'VELO'
                    cds.cbData = (DWORD)((filePath.length() + 1) * sizeof(wchar_t));
                    cds.lpData = (void*)filePath.c_str();
                    SendMessageW(hwndExisting, WM_COPYDATA, 0, (LPARAM)&cds);
                }
                LocalFree(argv);
            }
            // Bring the existing window to the foreground
            if (IsIconic(hwndExisting)) ShowWindow(hwndExisting, SW_RESTORE);
            SetForegroundWindow(hwndExisting);
        }
        CloseHandle(hMutex);
        return 0;
    }

    LoadTheme();
    CleanupUserChoiceAssociations();
    SetProcessDPIAware();
    LoadFonts();
    WNDCLASSW wc = { CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInst, LoadIconW(hInst, MAKEINTRESOURCEW(1)), LoadCursorW(NULL, (LPCWSTR)IDC_ARROW), NULL, NULL, L"VeloClass" };
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
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }
    SetTimer(hwnd, 3, 1000, NULL);
    ShowWindow(hwnd, nCmd);
    
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        bool openedAny = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring filePath = argv[i];
            if (filePath.empty()) continue;
            
            wchar_t fullPath[MAX_PATH];
            if (GetFullPathNameW(filePath.c_str(), MAX_PATH, fullPath, NULL) != 0) {
                filePath = fullPath;
            }
            
            int existingIndex = -1;
            for (size_t t = 0; t < tabs.size(); ++t) {
                if (!_wcsicmp(tabs[t].filePath.c_str(), filePath.c_str())) {
                    existingIndex = (int)t;
                    break;
                }
            }
            
            if (existingIndex >= 0) {
                SwitchToTab(hwnd, existingIndex);
                openedAny = true;
            } else {
                if (!openedAny && tabs.size() == 1 && tabs[0].filePath.empty() && Sci(SCI_GETLENGTH) == 0 && !tabs[0].isModified) {
                    LoadFileInActiveTab(hwnd, filePath.c_str());
                } else {
                    CreateNewTab(hwnd, filePath);
                    LoadFileInActiveTab(hwnd, filePath.c_str());
                }
                openedAny = true;
            }
        }
        LocalFree(argv);
    }

    if (hwndScintilla) {
        SetFocus(hwndScintilla);
    }
    MSG msg = { };
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_F5 && (GetKeyState(VK_SHIFT) & 0x8000) && !(GetKeyState(VK_CONTROL) & 0x8000)) {
            RunCurrentFile(hwnd);
            continue;
        }
        if (msg.message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000)) {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            switch (msg.wParam) {
                case 'N': CreateNewTab(hwnd); continue;
                case 'O': DoFileOpen(hwnd); continue;
                case 'S': 
                    if (shift) DoFileSaveAs(hwnd);
                    else DoFileSave(hwnd);
                    continue;
                case 'W': 
                    if (shift) {
                        for (int i = (int)tabs.size() - 1; i >= 0; --i) {
                            size_t prevSize = tabs.size();
                            CloseTab(hwnd, i);
                            if (i > 0 && tabs.size() == prevSize) break;
                            if (i == 0 && Sci(SCI_GETMODIFY)) break;
                        }
                    } else {
                        CloseTab(hwnd, activeTabIndex);
                    }
                    continue;
                case 'Z': Sci(SCI_UNDO); continue;
                case 'Y': Sci(SCI_REDO); continue;
                case 'F': TriggerSearchDialog(hwnd); continue;
                case VK_TAB: SwitchToTab(hwnd, shift ? (activeTabIndex > 0 ? activeTabIndex - 1 : 0) : (activeTabIndex + 1 < tabs.size() ? activeTabIndex + 1 : activeTabIndex)); continue;
            }
        }
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    UnloadFonts();
    if (hMutex) CloseHandle(hMutex);
    return 0;
}
