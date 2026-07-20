#include "../globals.h"
#include "editor.h"
#include "ui_drawing.h"

int GetTotalMarginWidth() {
    if (!hwndScintilla) return 0;
    int w = 0;
    for (int i = 0; i < 5; ++i) w += Sci(SCI_GETMARGINWIDTHN, i);
    return w > 0 ? w + 4 : 0; 
}

void UpdateLineNumberWidth() {
    if (!hwndScintilla) return;
    if (Sci(SCI_GETMARGINWIDTHN, 0) == 0) return; // Hidden
    
    int lines = Sci(SCI_GETLINECOUNT);
    int digits = 1;
    while (lines >= 10) {
        lines /= 10;
        digits++;
    }
    
    int charW = Sci(SCI_TEXTWIDTH, STYLE_LINENUMBER, (LPARAM)"8");
    int spaceW = Sci(SCI_TEXTWIDTH, STYLE_LINENUMBER, (LPARAM)" ");
    
    int newW = (digits * charW) + (spaceW * 2) + 8;
    
    if ((int)Sci(SCI_GETMARGINWIDTHN, 0) != newW) {
        Sci(SCI_SETMARGINWIDTHN, 0, newW);
        SyncScrollbars();
    }
}

void RecalculateScrollWidth() {
    int maxTextWidth = 0;
    int lineCount = Sci(SCI_GETLINECOUNT);
    std::string lineBuf;
    std::string expandedBuf;
    
    int charW = Sci(SCI_TEXTWIDTH, STYLE_DEFAULT, (LPARAM)"A");
    if (charW <= 0) charW = 8;
    
    for (int i = 0; i < lineCount; ++i) {
        int len = Sci(SCI_LINELENGTH, i);
        if (len <= 0) continue;
        
        // Quick estimate: even if all bytes were tabs expanding to editorTabWidth,
        // if that width is less than maxTextWidth, we can skip it entirely.
        if (len * editorTabWidth * charW <= maxTextWidth) continue;
        
        lineBuf.resize(len);
        Sci(SCI_GETLINE, i, (LPARAM)lineBuf.data());
        
        // Strip trailing line endings (\r, \n)
        int actualLen = len;
        while (actualLen > 0 && (lineBuf[actualLen - 1] == '\r' || lineBuf[actualLen - 1] == '\n')) {
            actualLen--;
        }
        
        // First, let's calculate the columns (to see if this line is a candidate)
        int cols = 0;
        for (int j = 0; j < actualLen; ++j) {
            if (lineBuf[j] == '\t') {
                cols += editorTabWidth - (cols % editorTabWidth);
            } else {
                unsigned char c = (unsigned char)lineBuf[j];
                if (c < 0x80 || c >= 0xC0) {
                    cols++;
                }
            }
        }
        
        // Estimate the width using cols
        if (cols * charW <= maxTextWidth) continue;
        
        // Expand tabs to spaces to measure exact pixel width via SCI_TEXTWIDTH
        expandedBuf.clear();
        int col = 0;
        for (int j = 0; j < actualLen; ++j) {
            char c = lineBuf[j];
            if (c == '\t') {
                int spaces = editorTabWidth - (col % editorTabWidth);
                expandedBuf.append(spaces, ' ');
                col += spaces;
            } else {
                expandedBuf.push_back(c);
                unsigned char uc = (unsigned char)c;
                if (uc < 0x80 || uc >= 0xC0) {
                    col++;
                }
            }
        }
        
        int textW = Sci(SCI_TEXTWIDTH, STYLE_DEFAULT, (LPARAM)expandedBuf.c_str());
        if (textW > maxTextWidth) {
            maxTextWidth = textW;
        }
    }
    
    int scrollWidth = maxTextWidth + 100;
    if (scrollWidth < 1) scrollWidth = 1;
    Sci(SCI_SETSCROLLWIDTH, scrollWidth);
}

