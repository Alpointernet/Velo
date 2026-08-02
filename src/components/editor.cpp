#include "../globals.h"
#include "editor.h"
#include "ui_drawing.h"
#include "dialogs.h"
#include "tabmanager.h"
#include <vector>

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
    std::string manualLang = "";
    if (activeTabIndex < tabs.size()) {
        std::wstring fp = tabs[activeTabIndex].filePath;
        size_t dot = fp.find_last_of(L'.');
        if (dot != std::wstring::npos) ext = fp.substr(dot + 1);
        manualLang = tabs[activeTabIndex].manualLanguage;
    }
    const char* lang = "null"; int lex = SCLEX_NULL;
    const char *kw0 = "", *kw1 = "", *kw3 = "";

    if (!manualLang.empty()) {
        if (manualLang == "cpp") { lang = "cpp"; lex = SCLEX_CPP; }
        else if (manualLang == "python") { lang = "python"; lex = SCLEX_PYTHON; }
        else if (manualLang == "hypertext") { lang = "hypertext"; lex = SCLEX_HTML; }
        else if (manualLang == "css") { lang = "css"; lex = SCLEX_CSS; }
        else if (manualLang == "markdown") { lang = "markdown"; lex = SCLEX_MARKDOWN; }
        else if (manualLang == "null") { lang = "null"; lex = SCLEX_NULL; }

        if (lex == SCLEX_CPP) {
            kw0 = "int float double char void bool long short signed unsigned auto const static extern inline virtual public private protected struct class enum union namespace template typename typedef false true null nullptr var let function extends export import from default if else for while do switch case break continue return goto try catch throw new delete and or not NULL TRUE FALSE WINAPI CALLBACK package func type interface defer select chan range map make append len cap println printf fmt using override abstract final sealed async await yield base this super implements throws synchronized volatile transient native";
            kw1 = "WinMain WndProc ScrollbarProc SearchEditProc CreateWindowExW LoadCursorW RegisterClassW ShowWindow GetMessageW TranslateMessage DispatchMessageW PostMessage DefWindowProcW GetClientRect GetWindowRect FillRectColor SendMessage GetProcAddress LoadLibraryW LoadLibraryExW MessageBoxW CreateFileW ReadFile WriteFile GetFileSize CloseHandle wcscpy_s GetOpenFileNameW GetSaveFileNameW CreatePopupMenu AppendMenuW ClientToScreen TrackPopupMenu DestroyMenu PostQuitMessage CreateFontW GetDC SelectObject DrawTextW ReleaseDC SetTextColor SetBkMode SetBkColor ExtTextOutW AlphaBlend";
            kw3 = "std string wstring vector map set pair string_view cout cin endl HANDLE HWND HDC HFONT HBITMAP HMENU HINSTANCE LPSTR LPCWSTR WNDCLASSW WNDPROC MSG RECT POINT SIZE FILE COLORREF DWORD WORD BYTE INT_PTR LONG_PTR LRESULT UINT WPARAM LPARAM BOOL sptr_t Tab HoverElement console window document Math JSON Promise Array Object String Number Boolean SELECT FROM WHERE INSERT UPDATE DELETE CREATE TABLE ALTER DROP INDEX JOIN LEFT RIGHT INNER OUTER ON AS AND OR NOT IN EXISTS GROUP BY ORDER HAVING LIMIT OFFSET UNION INTO VALUES SET";
        } else if (lex == SCLEX_PYTHON) {
            kw0 = "False None True and as assert async await break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass raise return try while with yield";
            kw1 = "print len range str int float list dict set tuple bool";
        } else if (lex == SCLEX_HTML) {
            kw0 = "html head title body div span a img ul li table tr td th form input button script style link meta header footer nav section article main p h1 h2 h3 h4 h5 h6 br hr";
        } else if (lex == SCLEX_CSS) {
            kw0 = "color background background-color background-image background-repeat background-position background-size background-attachment border border-top border-right border-bottom border-left border-color border-style border-width border-radius box-shadow box-sizing clear clip content cursor direction display float font font-family font-size font-style font-variant font-weight height left letter-spacing line-height list-style list-style-image list-style-position list-style-type margin margin-top margin-right margin-bottom margin-left max-height max-width min-height min-width opacity outline outline-color outline-style outline-width overflow overflow-x overflow-y padding padding-top padding-right padding-bottom padding-left position right text-align text-decoration text-indent text-overflow text-shadow text-transform top transition transform vertical-align visibility white-space width word-break word-spacing word-wrap z-index flex flex-basis flex-direction flex-flow flex-grow flex-shrink flex-wrap align-content align-items align-self justify-content order grid grid-area grid-column grid-gap grid-row grid-template animation gap place-items";
            kw1 = "active after before checked disabled empty enabled first-child first-letter first-line first-of-type focus hover in-range invalid lang last-child last-of-type link not nth-child nth-last-child nth-last-of-type nth-of-type only-child only-of-type optional out-of-range placeholder read-only read-write required root selection target valid visited";
        }
    } else if (!_wcsicmp(ext.c_str(), L"cpp") || !_wcsicmp(ext.c_str(), L"c") || !_wcsicmp(ext.c_str(), L"h") || !_wcsicmp(ext.c_str(), L"hpp") || !_wcsicmp(ext.c_str(), L"cc") ||
        !_wcsicmp(ext.c_str(), L"js") || !_wcsicmp(ext.c_str(), L"jsx") || !_wcsicmp(ext.c_str(), L"ts") || !_wcsicmp(ext.c_str(), L"tsx") ||
        !_wcsicmp(ext.c_str(), L"json") || !_wcsicmp(ext.c_str(), L"jsonc") || !_wcsicmp(ext.c_str(), L"ahk") ||
        !_wcsicmp(ext.c_str(), L"cs") || !_wcsicmp(ext.c_str(), L"java") || !_wcsicmp(ext.c_str(), L"go") || !_wcsicmp(ext.c_str(), L"rs") ||
        !_wcsicmp(ext.c_str(), L"sql") || !_wcsicmp(ext.c_str(), L"rc") || !_wcsicmp(ext.c_str(), L"iss")) {
        lang = "cpp"; lex = SCLEX_CPP;
        kw0 = "int float double char void bool long short signed unsigned auto const static extern inline virtual public private protected struct class enum union namespace template typename typedef false true null nullptr var let function extends export import from default if else for while do switch case break continue return goto try catch throw new delete and or not NULL TRUE FALSE WINAPI CALLBACK package func type interface defer select chan range map make append len cap println printf fmt using override abstract final sealed async await yield base this super implements throws synchronized volatile transient native";
        kw1 = "WinMain WndProc ScrollbarProc SearchEditProc CreateWindowExW LoadCursorW RegisterClassW ShowWindow GetMessageW TranslateMessage DispatchMessageW PostMessage DefWindowProcW GetClientRect GetWindowRect FillRectColor SendMessage GetProcAddress LoadLibraryW LoadLibraryExW MessageBoxW CreateFileW ReadFile WriteFile GetFileSize CloseHandle wcscpy_s GetOpenFileNameW GetSaveFileNameW CreatePopupMenu AppendMenuW ClientToScreen TrackPopupMenu DestroyMenu PostQuitMessage CreateFontW GetDC SelectObject DrawTextW ReleaseDC SetTextColor SetBkMode SetBkColor ExtTextOutW AlphaBlend";
        kw3 = "std string wstring vector map set pair string_view cout cin endl HANDLE HWND HDC HFONT HBITMAP HMENU HINSTANCE LPSTR LPCWSTR WNDCLASSW WNDPROC MSG RECT POINT SIZE FILE COLORREF DWORD WORD BYTE INT_PTR LONG_PTR LRESULT UINT WPARAM LPARAM BOOL sptr_t Tab HoverElement console window document Math JSON Promise Array Object String Number Boolean SELECT FROM WHERE INSERT UPDATE DELETE CREATE TABLE ALTER DROP INDEX JOIN LEFT RIGHT INNER OUTER ON AS AND OR NOT IN EXISTS GROUP BY ORDER HAVING LIMIT OFFSET UNION INTO VALUES SET";
    } else if (!_wcsicmp(ext.c_str(), L"py")) {
        lang = "python"; lex = SCLEX_PYTHON;
        kw0 = "False None True and as assert async await break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass raise return try while with yield";
        kw1 = "print len range str int float list dict set tuple bool";
    } else if (!_wcsicmp(ext.c_str(), L"html") || !_wcsicmp(ext.c_str(), L"htm") || !_wcsicmp(ext.c_str(), L"xml")) {
        lang = "hypertext"; lex = SCLEX_HTML;
        kw0 = "html head title body div span a img ul li table tr td th form input button script style link meta header footer nav section article main p h1 h2 h3 h4 h5 h6 br hr";
    } else if (!_wcsicmp(ext.c_str(), L"css") || !_wcsicmp(ext.c_str(), L"scss")) {
        lang = "css"; lex = SCLEX_CSS;
        kw0 = "color background background-color background-image background-repeat background-position background-size background-attachment border border-top border-right border-bottom border-left border-color border-style border-width border-radius box-shadow box-sizing clear clip content cursor direction display float font font-family font-size font-style font-variant font-weight height left letter-spacing line-height list-style list-style-image list-style-position list-style-type margin margin-top margin-right margin-bottom margin-left max-height max-width min-height min-width opacity outline outline-color outline-style outline-width overflow overflow-x overflow-y padding padding-top padding-right padding-bottom padding-left position right text-align text-decoration text-indent text-overflow text-shadow text-transform top transition transform vertical-align visibility white-space width word-break word-spacing word-wrap z-index flex flex-basis flex-direction flex-flow flex-grow flex-shrink flex-wrap align-content align-items align-self justify-content order grid grid-area grid-column grid-gap grid-row grid-template animation gap place-items";
        kw1 = "active after before checked disabled empty enabled first-child first-letter first-line first-of-type focus hover in-range invalid lang last-child last-of-type link not nth-child nth-last-child nth-last-of-type nth-of-type only-child only-of-type optional out-of-range placeholder read-only read-write required root selection target valid visited";
    } else if (!_wcsicmp(ext.c_str(), L"md") || !_wcsicmp(ext.c_str(), L"markdown")) {
        lang = "markdown"; lex = SCLEX_MARKDOWN;
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
    Sci(SCI_SETPROPERTY, (LPARAM)"lexer.markdown.header.eolfill", (LPARAM)"1");
    Sci(SCI_SETKEYWORDS, 0, (LPARAM)kw0); Sci(SCI_SETKEYWORDS, 1, (LPARAM)kw1); Sci(SCI_SETKEYWORDS, 3, (LPARAM)kw3);

    // Set default base styles
    for (int i = 0; i <= 127; ++i) {
        Sci(SCI_STYLESETBACK, i, theme.editorBg); Sci(SCI_STYLESETFORE, i, theme.editorFg);
        Sci(SCI_STYLESETFONT, i, (LPARAM)"JetBrains Mono Medium"); Sci(SCI_STYLESETSIZE, i, editorFontSize);
        Sci(SCI_STYLESETBOLD, i, FALSE);
        Sci(SCI_STYLESETITALIC, i, FALSE);
    }
    
    Sci(SCI_SETCARETFORE, theme.accent, 0); Sci(SCI_SETCARETWIDTH, 2);
    Sci(SCI_SETCARETLINEVISIBLE, TRUE); Sci(SCI_SETCARETLINEBACK, theme.editorActiveLineBg); Sci(SCI_SETCARETLINEBACKALPHA, 256);
    Sci(SCI_SETCARETLINEVISIBLEALWAYS, TRUE);
    Sci(SCI_SETSELBACK, TRUE, theme.editorSelectionBg);
    
    // Style Line Numbers
    Sci(SCI_STYLESETBACK, STYLE_LINENUMBER, theme.editorBg); Sci(SCI_STYLESETFORE, STYLE_LINENUMBER, theme.editorLineNumberFg);
    Sci(SCI_STYLESETFONT, STYLE_LINENUMBER, (LPARAM)"JetBrains Mono Medium"); Sci(SCI_STYLESETSIZE, STYLE_LINENUMBER, editorFontSize - 1);
    Sci(SCI_STYLESETBACK, 40, theme.editorBg); Sci(SCI_STYLESETFORE, 40, theme.textActive);
    Sci(SCI_STYLESETFONT, 40, (LPARAM)"JetBrains Mono Medium"); Sci(SCI_STYLESETSIZE, 40, editorFontSize - 1); Sci(SCI_STYLESETBOLD, 40, TRUE);
    
    // Style Brace Matching
    Sci(SCI_STYLESETFORE, STYLE_BRACELIGHT, theme.accent);
    Sci(SCI_STYLESETBACK, STYLE_BRACELIGHT, theme.editorActiveLineBg);
    Sci(SCI_STYLESETBOLD, STYLE_BRACELIGHT, TRUE);
    Sci(SCI_STYLESETFONT, STYLE_BRACELIGHT, (LPARAM)"JetBrains Mono Medium");
    Sci(SCI_STYLESETSIZE, STYLE_BRACELIGHT, editorFontSize);
    Sci(SCI_STYLESETFORE, STYLE_BRACEBAD, RGB(0xF4, 0x43, 0x36));
    Sci(SCI_STYLESETBACK, STYLE_BRACEBAD, theme.editorBg);
    Sci(SCI_STYLESETBOLD, STYLE_BRACEBAD, TRUE);
    Sci(SCI_STYLESETFONT, STYLE_BRACEBAD, (LPARAM)"JetBrains Mono Medium");
    Sci(SCI_STYLESETSIZE, STYLE_BRACEBAD, editorFontSize);
    
    // Setup Custom Indicators
    Sci(SCI_INDICSETSTYLE, INDICATOR_URL, INDIC_TEXTFORE);
    Sci(SCI_INDICSETFORE, INDICATOR_URL, theme.synComment);
    Sci(SCI_INDICSETHOVERSTYLE, INDICATOR_URL, INDIC_PLAIN);
    Sci(SCI_INDICSETHOVERFORE, INDICATOR_URL, theme.synComment);
    
    Sci(SCI_INDICSETSTYLE, INDICATOR_STRIKE, INDIC_STRIKE);
    Sci(SCI_INDICSETFORE, INDICATOR_STRIKE, 0x808080);
    
    // Style the indentation guides
    Sci(SCI_STYLESETBACK, STYLE_INDENTGUIDE, theme.editorBg);
    Sci(SCI_STYLESETFORE, STYLE_INDENTGUIDE, RGB(0x6E, 0x76, 0x89));

    // Apply document-specific settings so they persist across tab switches/new documents
    Sci(SCI_SETTABWIDTH, editorTabWidth);
    Sci(SCI_SETINDENTATIONGUIDES, showIndentGuides ? SC_IV_LOOKBOTH : SC_IV_NONE);
    Sci(SCI_SETVIEWWS, showWhitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE);


    if (lex == SCLEX_CPP) {
        Sci(SCI_STYLESETFORE, 1, theme.synComment); Sci(SCI_STYLESETFORE, 2, theme.synComment);
        Sci(SCI_STYLESETFORE, 4, theme.synNumber); Sci(SCI_STYLESETFORE, 5, theme.synKeyword); Sci(SCI_STYLESETBOLD, 5, FALSE);
        Sci(SCI_STYLESETFORE, 6, theme.synString); Sci(SCI_STYLESETFORE, 7, theme.synString);
        Sci(SCI_STYLESETFORE, 9, theme.synConstant); Sci(SCI_STYLESETFORE, 10, theme.editorFg);
        Sci(SCI_STYLESETFORE, 11, theme.synVariable); Sci(SCI_STYLESETFORE, 16, theme.synFunction);
        Sci(SCI_STYLESETFORE, 19, theme.synType);
    } else if (lex == SCLEX_PYTHON) {
        Sci(SCI_STYLESETFORE, 1, theme.synComment); Sci(SCI_STYLESETFORE, 2, theme.synNumber);
        Sci(SCI_STYLESETFORE, 3, theme.synString); Sci(SCI_STYLESETFORE, 4, theme.synString);
        Sci(SCI_STYLESETFORE, 5, theme.synConstant); Sci(SCI_STYLESETFORE, 8, theme.synType);
        Sci(SCI_STYLESETFORE, 9, theme.synFunction); Sci(SCI_STYLESETFORE, 10, theme.editorFg);
        Sci(SCI_STYLESETFORE, 11, theme.synVariable); Sci(SCI_STYLESETFORE, 14, theme.synKeyword);
    } else if (lex == SCLEX_HTML) {
        Sci(SCI_STYLESETFORE, 1, theme.synKeyword); Sci(SCI_STYLESETFORE, 3, theme.synVariable);
        Sci(SCI_STYLESETFORE, 5, theme.synNumber); Sci(SCI_STYLESETFORE, 6, theme.synString);
        Sci(SCI_STYLESETFORE, 9, theme.synComment);
    } else if (lex == SCLEX_CSS) {
        // SCE_CSS_COMMENT=9, SCE_CSS_TAG=1, SCE_CSS_CLASS=2, SCE_CSS_PSEUDOCLASS=3,
        // SCE_CSS_UNKNOWN_PSEUDOCLASS=4, SCE_CSS_OPERATOR=5, SCE_CSS_IDENTIFIER=6,
        // SCE_CSS_UNKNOWN_IDENTIFIER=7, SCE_CSS_VALUE=8, SCE_CSS_DOUBLESTRING=13,
        // SCE_CSS_SINGLESTRING=14, SCE_CSS_IDENTIFIER2=15, SCE_CSS_ATTRIBUTE=16,
        // SCE_CSS_ID=10, SCE_CSS_IMPORTANT=11, SCE_CSS_DIRECTIVE=12
        Sci(SCI_STYLESETFORE, 1, theme.synKeyword);     // Tag selectors (div, body, etc.)
        Sci(SCI_STYLESETFORE, 2, theme.synFunction);     // .class selectors
        Sci(SCI_STYLESETFORE, 3, theme.synConstant);     // :pseudo-class
        Sci(SCI_STYLESETFORE, 4, theme.synConstant);     // Unknown pseudo-class
        Sci(SCI_STYLESETFORE, 5, theme.editorFg);        // Operators ({, }, :, ;)
        Sci(SCI_STYLESETFORE, 6, theme.synVariable);     // CSS properties (known)
        Sci(SCI_STYLESETFORE, 7, theme.synVariable);     // CSS properties (unknown)
        Sci(SCI_STYLESETFORE, 8, theme.synNumber);       // Values
        Sci(SCI_STYLESETFORE, 9, theme.synComment);      // Comments
        Sci(SCI_STYLESETFORE, 10, theme.synString);      // #id selectors
        Sci(SCI_STYLESETFORE, 11, theme.synConstant); Sci(SCI_STYLESETBOLD, 11, TRUE); // !important
        Sci(SCI_STYLESETFORE, 12, theme.synConstant);    // @directives (@media, @import)
        Sci(SCI_STYLESETFORE, 13, theme.synString);      // Double-quoted strings
        Sci(SCI_STYLESETFORE, 14, theme.synString);      // Single-quoted strings
        Sci(SCI_STYLESETFORE, 15, theme.synType);        // Pseudo-elements (kw1)
        Sci(SCI_STYLESETFORE, 16, theme.synFunction);    // Attributes [attr]
    } else if (lex == SCLEX_MARKDOWN) {
        Sci(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER1, theme.synString); Sci(SCI_STYLESETBOLD, SCE_MARKDOWN_HEADER1, TRUE);
        Sci(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER2, theme.synString); Sci(SCI_STYLESETBOLD, SCE_MARKDOWN_HEADER2, TRUE);
        Sci(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER3, theme.synString); Sci(SCI_STYLESETBOLD, SCE_MARKDOWN_HEADER3, TRUE);
        Sci(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER4, theme.synString); Sci(SCI_STYLESETBOLD, SCE_MARKDOWN_HEADER4, TRUE);
        Sci(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER5, theme.synString); Sci(SCI_STYLESETBOLD, SCE_MARKDOWN_HEADER5, TRUE);
        Sci(SCI_STYLESETFORE, SCE_MARKDOWN_HEADER6, theme.synString); Sci(SCI_STYLESETBOLD, SCE_MARKDOWN_HEADER6, TRUE);
        Sci(SCI_STYLESETFORE, SCE_MARKDOWN_STRONG1, theme.synKeyword); Sci(SCI_STYLESETBOLD, SCE_MARKDOWN_STRONG1, TRUE);
        Sci(SCI_STYLESETFORE, SCE_MARKDOWN_STRONG2, theme.synKeyword); Sci(SCI_STYLESETBOLD, SCE_MARKDOWN_STRONG2, TRUE);
        Sci(SCI_STYLESETFORE, SCE_MARKDOWN_EM1, theme.synNumber); Sci(SCI_STYLESETITALIC, SCE_MARKDOWN_EM1, TRUE);
        Sci(SCI_STYLESETFORE, SCE_MARKDOWN_EM2, theme.synNumber); Sci(SCI_STYLESETITALIC, SCE_MARKDOWN_EM2, TRUE);
        
        // Link clicking setup (native lexer links)
        Sci(SCI_STYLESETHOTSPOT, SCE_MARKDOWN_LINK, TRUE);
        Sci(SCI_SETHOTSPOTACTIVEFORE, TRUE, theme.synComment);
        Sci(SCI_SETHOTSPOTACTIVEUNDERLINE, TRUE);
    }
    
    Sci(SCI_COLOURISE, 0, -1);
    UpdateCustomIndicators(hwndScintilla);
    UpdateBraceMatch();
}

