#ifndef GLOBALS_H
#define GLOBALS_H

#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <fstream>
#include "include/Scintilla.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

// Standalone Scintilla Lexer IDs
#define SCLEX_NULL 0
#define SCLEX_PYTHON 2
#define SCLEX_CPP 3
#define SCLEX_HTML 4
#ifndef SCI_SETLEXER
#define SCI_SETLEXER 4001
#endif
#ifndef SCI_SETLEXERLANGUAGE
#define SCI_SETLEXERLANGUAGE 4006
#endif
#ifndef SCI_SETILEXER
#define SCI_SETILEXER 4033
#endif
#ifndef SCI_SETSCROLLWIDTHTRACKING
#define SCI_SETSCROLLWIDTHTRACKING 2516
#endif

#define IDM_FILE_NEW 1001
#define IDM_FILE_OPEN 1002
#define IDM_FILE_SAVE 1003
#define IDM_FILE_SAVE_AS 1005
#define IDM_FILE_CLOSE_TAB 1006
#define IDM_FILE_EXIT 1004
#define IDM_EDIT_UNDO 2001
#define IDM_EDIT_REDO 2002
#define IDM_TOGGLE_WRAP 3001
#define IDM_TOGGLE_LINES 3002
#define IDM_SETTINGS_DIALOG 3003
#define CUSTOM_SB_SIZE 10
#define EDITOR_TOP_MARGIN 6

struct Tab {
    std::wstring filePath, title;
    sptr_t docPointer;
    bool isModified;
    std::wstring backupFile = L"";
    bool isLoaded = false;
    bool hasUndoableBackupLoad = false;
};

enum HoverElement {
    HOVER_NONE, HOVER_MINIMIZE, HOVER_MAXIMIZE, HOVER_CLOSE, HOVER_UNDO, HOVER_REDO, HOVER_ADD_TAB,
    HOVER_TAB_BASE, HOVER_TAB_CLOSE_BASE = 100, HOVER_SETTINGS = 200, HOVER_SEARCH,
    HOVER_SEARCH_PREV, HOVER_SEARCH_NEXT, HOVER_SEARCH_SELECT_ALL, HOVER_SEARCH_REPLACE_TOGGLE,
    HOVER_SEARCH_CLOSE, HOVER_REPLACE_NEXT, HOVER_REPLACE_ALL,
    HOVER_STATUS_EOL
};

struct DlgButton {
    RECT rect;
    std::wstring label;
    int result;
};

struct CustomDialogData {
    std::wstring message;
    std::wstring title;
    UINT type;
    std::vector<DlgButton> buttons;
    int hoveredButtonIndex;
    int pressedButtonIndex;
    int result;
    bool running;
};

struct PopupMenuItem {
    std::wstring label;
    int id;
    bool isSeparator;
    bool isChecked;
    std::wstring shortcut;
};

// Global Variables
extern HWND hwndMain, hwndScintilla, hwndSearchEdit, hwndReplaceEdit;
extern HWND hwndVScroll, hwndHScroll;
extern HFONT hUIFont, hIconFont, hSmallFont;
extern bool searchVisible, replaceVisible, scrollbarsVisible, vScrollHover, vScrollDrag, hScrollHover, hScrollDrag;
extern int scrollDragStart, scrollDragStartPos, scrollDragMaxScroll, scrollDragMaxTravel;
extern int activeLineStart, activeLineEnd;
extern std::vector<Tab> tabs;
extern size_t activeTabIndex;
extern HoverElement hoverElement, pressedElement;
extern WNDPROC oldSearchEditProc, oldReplaceEditProc;
extern WNDPROC oldSciProc;

extern int currentMatchIndex;
extern int totalMatchesCount;
extern std::vector<std::pair<int, int>> searchMatches;

#ifndef SCI_SETMULTIPLESELECTION
#define SCI_SETMULTIPLESELECTION 2563
#endif
#ifndef SCI_SETADDITIONALSELECTIONTYPING
#define SCI_SETADDITIONALSELECTIONTYPING 2565
#endif
#ifndef SCI_ADDSELECTION
#define SCI_ADDSELECTION 2573
#endif

// Settings Globals
extern int editorFontSize;
extern int editorTabWidth;
extern bool autoSaveOnSwitch;
extern bool autoCloseBraces;
extern bool showIndentGuides;
extern bool showWhitespace;
extern bool caretStyleBlock;
extern bool isSavingSession;

// Utilities
void FillRectColor(HDC hdc, const RECT& rc, COLORREF color);
RECT GetPad(HWND h);

// Session Persistence
void SaveSession();
void LoadSession(HWND hwndParent);

#endif // GLOBALS_H