void FindNextText(const std::string& query, bool forward) {
    if (query.empty()) return;
    int cur = Sci(SCI_GETCURRENTPOS), len = Sci(SCI_GETLENGTH);
    Sci_TextToFind ft = { { forward ? cur : max(0, cur - (int)query.length()), forward ? len : 0 }, (char*)query.c_str() };
    int found = Sci(SCI_FINDTEXT, SCFIND_NONE, (LPARAM)&ft);
    if (found == -1) { ft.chrg = { forward ? 0 : len, cur }; found = Sci(SCI_FINDTEXT, SCFIND_NONE, (LPARAM)&ft); }
    if (found != -1) { Sci(SCI_SETSEL, ft.chrgText.cpMin, ft.chrgText.cpMax); Sci(SCI_VERTICALCENTRECARET); }
}

void ApplySyntax() {
    std::wstring ext = L"";
    if (activeTabIndex < tabs.size()) {
        std::wstring fp = tabs[activeTabIndex].filePath;
        size_t dot = fp.find_last_of(L'.');
        if (dot != std::wstring::npos) ext = fp.substr(dot + 1);
    }
    const char* lang = "null"; int lex = SCLEX_NULL;
    const char *kw0 = "", *kw1 = "", *kw3 = "";

    if (!_wcsicmp(ext.c_str(), L"cpp") || !_wcsicmp(ext.c_str(), L"c") || !_wcsicmp(ext.c_str(), L"h") || !_wcsicmp(ext.c_str(), L"hpp") || !_wcsicmp(ext.c_str(), L"js") || !_wcsicmp(ext.c_str(), L"ts") || !_wcsicmp(ext.c_str(), L"json") || !_wcsicmp(ext.c_str(), L"ahk")) {
        lang = "cpp"; lex = SCLEX_CPP;
        kw0 = "int float double char void bool long short signed unsigned auto const static extern inline virtual public private protected struct class enum union namespace template typename typedef false true null nullptr var let function extends export import from default if else for while do switch case break continue return goto try catch throw new delete and or not NULL TRUE FALSE WINAPI CALLBACK";
        kw1 = "WinMain WndProc ScrollbarProc SearchEditProc CreateWindowExW LoadCursorW RegisterClassW ShowWindow GetMessageW TranslateMessage DispatchMessageW PostMessage DefWindowProcW GetClientRect GetWindowRect FillRectColor SendMessage GetProcAddress LoadLibraryW LoadLibraryExW MessageBoxW CreateFileW ReadFile WriteFile GetFileSize CloseHandle wcscpy_s GetOpenFileNameW GetSaveFileNameW CreatePopupMenu AppendMenuW ClientToScreen TrackPopupMenu DestroyMenu PostQuitMessage CreateFontW GetDC SelectObject DrawTextW ReleaseDC SetTextColor SetBkMode SetBkColor ExtTextOutW AlphaBlend";
        kw3 = "std string wstring vector map set pair string_view cout cin endl HANDLE HWND HDC HFONT HBITMAP HMENU HINSTANCE LPSTR LPCWSTR WNDCLASSW WNDPROC MSG RECT POINT SIZE FILE COLORREF DWORD WORD BYTE INT_PTR LONG_PTR LRESULT UINT WPARAM LPARAM BOOL sptr_t Tab HoverElement console window document Math JSON Promise Array Object String Number Boolean";
    } else if (!_wcsicmp(ext.c_str(), L"py")) {
        lang = "python"; lex = SCLEX_PYTHON;
        kw0 = "False None True and as assert async await break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass raise return try while with yield";
        kw1 = "print len range str int float list dict set tuple bool";
    } else if (!_wcsicmp(ext.c_str(), L"html") || !_wcsicmp(ext.c_str(), L"htm") || !_wcsicmp(ext.c_str(), L"xml")) {
        lang = "hypertext"; lex = SCLEX_HTML;
        kw0 = "html head title body div span a img ul li table tr td th form input button script style link meta header footer nav section article main p h1 h2 h3 h4 h5 h6 br hr";
    }

    if (lex != SCLEX_NULL) {
        HMODULE hLex = GetModuleHandleW(L"lexilla.dll");
        if (!hLex) hLex = GetModuleHandleW(L"SciLexer.dll");
        if (hLex) {
            if (auto CreateLexer = (void* (__stdcall*)(const char*))GetProcAddress(hLex, "CreateLexer")) Sci(SCI_SETILEXER, 0, (LPARAM)CreateLexer(lang));
            else { Sci(SCI_SETLEXER, lex); Sci(SCI_SETLEXERLANGUAGE, 0, (LPARAM)lang); }
        } else { Sci(SCI_SETLEXER, lex); Sci(SCI_SETLEXERLANGUAGE, 0, (LPARAM)lang); }
    } else Sci(SCI_SETLEXER, SCLEX_NULL);

    Sci(SCI_SETPROPERTY, (LPARAM)"styling.within.preprocessor", (LPARAM)"1");
    Sci(SCI_SETKEYWORDS, 0, (LPARAM)kw0); Sci(SCI_SETKEYWORDS, 1, (LPARAM)kw1); Sci(SCI_SETKEYWORDS, 3, (LPARAM)kw3);

    for (int i = 0; i <= 32; ++i) {
        Sci(SCI_STYLESETBACK, i, 0x2B2521); Sci(SCI_STYLESETFORE, i, 0xD4D4D4);
        Sci(SCI_STYLESETFONT, i, (LPARAM)"JetBrains Mono Medium"); Sci(SCI_STYLESETSIZE, i, editorFontSize);
    }

    // Style the indentation guides using the unactive line/row number color (0x70635C)
    Sci(SCI_STYLESETBACK, STYLE_INDENTGUIDE, 0x2B2521);
    Sci(SCI_STYLESETFORE, STYLE_INDENTGUIDE, 0x70635C);

    // Apply document-specific settings so they persist across tab switches/new documents
    Sci(SCI_SETTABWIDTH, editorTabWidth);
    Sci(SCI_SETINDENTATIONGUIDES, showIndentGuides ? SC_IV_LOOKBOTH : SC_IV_NONE);
    Sci(SCI_SETVIEWWS, showWhitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE);


    if (lex == SCLEX_CPP) {
        Sci(SCI_STYLESETFORE, 1, 0x55996A); Sci(SCI_STYLESETFORE, 2, 0x55996A);
        Sci(SCI_STYLESETFORE, 4, 0xA8CEB5); Sci(SCI_STYLESETFORE, 5, 0xD69C56); Sci(SCI_STYLESETBOLD, 5, FALSE);
        Sci(SCI_STYLESETFORE, 6, 0x7891CE); Sci(SCI_STYLESETFORE, 7, 0x7891CE);
        Sci(SCI_STYLESETFORE, 9, 0xC086C5); Sci(SCI_STYLESETFORE, 10, 0xD4D4D4);
        Sci(SCI_STYLESETFORE, 11, 0xFEDC9C); Sci(SCI_STYLESETFORE, 16, 0xAADCDC);
        Sci(SCI_STYLESETFORE, 19, 0xB0C94E);
    } else if (lex == SCLEX_PYTHON) {
        Sci(SCI_STYLESETFORE, 1, 0x55996A); Sci(SCI_STYLESETFORE, 2, 0xA8CEB5);
        Sci(SCI_STYLESETFORE, 3, 0x7891CE); Sci(SCI_STYLESETFORE, 4, 0x7891CE);
        Sci(SCI_STYLESETFORE, 5, 0xC086C5); Sci(SCI_STYLESETFORE, 8, 0xB0C94E);
        Sci(SCI_STYLESETFORE, 9, 0xAADCDC); Sci(SCI_STYLESETFORE, 10, 0xD4D4D4);
        Sci(SCI_STYLESETFORE, 11, 0xFEDC9C); Sci(SCI_STYLESETFORE, 14, 0xD69C56);
    } else if (lex == SCLEX_HTML) {
        Sci(SCI_STYLESETFORE, 1, 0xD69C56); Sci(SCI_STYLESETFORE, 3, 0xFEDC9C);
        Sci(SCI_STYLESETFORE, 5, 0xA8CEB5); Sci(SCI_STYLESETFORE, 6, 0x7891CE);
        Sci(SCI_STYLESETFORE, 9, 0x55996A);
    }
    Sci(SCI_COLOURISE, 0, -1);
}

