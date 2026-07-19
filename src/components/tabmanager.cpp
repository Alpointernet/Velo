#include "../globals.h"
#include "tabmanager.h"
#include "editor.h"
#include "ui_drawing.h"
#include "dialogs.h"
#include <shlobj.h>

std::wstring GetFileName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

int GetTabWidth(size_t index) {
    if (index >= tabs.size()) return 0;
    
    auto GetTabNaturalWidth = [](size_t idx) {
        if (idx >= tabs.size()) return 0;
        int textW = (int)tabs[idx].title.length() * 8;
        return max(80, min(180, textW + 45));
    };
    
    int naturalW = GetTabNaturalWidth(index);
    
    RECT rcMain; GetClientRect(hwndMain, &rcMain);
    RECT pad = GetPad(hwndMain);
    int availableW = rcMain.right - pad.left - pad.right - 70 - 135 - 30;
    if (availableW < 100) availableW = 100;
    
    int totalNaturalW = 0;
    for (size_t i = 0; i < tabs.size(); ++i) {
        totalNaturalW += GetTabNaturalWidth(i);
    }
    
    if (totalNaturalW <= availableW) {
        return naturalW;
    }
    
    double scale = (double)availableW / totalNaturalW;
    int scaledW = (int)(naturalW * scale);
    if (scaledW < 45) scaledW = 45;
    return scaledW;
}

void SwitchToTab(HWND h, size_t idx) {
    if (idx >= tabs.size()) return;
    if (autoSaveOnSwitch && activeTabIndex < tabs.size() && Sci(SCI_GETMODIFY)) {
        DoFileSave(h);
    }
    activeTabIndex = idx; Sci(SCI_SETDOCPOINTER, 0, tabs[activeTabIndex].docPointer);
    activeLineStart = -1; activeLineEnd = -1; ApplySyntax(); SyncLineNumbers(true);
    RecalculateScrollWidth();
    UpdateUI(h);
    SetFocus(hwndScintilla);
    SaveSession();
}

void CreateNewTab(HWND h, std::wstring path) {
    tabs.push_back({ path, path.empty() ? L"Untitled" : GetFileName(path), Sci(SCI_CREATEDOCUMENT), false });
    SwitchToTab(h, tabs.size() - 1); Sci(SCI_EMPTYUNDOBUFFER);
    SaveSession();
}

void CloseTab(HWND h, size_t idx) {
    if (idx >= tabs.size()) return;
    size_t oldActive = activeTabIndex; SwitchToTab(h, idx);
    if (Sci(SCI_GETMODIFY)) {
        int res = ShowCustomMessageBox(h, L"Save changes to " + tabs[idx].title + L"?", L"Unsaved Changes", MB_YESNOCANCEL);
        if (res == IDYES) DoFileSave(h);
        else if (res == IDCANCEL) { SwitchToTab(h, oldActive); return; }
    }
    if (tabs.size() == 1) {
        Sci(SCI_CLEARALL); Sci(SCI_SETSAVEPOINT); Sci(SCI_EMPTYUNDOBUFFER);
        tabs[0] = { L"", L"Untitled", tabs[0].docPointer, false };
        activeLineStart = -1; activeLineEnd = -1; ApplySyntax(); SyncLineNumbers(true); UpdateUI(h);
        SaveSession();
        return;
    }
    sptr_t docToRelease = tabs[idx].docPointer;
    size_t newActive = (idx == oldActive) ? ((idx == tabs.size() - 1) ? idx - 1 : idx + 1) : oldActive;
    if (idx == oldActive) Sci(SCI_SETDOCPOINTER, 0, tabs[newActive].docPointer);
    Sci(SCI_RELEASEDOCUMENT, 0, docToRelease); tabs.erase(tabs.begin() + idx);
    activeTabIndex = (idx < oldActive) ? oldActive - 1 : ((idx == oldActive) ? ((idx == tabs.size()) ? idx - 1 : idx) : oldActive);
    Sci(SCI_SETDOCPOINTER, 0, tabs[activeTabIndex].docPointer);
    activeLineStart = -1; activeLineEnd = -1; ApplySyntax(); SyncLineNumbers(true); UpdateUI(h);
    SaveSession();
}

