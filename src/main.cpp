#include "globals.h"
#include "components/fonts.h"
#include "components/dialogs.h"
#include "components/editor.h"
#include "components/tabmanager.h"
#include "components/ui_drawing.h"
#include "components/animations.h"
#include <sstream>

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
POINT clickPos = {0, 0};
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
bool enableAnimations = false;
bool zenMode = false;
bool zenTopVisible = false;
bool zenBottomVisible = false;
float zenTopProgress = 0.0f;
float zenBottomProgress = 0.0f;

ULONGLONG zenTopAnimStart = 0;
float zenTopAnimStartProgress = 0.0f;
float zenTopAnimTargetProgress = 0.0f;

ULONGLONG zenBottomAnimStart = 0;
float zenBottomAnimStartProgress = 0.0f;
float zenBottomAnimTargetProgress = 0.0f;

bool isSavingSession = false;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COPYDATA: {
            COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lParam;
            if (cds && cds->dwData == VELO_COPYDATA_TAB_TRANSFER && cds->lpData && cds->cbData >= sizeof(VeloTabTransferHeader)) {
                VeloTabTransferHeader* hdr = (VeloTabTransferHeader*)cds->lpData;
                const char* textPtr = (const char*)cds->lpData + sizeof(VeloTabTransferHeader);
                
                CreateNewTab(hwnd, hdr->filePath, false);
                size_t newIdx = tabs.size() - 1;
                if (hdr->title[0] != L'\0') tabs[newIdx].title = hdr->title;
                if (hdr->filePath[0] != L'\0') tabs[newIdx].filePath = hdr->filePath;
                tabs[newIdx].eolMode = hdr->eolMode;
                
                if (hdr->textSize > 0) {
                    Sci(SCI_CLEARALL);
                    Sci(SCI_APPENDTEXT, hdr->textSize, (LPARAM)textPtr);
                    if (!hdr->isModified) {
                        Sci(SCI_SETSAVEPOINT);
                        Sci(SCI_EMPTYUNDOBUFFER);
                    }
                    tabs[newIdx].isModified = hdr->isModified;
                }
                
                ApplySyntax();
                UpdateUI(hwnd);
                if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
                return TRUE;
            }
            if (cds && cds->dwData == VELO_COPYDATA_FILE_OPEN && cds->lpData && cds->cbData > 0) {
                std::wstring filePath((const wchar_t*)cds->lpData, cds->cbData / sizeof(wchar_t));
                while (!filePath.empty() && filePath.back() == L'\0') filePath.pop_back();
                if (!filePath.empty()) {
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
                if (!checkingFileChanges && !IsIconic(hwnd)) {
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
            } else if (wParam == 4) {
                UpdateZenAnimations(hwnd);
            } else if (wParam == 5) {
                UpdateTabAnimations(hwnd);
            }
            if (zenMode) {
                POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
                RECT rc; GetClientRect(hwnd, &rc);
                if (!zenTopVisible && pt.y <= 5) TriggerZenTopAnimation(hwnd, true);
                else if (zenTopVisible && pt.y > 75) TriggerZenTopAnimation(hwnd, false);
                
                if (!zenBottomVisible && pt.y >= rc.bottom - 5) TriggerZenBottomAnimation(hwnd, true);
                else if (zenBottomVisible && pt.y < rc.bottom - 45) TriggerZenBottomAnimation(hwnd, false);
            }
            break;
        }
        case WM_CANCELMODE:
        case WM_KILLFOCUS: {
            if (GetCapture() == hwnd) {
                ReleaseCapture();
                SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));
                pressedElement = HOVER_NONE;
                hoverElement = HOVER_NONE;
                UpdateUI(hwnd);
            }
            break;
        }
        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE && GetCapture() == hwnd) {
                ReleaseCapture();
                SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));
                pressedElement = HOVER_NONE;
                hoverElement = HOVER_NONE;
                UpdateUI(hwnd);
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
        case WM_NCCALCSIZE: {
            if (wParam && IsIconic(hwnd)) {
                return DefWindowProcW(hwnd, msg, wParam, lParam);
            }
            return 0;
        }
        case WM_NCHITTEST: {
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) }; ScreenToClient(hwnd, &pt);
            RECT rc; GetClientRect(hwnd, &rc);
            if (!IsZoomed(hwnd) && !zenMode) {
                int bs = 6; bool l = pt.x < bs, r = pt.x > rc.right - bs, t = pt.y < bs, b = pt.y > rc.bottom - bs;
                if (t && l) return HTTOPLEFT; if (t && r) return HTTOPRIGHT; if (b && l) return HTBOTTOMLEFT; if (b && r) return HTBOTTOMRIGHT;
                if (l) return HTLEFT; if (r) return HTRIGHT; if (t) return HTTOP; if (b) return HTBOTTOM;
            }
            RECT pad = GetPad(hwnd);
            int topDragH = showTopBar ? 70 : 35;
            if (pt.y >= pad.top && pt.y < pad.top + topDragH && HitTest(hwnd, pt) == HOVER_NONE) return HTCAPTION;
            if (pt.y >= rc.bottom - pad.bottom - 24 && pt.y < rc.bottom && HitTest(hwnd, pt) == HOVER_NONE) return HTCAPTION;
            return HTCLIENT;
        }
        case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT) {
                if (GetCapture() == hwnd && pressedElement >= HOVER_TAB_BASE && pressedElement < HOVER_TAB_CLOSE_BASE) {
                    POINT ptScreen; GetCursorPos(&ptScreen);
                    POINT ptClient = ptScreen; ScreenToClient(hwnd, &ptClient);
                    RECT rcClient; GetClientRect(hwnd, &rcClient);
                    RECT pad = GetPad(hwnd);

                    bool isOutside = (ptClient.y < pad.top - 10 || ptClient.y > pad.top + 45 || ptClient.x < pad.left || ptClient.x > rcClient.right - pad.right);
                    if (isOutside) {
                        SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_SIZEALL));
                        return TRUE;
                    }
                } else if ((HWND)wParam == hwnd) {
                    SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));
                    return TRUE;
                }
            }
            break;
        }
        case WM_MOUSEMOVE: {
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
            HoverElement newHover = HitTest(hwnd, pt);
            
            if (zenMode) {
                RECT rc; GetClientRect(hwnd, &rc);
                if (!zenTopVisible && pt.y <= 5) TriggerZenTopAnimation(hwnd, true);
                else if (zenTopVisible && pt.y > 75) TriggerZenTopAnimation(hwnd, false);
                
                if (!zenBottomVisible && pt.y >= rc.bottom - 5) TriggerZenBottomAnimation(hwnd, true);
                else if (zenBottomVisible && pt.y < rc.bottom - 45) TriggerZenBottomAnimation(hwnd, false);
            }
            
            if (GetCapture() == hwnd && pressedElement >= HOVER_TAB_BASE && pressedElement < HOVER_TAB_CLOSE_BASE) {
                POINT ptScreen; GetCursorPos(&ptScreen);
                POINT ptClient = ptScreen; ScreenToClient(hwnd, &ptClient);
                RECT rcClient; GetClientRect(hwnd, &rcClient);
                RECT pad = GetPad(hwnd);

                bool isOutside = (ptClient.y < pad.top - 10 || ptClient.y > pad.top + 45 || ptClient.x < pad.left || ptClient.x > rcClient.right - pad.right);
                if (isOutside) {
                    SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_SIZEALL));
                } else {
                    SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));
                }

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
            
            if (GetCapture() == hwnd && (pressedElement == HOVER_FILE_NAME || pressedElement >= HOVER_PATH_PART_BASE)) {
                if (abs(pt.x - clickPos.x) > GetSystemMetrics(SM_CXDRAG) || abs(pt.y - clickPos.y) > GetSystemMetrics(SM_CYDRAG)) {
                    ReleaseCapture();
                    pressedElement = HOVER_NONE;
                    if (zenMode) ToggleZenMode(hwnd);
                    SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE | 0x0002, 0);
                    break;
                }
            }
            
            if (newHover != hoverElement) { hoverElement = newHover; UpdateUI(hwnd); TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&tme); }
            break;
        }
        case WM_SYSCOMMAND: {
            if ((wParam & 0xFFF0) == SC_MOVE && zenMode) {
                ToggleZenMode(hwnd);
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        case WM_MOUSELEAVE: { hoverElement = HOVER_NONE; UpdateUI(hwnd); break; }
        case WM_LBUTTONDOWN: {
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) }; pressedElement = HitTest(hwnd, pt);
            clickPos = pt;
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
            if (GetCapture() == hwnd) {
                int dragIdx = -1;
                if (pressedElement >= HOVER_TAB_BASE && pressedElement < HOVER_TAB_CLOSE_BASE) {
                    dragIdx = pressedElement - HOVER_TAB_BASE;
                }
                ReleaseCapture();
                SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));

                if (dragIdx >= 0 && dragIdx < (int)tabs.size()) {
                    POINT ptScreen; GetCursorPos(&ptScreen);
                    POINT ptClient = ptScreen; ScreenToClient(hwnd, &ptClient);
                    RECT rcClient; GetClientRect(hwnd, &rcClient);
                    RECT pad = GetPad(hwnd);

                    bool isOutside = (ptClient.y < pad.top - 10 || ptClient.y > pad.top + 45 || ptClient.x < pad.left || ptClient.x > rcClient.right - pad.right);
                    if (isOutside) {
                        pressedElement = HOVER_NONE;
                        hoverElement = HOVER_NONE;
                        SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));

                        HWND hwndTarget = WindowFromPoint(ptScreen);
                        HWND hwndTopTarget = GetAncestor(hwndTarget, GA_ROOT);

                        wchar_t cls[64] = {0};
                        if (hwndTopTarget) GetClassNameW(hwndTopTarget, cls, 64);

                        if (hwndTopTarget && hwndTopTarget != hwnd && !_wcsicmp(cls, L"VeloClass")) {
                            TransferTabToWindow(hwnd, hwndTopTarget, (size_t)dragIdx);
                        } else {
                            DetachTabToNewWindow(hwnd, (size_t)dragIdx);
                        }
                        UpdateUI(hwnd);
                        break;
                    }
                }
            }
            POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) }; HoverElement clicked = HitTest(hwnd, pt);
            if (clicked == pressedElement && pressedElement != HOVER_NONE) OnElementClicked(hwnd, pressedElement);
            pressedElement = HOVER_NONE;
            POINT pt2; GetCursorPos(&pt2); ScreenToClient(hwnd, &pt2);
            hoverElement = HitTest(hwnd, pt2);
            SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));
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
            
            static HDC s_memDC = NULL;
            static HBITMAP s_memBmp = NULL;
            static HBITMAP s_oldBmp = NULL;
            static int s_memW = 0, s_memH = 0;
            
            if (!s_memDC || s_memW != rc.right || s_memH != rc.bottom) {
                if (s_memDC) {
                    SelectObject(s_memDC, s_oldBmp);
                    DeleteObject(s_memBmp);
                    DeleteDC(s_memDC);
                }
                s_memDC = CreateCompatibleDC(hdc);
                s_memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
                s_oldBmp = (HBITMAP)SelectObject(s_memDC, s_memBmp);
                s_memW = rc.right;
                s_memH = rc.bottom;
            }
            HDC memDC = s_memDC;
            FillRectColor(memDC, rc, theme.editorBg);
            
            int fullTopBarH = showTopBar ? 70 : 36;
            int topBarH = fullTopBarH;
            if (zenMode) topBarH = (int)(fullTopBarH * zenTopProgress + 0.5f);

            if (!zenMode || zenTopProgress > 0.001f) {
                int topOffset = zenMode ? (int)((zenTopProgress - 1.0f) * fullTopBarH) : 0;
                POINT oldOrg;
                if (topOffset != 0) SetWindowOrgEx(memDC, 0, -topOffset, &oldOrg);
                PaintTopBar(hwnd, memDC, rc);
                if (showTopBar) PaintHeaderBar(hwnd, memDC, rc);
                if (topOffset != 0) SetWindowOrgEx(memDC, oldOrg.x, oldOrg.y, NULL);
            }
            int offset = 0;
            if (searchVisible && (!zenMode || zenTopProgress > 0.001f)) {
                PaintSearchBar(hwnd, memDC, rc);
                bool inlineReplace = (rc.right - pad.right - pad.left > 1230);
                offset = replaceVisible ? (inlineReplace ? 36 : 72) : 36;
            }
            int topMargin = (zenMode && zenTopProgress < 0.001f) ? 0 : EDITOR_TOP_MARGIN;
            RECT rcTopGap = { pad.left, pad.top + topBarH + offset, rc.right - pad.right, pad.top + topBarH + offset + topMargin };
            FillRectColor(memDC, rcTopGap, theme.editorBg);
            
            int fullBottomBarH = 24;
            int bottomBarH = fullBottomBarH;
            if (zenMode) bottomBarH = (int)(fullBottomBarH * zenBottomProgress + 0.5f);
            
            if (!zenMode || zenBottomProgress > 0.001f) {
                int bottomOffset = zenMode ? (int)((1.0f - zenBottomProgress) * 24.0f) : 0;
                POINT oldOrg;
                SetWindowOrgEx(memDC, 0, -(rc.bottom - pad.bottom - 24 + bottomOffset), &oldOrg);
                PaintStatusBar(hwnd, memDC, rc); SetWindowOrgEx(memDC, oldOrg.x, oldOrg.y, NULL);
            }
            
            if (pad.left > 1) FillRectColor(memDC, { 0, pad.top + topBarH + offset, pad.left, rc.bottom - pad.bottom - bottomBarH }, theme.editorBg);
            if (pad.right > 1) FillRectColor(memDC, { rc.right - pad.right, pad.top + topBarH + offset, rc.right, rc.bottom - pad.bottom - bottomBarH }, theme.editorBg);
            if (pad.bottom > 1) FillRectColor(memDC, { 0, rc.bottom - pad.bottom, rc.right, rc.bottom }, theme.tabBg);
            
            if (!zenMode) FillRectColor(memDC, { 0, 0, rc.right, 1 }, theme.border);
            if (!IsZoomed(hwnd) && !zenMode) {
                FillRectColor(memDC, { 0, rc.bottom - 1, rc.right, rc.bottom }, theme.border); 
                FillRectColor(memDC, { 0, 0, 1, rc.bottom }, theme.border);              
                FillRectColor(memDC, { rc.right - 1, 0, rc.right, rc.bottom }, theme.border); 
            }
            
            BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, memDC, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            EndPaint(hwnd, &ps); return 0;
        }
        case WM_SIZE: {
            if (hwndScintilla) {
                RECT rc; GetClientRect(hwnd, &rc); RECT pad = GetPad(hwnd);
                bool inlineReplace = (rc.right - pad.right - pad.left > 1230);
                int offset = 0;
                if (searchVisible) {
                    offset = replaceVisible ? (inlineReplace ? 36 : 72) : 36;
                }
                int fullTopBarH = showTopBar ? 70 : 36;
                int topBarH = fullTopBarH;
                if (zenMode) topBarH = (int)(fullTopBarH * zenTopProgress + 0.5f);
                int topH = pad.top + topBarH + offset + (zenMode && zenTopProgress < 0.001f ? 0 : EDITOR_TOP_MARGIN);
                int ew = rc.right - pad.left - pad.right;
                int fullBottomBarH = 24;
                int bottomBarH = fullBottomBarH;
                if (zenMode) bottomBarH = (int)(fullBottomBarH * zenBottomProgress + 0.5f);
                int eh = rc.bottom - topH - bottomBarH - pad.bottom;
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
                } else if (n->nmhdr.code == SCN_HOTSPOTRELEASECLICK || n->nmhdr.code == SCN_INDICATORRELEASE) {
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

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nCmd) {
    bool isDetachedLaunch = false;
    std::wstring detachedFilePath = L"";

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if ((!_wcsicmp(argv[i], L"--detached") || !_wcsicmp(argv[i], L"-detached")) && i + 1 < argc) {
                isDetachedLaunch = true;
                detachedFilePath = argv[i + 1];
                break;
            }
        }
    }

    HANDLE hMutex = NULL;
    if (!isDetachedLaunch) {
        hMutex = CreateMutexW(NULL, FALSE, L"VeloSingleInstanceMutex");
        if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
            HWND hwndExisting = FindWindowW(L"VeloClass", NULL);
            if (hwndExisting) {
                if (argv) {
                    for (int i = 1; i < argc; ++i) {
                        std::wstring filePath = argv[i];
                        if (filePath.empty()) continue;
                        wchar_t fullPath[MAX_PATH];
                        if (GetFullPathNameW(filePath.c_str(), MAX_PATH, fullPath, NULL) != 0) {
                            filePath = fullPath;
                        }
                        COPYDATASTRUCT cds = {};
                        cds.dwData = VELO_COPYDATA_FILE_OPEN;
                        cds.cbData = (DWORD)((filePath.length() + 1) * sizeof(wchar_t));
                        cds.lpData = (void*)filePath.c_str();
                        DWORD_PTR dwRes = 0;
                        SendMessageTimeoutW(hwndExisting, WM_COPYDATA, 0, (LPARAM)&cds, SMTO_ABORTIFHUNG | SMTO_NORMAL, 3000, &dwRes);
                    }
                }
                if (IsIconic(hwndExisting)) ShowWindow(hwndExisting, SW_RESTORE);
                SetForegroundWindow(hwndExisting);
            }
            if (argv) LocalFree(argv);
            if (hMutex) CloseHandle(hMutex);
            return 0;
        }
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
        if (argv) LocalFree(argv);
        return 0;
    }
    SetTimer(hwnd, 3, 1000, NULL);
    ShowWindow(hwnd, nCmd);
    
    if (isDetachedLaunch && !detachedFilePath.empty()) {
        std::ifstream in(detachedFilePath, std::ios::in | std::ios::binary);
        if (in.is_open()) {
            std::string u8Title, u8Path, u8Mod, u8Eol, sCurX, sCurY, headerMarker;
            std::getline(in, u8Title);
            std::getline(in, u8Path);
            std::getline(in, u8Mod);
            std::getline(in, u8Eol);
            std::getline(in, sCurX);
            std::getline(in, sCurY);
            std::getline(in, headerMarker);
            
            std::stringstream ss;
            ss << in.rdbuf();
            std::string body = ss.str();
            in.close();
            DeleteFileW(detachedFilePath.c_str());

            try {
                int cx = std::stoi(sCurX);
                int cy = std::stoi(sCurY);
                int spawnX = cx - 150;
                int spawnY = cy - 18;
                if (spawnX < 0) spawnX = 0;
                if (spawnY < 0) spawnY = 0;
                SetWindowPos(hwnd, NULL, spawnX, spawnY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            } catch (...) {}

            std::wstring wTitle = L"Untitled", wPath = L"";
            int lenT = MultiByteToWideChar(CP_UTF8, 0, u8Title.c_str(), -1, NULL, 0);
            if (lenT > 0) { std::vector<wchar_t> b(lenT); MultiByteToWideChar(CP_UTF8, 0, u8Title.c_str(), -1, b.data(), lenT); wTitle = b.data(); }
            int lenP = MultiByteToWideChar(CP_UTF8, 0, u8Path.c_str(), -1, NULL, 0);
            if (lenP > 0) { std::vector<wchar_t> b(lenP); MultiByteToWideChar(CP_UTF8, 0, u8Path.c_str(), -1, b.data(), lenP); wPath = b.data(); }

            if (!tabs.empty()) {
                tabs[0].title = wTitle;
                tabs[0].filePath = wPath;
                try { tabs[0].eolMode = std::stoi(u8Eol); } catch (...) {}
                Sci(SCI_CLEARALL);
                if (!body.empty()) {
                    Sci(SCI_APPENDTEXT, body.size(), (LPARAM)body.data());
                }
                if (u8Mod != "1") {
                    Sci(SCI_SETSAVEPOINT);
                    Sci(SCI_EMPTYUNDOBUFFER);
                }
                tabs[0].isModified = (u8Mod == "1");
                ApplySyntax();
                UpdateUI(hwnd);
            }
        }
    } else if (argv) {
        bool openedAny = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring filePath = argv[i];
            if (filePath.empty() || !_wcsicmp(filePath.c_str(), L"--detached") || !_wcsicmp(filePath.c_str(), L"-detached")) continue;
            if (i > 1 && (!_wcsicmp(argv[i-1], L"--detached") || !_wcsicmp(argv[i-1], L"-detached"))) continue;

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
    }
    if (argv) LocalFree(argv);

    if (hwndScintilla) {
        SetFocus(hwndScintilla);
    }
    MSG msg = { };
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_F11) {
            ToggleZenMode(hwnd);
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_F5) {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (ctrl && shift) {
                RunCurrentFile(hwnd, true);
                continue;
            } else if (shift || !ctrl) {
                RunCurrentFile(hwnd, false);
                continue;
            }
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