void StyleScintilla(HWND hwndSci) {
    SetWindowTheme(hwndSci, L"DarkMode_Explorer", NULL); ApplyDarkMode(hwndSci);
    Sci(SCI_SETMARGINTYPEN, 0, SC_MARGIN_RTEXT);
    Sci(SCI_SETMARGINWIDTHN, 0, 40); 
    Sci(SCI_SETMARGINWIDTHN, 1, 0); Sci(SCI_SETMARGINWIDTHN, 2, 0); 
    Sci(SCI_SETENDATLASTLINE, 0);

    Sci(SCI_STYLESETBACK, STYLE_LINENUMBER, 0x2B2521); Sci(SCI_STYLESETFORE, STYLE_LINENUMBER, 0x70635C);
    Sci(SCI_STYLESETFONT, STYLE_LINENUMBER, (LPARAM)"JetBrains Mono Light"); Sci(SCI_STYLESETSIZE, STYLE_LINENUMBER, editorFontSize - 1);
    Sci(SCI_STYLESETBACK, 40, 0x2B2521); Sci(SCI_STYLESETFORE, 40, 0xFFFFFF);
    Sci(SCI_STYLESETFONT, 40, (LPARAM)"JetBrains Mono Medium"); Sci(SCI_STYLESETSIZE, 40, editorFontSize - 1); Sci(SCI_STYLESETBOLD, 40, TRUE);
    Sci(SCI_SETCARETFORE, 0xFF8B52); Sci(SCI_SETCARETWIDTH, 2);
    Sci(SCI_SETCARETLINEVISIBLE, TRUE); Sci(SCI_SETCARETLINEBACK, 0x3C312C);
    Sci(SCI_SETCARETLINEVISIBLEALWAYS, TRUE);
    Sci(SCI_SETSELBACK, TRUE, 0x51443E); Sci(SCI_SETVSCROLLBAR, FALSE); Sci(SCI_SETHSCROLLBAR, FALSE);
    Sci(SCI_SETSCROLLWIDTHTRACKING, TRUE);
    Sci(SCI_SETINDENTATIONGUIDES, showIndentGuides ? SC_IV_LOOKBOTH : SC_IV_NONE);
    Sci(SCI_SETVIEWWS, showWhitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE);
    Sci(SCI_SETVIEWEOL, FALSE);
    Sci(SCI_SETCARETSTYLE, caretStyleBlock ? CARETSTYLE_BLOCK : CARETSTYLE_LINE);
    ApplySyntax();
    UpdateLineNumberWidth();
}