void UpdateCustomIndicators(HWND hwndScintilla) {
    if (!hwndScintilla) return;
    
    static DWORD lastUpdate = 0;
    DWORD now = GetTickCount();
    if (now - lastUpdate < 30) return;
    lastUpdate = now;
    
    int docLen = Sci(SCI_GETLENGTH);
    if (docLen <= 0) return;
    
    int firstVisibleLine = Sci(SCI_GETFIRSTVISIBLELINE);
    int linesOnScreen = Sci(SCI_LINESONSCREEN);
    int startPos = Sci(SCI_POSITIONFROMLINE, firstVisibleLine);
    int endPos = Sci(SCI_POSITIONFROMLINE, firstVisibleLine + linesOnScreen + 1);
    if (endPos == -1 || endPos > docLen) endPos = docLen;
    
    if (endPos <= startPos) return;
    
    int rangeLen = endPos - startPos;
    if (rangeLen > 262144) { startPos = endPos - 262144; rangeLen = 262144; }
    std::vector<char> buf(rangeLen + 1);
    Sci_TextRange tr;
    tr.chrg.cpMin = startPos;
    tr.chrg.cpMax = endPos;
    tr.lpstrText = buf.data();
    Sci(SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
    
    Sci(SCI_SETINDICATORCURRENT, INDICATOR_URL);
    Sci(SCI_INDICATORCLEARRANGE, startPos, rangeLen);
    
    const char* p = buf.data();
    const char* end = buf.data() + rangeLen;
    while (p < end) {
        const char* urlStart = nullptr;
        if (p + 7 <= end && p[0] == 'h' && p[1] == 't' && p[2] == 't' && p[3] == 'p' && p[4] == ':' && p[5] == '/' && p[6] == '/') {
            urlStart = p;
        } else if (p + 8 <= end && p[0] == 'h' && p[1] == 't' && p[2] == 't' && p[3] == 'p' && p[4] == 's' && p[5] == ':' && p[6] == '/' && p[7] == '/') {
            urlStart = p;
        }
        if (urlStart) {
            const char* urlEnd = urlStart;
            while (urlEnd < end) {
                char c = *urlEnd;
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '"' || c == '\'' || c == '<' || c == '>' || c == '[' || c == ']' || c == '(' || c == ')') break;
                urlEnd++;
            }
            int matchStart = startPos + (int)(urlStart - buf.data());
            int matchLen = (int)(urlEnd - urlStart);
            Sci(SCI_INDICATORFILLRANGE, matchStart, matchLen);
            p = urlEnd;
        } else {
            p++;
        }
    }
}