bool SaveModifiedTabs(HWND h) {
    for (size_t i = 0; i < tabs.size(); ++i) {
        SwitchToTab(h, i);
        if (Sci(SCI_GETMODIFY)) {
            int res = ShowCustomMessageBox(h, L"Save changes to " + tabs[i].title + L"?", L"Unsaved Changes", MB_YESNOCANCEL);
            if (res == IDYES) DoFileSave(h);
            else if (res == IDCANCEL) return false;
        }
    }
    return true;
}

void LoadFileInActiveTab(HWND h, const wchar_t* path) {
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(hFile, NULL), read; std::vector<char> buf(size + 1, 0);
        if (ReadFile(hFile, buf.data(), size, &read, NULL)) {
            Sci(SCI_CLEARALL); Sci(SCI_APPENDTEXT, read, (LPARAM)buf.data());
            Sci(SCI_SETSAVEPOINT); Sci(SCI_EMPTYUNDOBUFFER);
            tabs[activeTabIndex] = { path, GetFileName(path), tabs[activeTabIndex].docPointer, false };
            activeLineStart = -1; activeLineEnd = -1; ApplySyntax(); SyncLineNumbers(true);
            RecalculateScrollWidth();
            UpdateUI(h);
            SaveSession();
        }
        CloseHandle(hFile);
    }
}

void DoFileOpen(HWND h) {
    wchar_t szFile[260] = { 0 };
    OPENFILENAMEW ofn = { sizeof(ofn), h, 0, L"All Files\0*.*\0Text Files\0*.txt\0", 0, 0, 1, szFile, 260, 0, 0, 0, 0, OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST };
    if (GetOpenFileNameW(&ofn)) {
        for (size_t i = 0; i < tabs.size(); ++i) if (!_wcsicmp(tabs[i].filePath.c_str(), ofn.lpstrFile)) { SwitchToTab(h, i); return; }
        if (tabs[activeTabIndex].filePath.empty() && Sci(SCI_GETLENGTH) == 0 && !tabs[activeTabIndex].isModified) LoadFileInActiveTab(h, ofn.lpstrFile);
        else { CreateNewTab(h, ofn.lpstrFile); LoadFileInActiveTab(h, ofn.lpstrFile); }
    }
}

void DoFileSave(HWND h) {
    if (tabs[activeTabIndex].filePath.empty()) { DoFileSaveAs(h); return; }
    HANDLE hFile = CreateFileW(tabs[activeTabIndex].filePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        int len = Sci(SCI_GETLENGTH); std::vector<char> buf(len + 1, 0); Sci(SCI_GETTEXT, len + 1, (LPARAM)buf.data());
        DWORD written; WriteFile(hFile, buf.data(), len, &written, NULL); CloseHandle(hFile);
        Sci(SCI_SETSAVEPOINT); tabs[activeTabIndex].isModified = false; ApplySyntax(); UpdateUI(h);
        SaveSession();
    }
}

void DoFileSaveAs(HWND h) {
    wchar_t szFile[260] = { 0 };
    if (!tabs[activeTabIndex].filePath.empty()) wcscpy_s(szFile, tabs[activeTabIndex].filePath.c_str());
    OPENFILENAMEW ofn = { sizeof(ofn), h, 0, L"All Files\0*.*\0Text Files\0*.txt\0", 0, 0, 1, szFile, 260, 0, 0, 0, 0, OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT };
    if (GetSaveFileNameW(&ofn)) {
        tabs[activeTabIndex].filePath = ofn.lpstrFile; tabs[activeTabIndex].title = GetFileName(ofn.lpstrFile);
        DoFileSave(h);
    }
}