void SyncLineNumbers(bool rebuild) {
    int total = Sci(SCI_GETLINECOUNT);
    if (!rebuild && total > 0) {
        char testBuf[16] = { 0 };
        Sci(SCI_MARGINGETTEXT, 0, (LPARAM)testBuf);
        if (testBuf[0] == 0) {
            rebuild = true;
        }
    }
    if (rebuild) {
        for (int i = 0; i < total; ++i) {
            char buf[16]; sprintf_s(buf, "%d ", i + 1); 
            Sci(SCI_MARGINSETTEXT, i, (LPARAM)buf); Sci(SCI_MARGINSETSTYLE, i, STYLE_LINENUMBER);
        }
    }
    int sPos = Sci(SCI_GETSELECTIONSTART), ePos = Sci(SCI_GETSELECTIONEND);
    int s = Sci(SCI_LINEFROMPOSITION, sPos), e = Sci(SCI_LINEFROMPOSITION, ePos);
    if (e > s && ePos == Sci(SCI_POSITIONFROMLINE, e)) e--;
    if (!rebuild && (activeLineStart == s && activeLineEnd == e)) return;
    if (activeLineStart != -1) {
        for (int i = max(0, activeLineStart); i <= min(total - 1, activeLineEnd); ++i) Sci(SCI_MARGINSETSTYLE, i, STYLE_LINENUMBER);
    }
    for (int i = max(0, s); i <= min(total - 1, e); ++i) Sci(SCI_MARGINSETSTYLE, i, 40);
    activeLineStart = s; activeLineEnd = e;
    UpdateLineNumberWidth();
}