void StyleScintilla(HWND hwndSci) {
    SetWindowTheme(hwndSci, L"DarkMode_Explorer", NULL); ApplyDarkMode(hwndSci);
    Sci(SCI_SETMARGINTYPEN, 0, SC_MARGIN_RTEXT);
    Sci(SCI_SETMARGINWIDTHN, 0, 40); 
    Sci(SCI_SETMARGINWIDTHN, 1, 0); Sci(SCI_SETMARGINWIDTHN, 2, 0); 
    Sci(SCI_SETENDATLASTLINE, 0);

    Sci(SCI_SETVSCROLLBAR, FALSE); Sci(SCI_SETHSCROLLBAR, FALSE);
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

void UpdateBraceMatch() {
    if (!hwndScintilla) return;
    int pos = Sci(SCI_GETCURRENTPOS);
    int docLen = Sci(SCI_GETLENGTH);

    auto getChar = [](int p) -> char { return (char)Sci(SCI_GETCHARAT, p); };
    auto toLower = [](char c) -> char { return (c >= 'A' && c <= 'Z') ? c + 32 : c; };
    auto isTagNameChar = [](char c) -> bool {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    };

    // 1) Try brace matching: (, ), {, }, [, ]
    auto isBrace = [](char ch) -> bool {
        return ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '[' || ch == ']';
    };

    int bracePos = -1;
    char chAt = getChar(pos);
    char chBefore = pos > 0 ? getChar(pos - 1) : 0;

    if (isBrace(chAt)) {
        bracePos = pos;
    } else if (isBrace(chBefore)) {
        bracePos = pos - 1;
    }

    if (bracePos >= 0) {
        int matchPos = Sci(SCI_BRACEMATCH, bracePos, 0);
        if (matchPos >= 0) {
            Sci(SCI_BRACEHIGHLIGHT, bracePos, matchPos);
            int line1 = Sci(SCI_LINEFROMPOSITION, bracePos);
            int line2 = Sci(SCI_LINEFROMPOSITION, matchPos);
            if (line1 != line2) {
                int col1 = Sci(SCI_GETLINEINDENTATION, line1);
                int col2 = Sci(SCI_GETLINEINDENTATION, line2);
                int guideCol = col1 < col2 ? col1 : col2;
                int tabW = Sci(SCI_GETTABWIDTH);
                if (tabW > 0) guideCol = (guideCol / tabW) * tabW;
                Sci(SCI_SETHIGHLIGHTGUIDE, guideCol);
            } else {
                Sci(SCI_SETHIGHLIGHTGUIDE, 0);
            }
        } else {
            Sci(SCI_BRACEBADLIGHT, bracePos);
            Sci(SCI_SETHIGHLIGHTGUIDE, 0);
        }
        return;
    }

    // 2) HTML/XML tag matching
    // Find the < that opens the tag containing the cursor (search backward, skip comments)
    int tagOpen = -1;
    for (int i = pos; i >= max(0, pos - 1000); --i) {
        char c = getChar(i);
        if (c == '>') break;
        if (c == '<') { tagOpen = i; break; }
    }
    // Also search forward from cursor if not found backward
    if (tagOpen < 0) {
        for (int i = pos; i < min(docLen, pos + 1000); ++i) {
            char c = getChar(i);
            if (c == '<') { tagOpen = i; break; }
            if (c == '>') break;
        }
    }

    if (tagOpen < 0) {
        Sci(SCI_BRACEHIGHLIGHT, INVALID_POSITION, INVALID_POSITION);
        Sci(SCI_SETHIGHLIGHTGUIDE, 0);
        return;
    }

    // Find the closing > of this tag
    int tagClose = -1;
    int scanLimit = min(docLen, tagOpen + 1000);
    for (int i = tagOpen + 1; i < scanLimit; ++i) {
        char c = getChar(i);
        if (c == '>') { tagClose = i; break; }
        if (c == '<') break; // nested < is invalid, abort
    }

    if (tagClose < 0 || pos < tagOpen || pos > tagClose) {
        Sci(SCI_BRACEHIGHLIGHT, INVALID_POSITION, INVALID_POSITION);
        Sci(SCI_SETHIGHLIGHTGUIDE, 0);
        return;
    }

    // Skip HTML comments <!-- ... -->
    if (tagOpen + 3 < docLen && getChar(tagOpen + 1) == '!' && getChar(tagOpen + 2) == '-' && getChar(tagOpen + 3) == '-') {
        Sci(SCI_BRACEHIGHLIGHT, INVALID_POSITION, INVALID_POSITION);
        Sci(SCI_SETHIGHLIGHTGUIDE, 0);
        return;
    }
    // Skip CDATA <![CDATA[ ... ]]>
    if (tagOpen + 8 < docLen && getChar(tagOpen + 1) == '!' && getChar(tagOpen + 2) == '[') {
        Sci(SCI_BRACEHIGHLIGHT, INVALID_POSITION, INVALID_POSITION);
        Sci(SCI_SETHIGHLIGHTGUIDE, 0);
        return;
    }

    // Determine if closing tag: </tagname>
    bool isClosing = (getChar(tagOpen + 1) == '/');
    int nameStart = tagOpen + (isClosing ? 2 : 1);

    // Skip whitespace
    while (nameStart < tagClose && getChar(nameStart) == ' ') nameStart++;

    if (nameStart >= tagClose || !isTagNameChar(getChar(nameStart))) {
        Sci(SCI_BRACEHIGHLIGHT, INVALID_POSITION, INVALID_POSITION);
        Sci(SCI_SETHIGHLIGHTGUIDE, 0);
        return;
    }

    int nameEnd = nameStart;
    while (nameEnd < tagClose && isTagNameChar(getChar(nameEnd))) nameEnd++;
    int nameLen = nameEnd - nameStart;
    if (nameLen <= 0 || nameLen >= 128) {
        Sci(SCI_BRACEHIGHLIGHT, INVALID_POSITION, INVALID_POSITION);
        Sci(SCI_SETHIGHLIGHTGUIDE, 0);
        return;
    }

    char nameBuf[129];
    for (int i = 0; i < nameLen; ++i) nameBuf[i] = toLower(getChar(nameStart + i));
    nameBuf[nameLen] = '\0';

    // Check for self-closing: ends with />
    bool isSelfClosing = false;
    if (tagClose > 0 && getChar(tagClose - 1) == '/') {
        isSelfClosing = true;
    } else {
        // Check void elements
        const char* voidTags[] = {
            "area", "base", "br", "col", "embed", "hr", "img", "input",
            "link", "meta", "source", "track", "wbr", nullptr
        };
        for (int v = 0; voidTags[v]; ++v) {
            if (nameLen == (int)strlen(voidTags[v]) && !memcmp(nameBuf, voidTags[v], nameLen)) {
                isSelfClosing = true;
                break;
            }
        }
    }

    if (isSelfClosing) {
        Sci(SCI_BRACEHIGHLIGHT, INVALID_POSITION, INVALID_POSITION);
        Sci(SCI_SETHIGHLIGHTGUIDE, 0);
        return;
    }

    // Search for the matching tag
    int searchLimit = 200000;

    if (isClosing) {
        // Search backward for the matching opening tag
        int searchPos = tagOpen - 1;
        int depth = 0;
        while (searchPos >= 0 && (tagOpen - searchPos) < searchLimit) {
            char c = getChar(searchPos);
            if (c == '>') {
                // Found a potential tag end, now scan backward for its <
                int innerEnd = searchPos;
                int innerStart = searchPos;
                while (innerStart > 0) {
                    char ic = getChar(innerStart - 1);
                    if (ic == '<') { innerStart--; break; }
                    if (ic == '>') break; // Another tag boundary
                    innerStart--;
                    if ((innerEnd - innerStart) > 500) break;
                }

                if (innerStart >= 0 && getChar(innerStart) == '<') {
                    bool innerClosing = (getChar(innerStart + 1) == '/');
                    int iNameStart = innerStart + (innerClosing ? 2 : 1);
                    while (iNameStart < innerEnd && getChar(iNameStart) == ' ') iNameStart++;

                    if (iNameStart < innerEnd && isTagNameChar(getChar(iNameStart))) {
                        int iNameEnd = iNameStart;
                        while (iNameEnd < innerEnd && isTagNameChar(getChar(iNameEnd))) iNameEnd++;
                        int iNameLen = iNameEnd - iNameStart;

                        if (iNameLen == nameLen) {
                            bool match = true;
                            for (int k = 0; k < nameLen; ++k) {
                                if (toLower(getChar(iNameStart + k)) != nameBuf[k]) { match = false; break; }
                            }
                            if (match) {
                                if (innerClosing) {
                                    depth++;
                                } else {
                                    if (depth == 0) {
                                        Sci(SCI_BRACEHIGHLIGHT, tagOpen, innerStart);
                                        int l1 = Sci(SCI_LINEFROMPOSITION, tagOpen);
                                        int l2 = Sci(SCI_LINEFROMPOSITION, innerStart);
                                        if (l1 != l2) {
                                            int c1 = Sci(SCI_GETLINEINDENTATION, l1);
                                            int c2 = Sci(SCI_GETLINEINDENTATION, l2);
                                            int gc = c1 < c2 ? c1 : c2;
                                            int tw = Sci(SCI_GETTABWIDTH);
                                            if (tw > 0) gc = (gc / tw) * tw;
                                            Sci(SCI_SETHIGHLIGHTGUIDE, gc);
                                        } else {
                                            Sci(SCI_SETHIGHLIGHTGUIDE, 0);
                                        }
                                        return;
                                    }
                                    depth--;
                                }
                            }
                        }
                    }
                }
            }
            searchPos--;
        }
    } else {
        // Search forward for the matching closing tag
        int searchPos = tagClose + 1;
        int depth = 0;
        while (searchPos < docLen && (searchPos - tagClose) < searchLimit) {
            char c = getChar(searchPos);
            if (c == '<') {
                bool innerClosing = (getChar(searchPos + 1) == '/');
                int iNameStart = searchPos + (innerClosing ? 2 : 1);
                while (iNameStart < docLen && getChar(iNameStart) == ' ') iNameStart++;

                if (iNameStart < docLen && isTagNameChar(getChar(iNameStart))) {
                    int iNameEnd = iNameStart;
                    while (iNameEnd < docLen && isTagNameChar(getChar(iNameEnd))) iNameEnd++;
                    int iNameLen = iNameEnd - iNameStart;

                    // Check if it's a comment or CDATA — skip
                    if (iNameStart < docLen && getChar(iNameStart) == '!' && iNameLen > 1) {
                        searchPos = iNameEnd;
                        // Skip to the next >
                        while (searchPos < docLen && getChar(searchPos) != '>') searchPos++;
                        searchPos++;
                        continue;
                    }

                    if (iNameLen == nameLen) {
                        bool match = true;
                        for (int k = 0; k < nameLen; ++k) {
                            if (toLower(getChar(iNameStart + k)) != nameBuf[k]) { match = false; break; }
                        }
                        if (match) {
                            if (innerClosing) {
                                // Check this is the closing tag, not a self-closing opening tag
                                // Find the > for this tag
                                int thisEnd = iNameEnd;
                                while (thisEnd < docLen && getChar(thisEnd) != '>') thisEnd++;
                                if (thisEnd < docLen) {
                                    if (depth == 0) {
                                        Sci(SCI_BRACEHIGHLIGHT, tagOpen, searchPos);
                                        int l1 = Sci(SCI_LINEFROMPOSITION, tagOpen);
                                        int l2 = Sci(SCI_LINEFROMPOSITION, searchPos);
                                        if (l1 != l2) {
                                            int c1 = Sci(SCI_GETLINEINDENTATION, l1);
                                            int c2 = Sci(SCI_GETLINEINDENTATION, l2);
                                            int gc = c1 < c2 ? c1 : c2;
                                            int tw = Sci(SCI_GETTABWIDTH);
                                            if (tw > 0) gc = (gc / tw) * tw;
                                            Sci(SCI_SETHIGHLIGHTGUIDE, gc);
                                        } else {
                                            Sci(SCI_SETHIGHLIGHTGUIDE, 0);
                                        }
                                        return;
                                    }
                                    depth--;
                                }
                            } else {
                                // Check if it's self-closing
                                int thisEnd = iNameEnd;
                                while (thisEnd < docLen && getChar(thisEnd) != '>') thisEnd++;
                                bool selfClose = (thisEnd < docLen && thisEnd > 0 && getChar(thisEnd - 1) == '/');
                                if (!selfClose) depth++;
                            }
                        }
                    }
                }
            }
            searchPos++;
        }
    }

    // No match found
    Sci(SCI_BRACEHIGHLIGHT, INVALID_POSITION, INVALID_POSITION);
    Sci(SCI_SETHIGHLIGHTGUIDE, 0);
}

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
    if (msg == WM_MOUSEMOVE && zenMode) {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        ClientToScreen(hwnd, &pt);
        ScreenToClient(hwndMain, &pt);
        RECT rc; GetClientRect(hwndMain, &rc);
        
        if (!zenTopVisible && pt.y <= 5) TriggerZenTopAnimation(hwndMain, true);
        else if (zenTopVisible && pt.y > 75) TriggerZenTopAnimation(hwndMain, false);
        
        if (!zenBottomVisible && pt.y >= rc.bottom - 5) TriggerZenBottomAnimation(hwndMain, true);
        else if (zenBottomVisible && pt.y < rc.bottom - 45) TriggerZenBottomAnimation(hwndMain, false);
    }
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

void RunCurrentFile(HWND hwnd, bool runAsAdmin) {
    if (activeTabIndex >= tabs.size()) return;

    Tab& tab = tabs[activeTabIndex];

    if (tab.filePath.empty()) {
        ShowCustomMessageBox(hwnd, L"This file must be saved to disk before it can be run.", L"Run File", MB_OK);
        return;
    }

    if (tab.isModified) {
        DoFileSave(hwnd);
    }

    std::wstring dir = tab.filePath.substr(0, tab.filePath.find_last_of(L"\\/"));

    const wchar_t* verb = runAsAdmin ? L"runas" : L"open";

    HINSTANCE result = ShellExecuteW(NULL, verb, tab.filePath.c_str(), NULL, dir.c_str(), SW_SHOWNORMAL);
    intptr_t resCode = reinterpret_cast<intptr_t>(result);
    if (resCode <= 32) {
        if (runAsAdmin && (resCode == SE_ERR_ACCESSDENIED || resCode == 1223)) {
            // User cancelled UAC prompt
            return;
        }
        ShowCustomMessageBox(hwnd, L"Failed to open this file. No application is associated with this file type.", L"Run Error", MB_OK);
    }
}

void CleanupUserChoiceAssociations() {
    const wchar_t* extensions[] = {
        L".txt", L".log", L".md", L".markdown", L".json", L".jsonc", L".xml", L".html", L".htm", L".css", L".scss",
        L".ini", L".cfg", L".conf", L".config", L".env", L".yaml", L".yml", L".toml", L".c", L".cpp", L".cc", L".h",
        L".hpp", L".cs", L".java", L".js", L".jsx", L".ts", L".tsx", L".py", L".rb", L".go", L".rs", L".sql", L".ahk",
        L".rc", L".bat", L".cmd", L".ps1", L".sh", L".iss"
    };

    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of(L'\\');
    if (lastSlash != std::wstring::npos) exeDir = exeDir.substr(0, lastSlash + 1);

    {
        std::wstring appProgIdKey = L"Software\\Classes\\Applications\\Velo.exe";
        std::wstring pageIcon = exeDir + L"icon\\papirus\\_page.ico";
        bool needsWrite = true;

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
        std::wstring progId = std::wstring(L"Velo") + ext;
        std::wstring keyPath = std::wstring(L"Software\\Classes\\") + progId;

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
        else if (wcscmp(ext, L".js") == 0 || wcscmp(ext, L".jsx") == 0)
            iconFile = L"icon\\papirus\\js.ico";
        else if (wcscmp(ext, L".ts") == 0 || wcscmp(ext, L".tsx") == 0)
            iconFile = L"icon\\papirus\\ts.ico";
        else if (wcscmp(ext, L".c") == 0 || wcscmp(ext, L".h") == 0)
            iconFile = L"icon\\papirus\\c.ico";
        else if (wcscmp(ext, L".cpp") == 0 || wcscmp(ext, L".cc") == 0 || wcscmp(ext, L".hpp") == 0)
            iconFile = L"icon\\papirus\\cpp.ico";
        else if (wcscmp(ext, L".cs") == 0)
            iconFile = L"icon\\papirus\\cs.ico";
        else if (wcscmp(ext, L".java") == 0)
            iconFile = L"icon\\papirus\\java.ico";
        else if (wcscmp(ext, L".html") == 0 || wcscmp(ext, L".htm") == 0)
            iconFile = L"icon\\papirus\\html.ico";
        else if (wcscmp(ext, L".css") == 0 || wcscmp(ext, L".scss") == 0)
            iconFile = L"icon\\papirus\\css.ico";
        else if (wcscmp(ext, L".xml") == 0)
            iconFile = L"icon\\papirus\\xml.ico";
        else if (wcscmp(ext, L".sql") == 0)
            iconFile = L"icon\\papirus\\sql.ico";
        else if (wcscmp(ext, L".ahk") == 0)
            iconFile = L"icon\\papirus\\ahk.ico";
        else if (wcscmp(ext, L".sh") == 0 || wcscmp(ext, L".bash") == 0)
            iconFile = L"icon\\papirus\\sh.ico";
        else if (wcscmp(ext, L".ps1") == 0)
            iconFile = L"icon\\papirus\\ps1.ico";
        else if (wcscmp(ext, L".bat") == 0 || wcscmp(ext, L".cmd") == 0)
            iconFile = L"icon\\papirus\\bat.ico";
        else
            iconFile = L"icon\\papirus\\_page.ico";

        std::wstring fullIconPath = exeDir + iconFile;

        std::wstring rawExtName = (ext[0] == L'.') ? std::wstring(ext + 1) : std::wstring(ext);
        for (auto& ch : rawExtName) ch = towupper(ch);
        std::wstring progIdFriendlyName = rawExtName + L" Document";

        HKEY hProgId;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hProgId, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hProgId, NULL, 0, REG_SZ, (const BYTE*)progIdFriendlyName.c_str(), (DWORD)((progIdFriendlyName.length() + 1) * sizeof(wchar_t)));

            HKEY hIcon;
            if (RegCreateKeyExW(hProgId, L"DefaultIcon", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hIcon, NULL) == ERROR_SUCCESS) {
                RegSetValueExW(hIcon, NULL, 0, REG_SZ, (const BYTE*)fullIconPath.c_str(), (DWORD)((fullIconPath.length() + 1) * sizeof(wchar_t)));
                RegCloseKey(hIcon);
            }

            HKEY hCmd;
            if (RegCreateKeyExW(hProgId, L"shell\\open\\command", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hCmd, NULL) == ERROR_SUCCESS) {
                std::wstring cmd = L"\"" + std::wstring(exePath) + L"\" \"%1\"";
                RegSetValueExW(hCmd, NULL, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
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
                    
                    std::wstring classesKey = std::wstring(L"Software\\Classes\\") + ext;
                    HKEY hClasses;
                    if (RegCreateKeyExW(HKEY_CURRENT_USER, classesKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hClasses, NULL) == ERROR_SUCCESS) {
                        RegSetValueExW(hClasses, NULL, 0, REG_SZ, (const BYTE*)progId.c_str(), (DWORD)((progId.length() + 1) * sizeof(wchar_t)));
                        RegCloseKey(hClasses);
                    }
                    
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

                if (isAppProgId) {
                    std::wstring extStr(extBuf);
                    std::wstring perExtProgId = L"Velo" + extStr;
                    std::wstring perExtProgIdKey = std::wstring(L"Software\\Classes\\") + perExtProgId;

                    std::wstring pageIconPath2 = exeDir + L"icon\\papirus\\_page.ico";

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
                                std::wstring cmd = L"\"" + std::wstring(exePath) + L"\" \"%1\"";
                                RegSetValueExW(hCmd, NULL, 0, REG_SZ, (const BYTE*)cmd.c_str(),
                                              (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
                                RegCloseKey(hCmd);
                            }
                            RegCloseKey(hPerExt);
                        }

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

                std::wstring progIdKey = std::wstring(L"Software\\Classes\\") + progIdBuf;
                HKEY hProgId;

                std::wstring rawExt(progIdBuf + 4);
                if (!rawExt.empty() && rawExt[0] == L'.') rawExt = rawExt.substr(1);
                for (auto& ch : rawExt) ch = towupper(ch);
                std::wstring displayName = rawExt + L" Document";

                if (RegOpenKeyExW(HKEY_CURRENT_USER, progIdKey.c_str(), 0, KEY_READ, &hProgId) == ERROR_SUCCESS) {
                    HKEY hIcon = NULL;
                    bool hasIcon = (RegOpenKeyExW(hProgId, L"DefaultIcon", 0, KEY_READ, &hIcon) == ERROR_SUCCESS);
                    bool needsIconFix = false;
                    if (hasIcon) {
                        wchar_t iconVal[MAX_PATH] = {0};
                        DWORD iconValSize = sizeof(iconVal);
                        DWORD iconType = 0;
                        if (RegQueryValueExW(hIcon, NULL, NULL, &iconType, (LPBYTE)iconVal, &iconValSize) == ERROR_SUCCESS) {
                            std::wstring iconStr(iconVal);
                            if (iconStr.find(L"Velo.exe") != std::wstring::npos ||
                                iconStr.find(L"velo.exe") != std::wstring::npos) {
                                needsIconFix = true;
                            }
                        }
                        RegCloseKey(hIcon);
                    }
                    RegCloseKey(hProgId);

                    if (!hasIcon || needsIconFix) {
                        if (RegCreateKeyExW(HKEY_CURRENT_USER, progIdKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
                                            KEY_WRITE, NULL, &hProgId, NULL) == ERROR_SUCCESS) {
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
                            std::wstring cmd = L"\"" + std::wstring(exePath) + L"\" \"%1\"";
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
}