std::wstring GetConfigPath() {
    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::wstring path = std::wstring(appDataPath) + L"\\Velo";
        CreateDirectoryW(path.c_str(), NULL);
        return path + L"\\session.txt";
    }
    return L"";
}

void SaveSession() {
    std::wstring configPath = GetConfigPath();
    if (configPath.empty()) return;
    std::ofstream out(configPath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return;
    
    out << "[Settings]\n";
    out << "fontSize=" << editorFontSize << "\n";
    out << "tabWidth=" << editorTabWidth << "\n";
    out << "autoSaveOnSwitch=" << (autoSaveOnSwitch ? 1 : 0) << "\n";
    out << "autoCloseBraces=" << (autoCloseBraces ? 1 : 0) << "\n";
    out << "showIndentGuides=" << (showIndentGuides ? 1 : 0) << "\n";
    out << "showWhitespace=" << (showWhitespace ? 1 : 0) << "\n";
    out << "caretStyleBlock=" << (caretStyleBlock ? 1 : 0) << "\n";
    out << "activeTab=" << activeTabIndex << "\n";
    
    out << "\n[Tabs]\n";
    std::vector<std::wstring> validPaths;
    for (const auto& tab : tabs) {
        if (!tab.filePath.empty()) {
            validPaths.push_back(tab.filePath);
        }
    }
    out << "count=" << validPaths.size() << "\n";
    for (size_t i = 0; i < validPaths.size(); ++i) {
        std::wstring wpath = validPaths[i];
        int len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, NULL, 0, NULL, NULL);
        if (len > 0) {
            std::vector<char> u8path(len);
            WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, u8path.data(), len, NULL, NULL);
            std::string s(u8path.data());
            out << "tab" << i << "=" << s << "\n";
        }
    }
}

void LoadSession(HWND hwndParent) {
    std::wstring configPath = GetConfigPath();
    if (configPath.empty()) {
        CreateNewTab(hwndParent);
        return;
    }
    std::ifstream in(configPath);
    if (!in.is_open()) {
        CreateNewTab(hwndParent);
        return;
    }
    
    std::string line;
    std::vector<std::wstring> filePathsToOpen;
    int loadedActiveTab = 0;
    
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '[' || line[0] == ';') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        
        try {
            if (key == "fontSize") editorFontSize = std::stoi(val);
            else if (key == "tabWidth") editorTabWidth = std::stoi(val);
            else if (key == "autoSaveOnSwitch") autoSaveOnSwitch = (val == "1");
            else if (key == "autoCloseBraces") autoCloseBraces = (val == "1");
            else if (key == "showIndentGuides") showIndentGuides = (val == "1");
            else if (key == "showWhitespace") showWhitespace = (val == "1");
            else if (key == "caretStyleBlock") caretStyleBlock = (val == "1");
            else if (key == "activeTab") loadedActiveTab = std::stoi(val);
            else if (key.rfind("tab", 0) == 0 && key != "tabWidth") {
                int len = MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, NULL, 0);
                if (len > 0) {
                    std::vector<wchar_t> wval(len);
                    MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, wval.data(), len);
                    std::wstring ws(wval.data());
                    if (!ws.empty()) {
                        filePathsToOpen.push_back(ws);
                    }
                }
            }
        } catch (...) {}
    }
    
    if (filePathsToOpen.empty()) {
        CreateNewTab(hwndParent);
    } else {
        for (const auto& path : filePathsToOpen) {
            DWORD attrib = GetFileAttributesW(path.c_str());
            if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                CreateNewTab(hwndParent, path);
                LoadFileInActiveTab(hwndParent, path.c_str());
            }
        }
        if (tabs.empty()) {
            CreateNewTab(hwndParent);
        } else {
            if (loadedActiveTab >= (int)tabs.size()) {
                loadedActiveTab = (int)tabs.size() - 1;
            }
            if (loadedActiveTab < 0) loadedActiveTab = 0;
            SwitchToTab(hwndParent, loadedActiveTab);
        }
    }
}