void UpdateCurrentMatchIndex() {
    if (searchMatches.empty()) {
        currentMatchIndex = 0;
        return;
    }
    int curPos = (int)Sci(SCI_GETCURRENTPOS);
    int sPos = (int)Sci(SCI_GETSELECTIONSTART);
    int ePos = (int)Sci(SCI_GETSELECTIONEND);
    
    // 1. Check if the current selection is exactly one of the matches
    for (size_t i = 0; i < searchMatches.size(); ++i) {
        if (searchMatches[i].first == sPos && searchMatches[i].second == ePos) {
            currentMatchIndex = (int)i + 1;
            return;
        }
    }
    
    // 2. Otherwise, find the first match after or at curPos
    for (size_t i = 0; i < searchMatches.size(); ++i) {
        if (searchMatches[i].first >= curPos) {
            currentMatchIndex = (int)i + 1;
            return;
        }
    }
    
    // 3. Fallback: wrap around to the first match
    currentMatchIndex = 1;
}

void UpdateSearchMatches() {
    searchMatches.clear();
    currentMatchIndex = 0;
    totalMatchesCount = 0;
    
    if (!hwndSearchEdit) return;
    int len = GetWindowTextLengthW(hwndSearchEdit);
    if (len == 0) {
        UpdateUI(hwndMain);
        return;
    }
    
    std::wstring query(len + 1, 0);
    GetWindowTextW(hwndSearchEdit, &query[0], len + 1);
    query.resize(len);
    
    int u8len = WideCharToMultiByte(CP_UTF8, 0, query.c_str(), -1, NULL, 0, NULL, NULL);
    std::string u8query(u8len, 0);
    WideCharToMultiByte(CP_UTF8, 0, query.c_str(), -1, &u8query[0], u8len, NULL, NULL);
    if (!u8query.empty() && u8query.back() == '\0') u8query.pop_back();
    
    if (u8query.empty()) {
        UpdateUI(hwndMain);
        return;
    }
    
    int docLen = (int)Sci(SCI_GETLENGTH);
    int pos = 0;
    while (pos < docLen) {
        Sci_TextToFind ft = { { pos, docLen }, (char*)u8query.c_str() };
        int found = (int)Sci(SCI_FINDTEXT, SCFIND_NONE, (LPARAM)&ft);
        if (found == -1) break;
        searchMatches.push_back({ ft.chrgText.cpMin, ft.chrgText.cpMax });
        pos = ft.chrgText.cpMax;
        if (ft.chrgText.cpMin == ft.chrgText.cpMax) pos++;
    }
    
    totalMatchesCount = (int)searchMatches.size();
    UpdateCurrentMatchIndex();
    UpdateUI(hwndMain);
}

void SearchNext() {
    if (searchMatches.empty()) return;
    int curPos = (int)Sci(SCI_GETCURRENTPOS);
    int sPos = (int)Sci(SCI_GETSELECTIONSTART);
    int ePos = (int)Sci(SCI_GETSELECTIONEND);
    
    int nextIdx = 0;
    bool found = false;
    for (size_t i = 0; i < searchMatches.size(); ++i) {
        if (searchMatches[i].first == sPos && searchMatches[i].second == ePos) {
            nextIdx = (int)i + 1;
            found = true;
            break;
        }
    }
    
    if (!found) {
        for (size_t i = 0; i < searchMatches.size(); ++i) {
            if (searchMatches[i].first >= curPos) {
                nextIdx = (int)i;
                found = true;
                break;
            }
        }
    }
    
    if (nextIdx >= (int)searchMatches.size()) nextIdx = 0;
    
    int start = searchMatches[nextIdx].first;
    int end = searchMatches[nextIdx].second;
    Sci(SCI_SETSEL, start, end);
    Sci(SCI_VERTICALCENTRECARET);
    
    currentMatchIndex = nextIdx + 1;
    UpdateUI(hwndMain);
}

