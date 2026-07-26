#include "../globals.h"
#include "editor.h"
#include "ui_drawing.h"
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