void SearchPrev() {
    if (searchMatches.empty()) return;
    int curPos = (int)Sci(SCI_GETCURRENTPOS);
    int sPos = (int)Sci(SCI_GETSELECTIONSTART);
    int ePos = (int)Sci(SCI_GETSELECTIONEND);
    
    int prevIdx = -1;
    bool found = false;
    for (size_t i = 0; i < searchMatches.size(); ++i) {
        if (searchMatches[i].first == sPos && searchMatches[i].second == ePos) {
            prevIdx = (int)i - 1;
            found = true;
            break;
        }
    }
    
    if (!found) {
        for (int i = (int)searchMatches.size() - 1; i >= 0; --i) {
            if (searchMatches[i].second <= curPos) {
                prevIdx = i;
                found = true;
                break;
            }
        }
    }
    
    if (prevIdx < 0) prevIdx = (int)searchMatches.size() - 1;
    
    int start = searchMatches[prevIdx].first;
    int end = searchMatches[prevIdx].second;
    Sci(SCI_SETSEL, start, end);
    Sci(SCI_VERTICALCENTRECARET);
    
    currentMatchIndex = prevIdx + 1;
    UpdateUI(hwndMain);
}

void SearchSelectAll() {
    if (searchMatches.empty()) return;
    
    Sci(SCI_SETMULTIPLESELECTION, TRUE);
    Sci(SCI_SETADDITIONALSELECTIONTYPING, TRUE);
    
    int start0 = searchMatches[0].first;
    int end0 = searchMatches[0].second;
    Sci(SCI_SETSEL, start0, end0);
    
    for (size_t i = 1; i < searchMatches.size(); ++i) {
        Sci(SCI_ADDSELECTION, searchMatches[i].first, searchMatches[i].second);
    }
    Sci(SCI_VERTICALCENTRECARET);
    UpdateUI(hwndMain);
}

void SearchReplace() {
    if (searchMatches.empty()) return;
    
    int sPos = (int)Sci(SCI_GETSELECTIONSTART);
    int ePos = (int)Sci(SCI_GETSELECTIONEND);
    
    int matchIdx = -1;
    for (size_t i = 0; i < searchMatches.size(); ++i) {
        if (searchMatches[i].first == sPos && searchMatches[i].second == ePos) {
            matchIdx = (int)i;
            break;
        }
    }
    
    if (matchIdx == -1) {
        SearchNext();
        return;
    }
    
    int len = GetWindowTextLengthW(hwndReplaceEdit);
    std::wstring wrep(len + 1, 0);
    GetWindowTextW(hwndReplaceEdit, &wrep[0], len + 1);
    wrep.resize(len);
    
    int u8len = WideCharToMultiByte(CP_UTF8, 0, wrep.c_str(), -1, NULL, 0, NULL, NULL);
    std::string u8rep(u8len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wrep.c_str(), -1, &u8rep[0], u8len, NULL, NULL);
    if (!u8rep.empty() && u8rep.back() == '\0') u8rep.pop_back();
    
    Sci(SCI_REPLACESEL, 0, (LPARAM)u8rep.c_str());
    
    UpdateSearchMatches();
    SearchNext();
}

void SearchReplaceAll() {
    if (searchMatches.empty()) return;
    
    int len = GetWindowTextLengthW(hwndReplaceEdit);
    std::wstring wrep(len + 1, 0);
    GetWindowTextW(hwndReplaceEdit, &wrep[0], len + 1);
    wrep.resize(len);
    
    int u8len = WideCharToMultiByte(CP_UTF8, 0, wrep.c_str(), -1, NULL, 0, NULL, NULL);
    std::string u8rep(u8len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wrep.c_str(), -1, &u8rep[0], u8len, NULL, NULL);
    if (!u8rep.empty() && u8rep.back() == '\0') u8rep.pop_back();
    
    Sci(SCI_BEGINUNDOACTION);
    for (int i = (int)searchMatches.size() - 1; i >= 0; --i) {
        Sci(SCI_SETSEL, searchMatches[i].first, searchMatches[i].second);
        Sci(SCI_REPLACESEL, 0, (LPARAM)u8rep.c_str());
    }
    Sci(SCI_ENDUNDOACTION);
    
    UpdateSearchMatches();
}
