#include "../globals.h"
#include "tabmanager.h"
#include "editor.h"
#include "ui_drawing.h"
#include "dialogs.h"
#include "animations.h"
#include <shlobj.h>

static std::wstring GetBackupsDir();
static std::string GetDocText(sptr_t docPointer);
static void CleanOldBackups(size_t startIdx);
std::wstring GetFileName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

int GetTabWidth(size_t index) {
    if (index >= tabs.size()) return 0;
    
    auto GetTabNaturalWidth = [](size_t idx) -> int {
        if (idx >= tabs.size()) return 0;
        int textW = (int)tabs[idx].title.length() * 8;
        return max(80, min(180, textW + 45));
    };
    
    RECT rcMain; GetClientRect(hwndMain, &rcMain);
    RECT pad = GetPad(hwndMain);
    int availableW = rcMain.right - pad.left - pad.right - 70 - 135 - 30;
    if (availableW < 100) availableW = 100;
    
    double totalNaturalW = 0;
    for (size_t i = 0; i < tabs.size(); ++i) {
        double eff = (tabs[i].isOpening || tabs[i].isClosing) ? (double)tabs[i].animProgress : 1.0;
        totalNaturalW += GetTabNaturalWidth(i) * eff;
    }
    
    int naturalW = GetTabNaturalWidth(index);
    int baseW = naturalW;
    if (totalNaturalW > availableW && totalNaturalW > 0) {
        double scale = (double)availableW / totalNaturalW;
        baseW = (int)(naturalW * scale);
        if (baseW < 45) baseW = 45;
    }
    
    Tab& t = tabs[index];
    if (t.isOpening || t.isClosing) {
        int animatedW = (int)(baseW * t.animProgress);
        return max(0, animatedW);
    }
    
    return baseW;
}

void SwitchToTab(HWND h, size_t idx) {
    if (idx >= tabs.size()) return;
    if (tabs[idx].isClosing) return;
    
    // Save current active tab modified state and EOL mode
    if (activeTabIndex < tabs.size() && tabs[activeTabIndex].isLoaded) {
        tabs[activeTabIndex].isModified = (Sci(SCI_GETMODIFY) != 0);
        tabs[activeTabIndex].eolMode = (int)Sci(SCI_GETEOLMODE);
    }
    
    activeTabIndex = idx; 
    Sci(SCI_SETDOCPOINTER, 0, tabs[activeTabIndex].docPointer);
    
    // Lazy load the tab content if not loaded yet!
    if (!tabs[activeTabIndex].isLoaded) {
        bool loaded = false;
        std::wstring backup = tabs[activeTabIndex].backupFile;
        std::wstring path = tabs[activeTabIndex].filePath;
        std::vector<char> diskBuf;
        bool diskLoaded = false;
        if (!path.empty()) {
            HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD size = GetFileSize(hFile, NULL), read;
                std::vector<char> buf(size + 1, 0);
                if (ReadFile(hFile, buf.data(), size, &read, NULL)) {
                    diskBuf.assign(buf.begin(), buf.begin() + read);
                    diskLoaded = true;
                }
                CloseHandle(hFile);
            }
        }
        
        Sci(SCI_CLEARALL);
        if (diskLoaded) {
            Sci(SCI_APPENDTEXT, diskBuf.size(), (LPARAM)diskBuf.data());
        }
        Sci(SCI_SETSAVEPOINT);
        Sci(SCI_EMPTYUNDOBUFFER);
        
        // 2. If a backup file exists, replace the baseline content with the backup content, creating an undoable state.
        bool backupLoaded = false;
        if (!backup.empty()) {
            std::wstring backupsDir = GetBackupsDir();
            if (!backupsDir.empty()) {
                std::wstring backupPath = backupsDir + L"\\" + backup;
                HANDLE hFile = CreateFileW(backupPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD size = GetFileSize(hFile, NULL), read;
                    std::vector<char> buf(size + 1, 0);
                    if (ReadFile(hFile, buf.data(), size, &read, NULL)) {
                        Sci(SCI_SETSEL, 0, Sci(SCI_GETLENGTH));
                        Sci(SCI_REPLACESEL, 0, (LPARAM)buf.data());
                        Sci(SCI_SETSEL, 0, 0); // Reset selection/cursor
                        backupLoaded = true;
                        loaded = true;
                    }
                    CloseHandle(hFile);
                }
            }
        }
        
        if (diskLoaded && !backupLoaded) {
            loaded = true;
        }
        
        if (!loaded) {
            Sci(SCI_CLEARALL);
            Sci(SCI_SETSAVEPOINT);
            Sci(SCI_EMPTYUNDOBUFFER);
        }
        
        tabs[activeTabIndex].isLoaded = true;
        Sci(SCI_SETEOLMODE, tabs[activeTabIndex].eolMode);
        tabs[activeTabIndex].isModified = (Sci(SCI_GETMODIFY) != 0);
    }
    
    activeLineStart = -1; activeLineEnd = -1; ApplySyntax(); SyncLineNumbers(true);
    RecalculateScrollWidth();
    if (searchVisible) UpdateSearchMatches();
    UpdateUI(h);
    SetFocus(hwndScintilla);
    SaveSession();
}

void CreateNewTab(HWND h, std::wstring path, bool animate) {
    bool doAnimate = animate && enableAnimations;
    Tab newTab = { path, path.empty() ? L"Untitled" : GetFileName(path), Sci(SCI_CREATEDOCUMENT), false, L"", true, 0, GetFileLastWriteTime(path) };
    if (doAnimate) {
        newTab.isOpening = true;
        newTab.isClosing = false;
        QueryPerformanceCounter(&newTab.animStartQPC);
        newTab.animProgress = 0.0f;
    } else {
        newTab.isOpening = false;
        newTab.isClosing = false;
        newTab.animProgress = 1.0f;
    }
    tabs.push_back(newTab);
    SwitchToTab(h, tabs.size() - 1); Sci(SCI_EMPTYUNDOBUFFER);
    
    if (doAnimate) {
        timeBeginPeriod(1);
        SetTimer(h, 5, 1, NULL);
    }
    
    SaveSession();
}

bool CloseTab(HWND h, size_t idx) {
    if (idx >= tabs.size()) return false;
    if (tabs[idx].isClosing) return false;
    
    int activeCount = 0;
    for (const auto& t : tabs) {
        if (!t.isClosing) activeCount++;
    }
    
    size_t oldActive = activeTabIndex;
    bool isMod = tabs[idx].isModified || (idx == activeTabIndex && Sci(SCI_GETMODIFY) != 0);
    if (isMod && !tabs[idx].isClosing) {
        SwitchToTab(h, idx);
        int res = ShowCustomMessageBox(h, L"Save changes to " + tabs[idx].title + L"?", L"Unsaved Changes", MB_YESNOCANCEL);
        if (res == IDYES) {
            DoFileSave(h);
            if (tabs[idx].isModified || Sci(SCI_GETMODIFY) != 0) {
                SwitchToTab(h, oldActive);
                return false;
            }
        }
        else if (res == IDCANCEL) {
            SwitchToTab(h, oldActive);
            return false;
        }
        else if (res == IDNO) {
            tabs[idx].isModified = false;
            if (idx == activeTabIndex) {
                Sci(SCI_SETSAVEPOINT);
            }
            if (tabs[idx].filePath.empty()) {
                if (idx == activeTabIndex) {
                    Sci(SCI_CLEARALL);
                    Sci(SCI_SETSAVEPOINT);
                }
                tabs[idx].title = L"Untitled";
            }
            std::wstring backupsDir = GetBackupsDir();
            if (!backupsDir.empty()) {
                std::wstring backupPath = backupsDir + L"\\backup_" + std::to_wstring(idx) + L".txt";
                DeleteFileW(backupPath.c_str());
            }
        }
    }
    
    if (activeCount <= 1) {
        Sci(SCI_CLEARALL); Sci(SCI_SETSAVEPOINT); Sci(SCI_EMPTYUNDOBUFFER);
        tabs[idx] = { L"", L"Untitled", tabs[idx].docPointer, false, L"", true, 0, GetFileLastWriteTime(L"") };
        tabs[idx].isOpening = false; tabs[idx].isClosing = false; tabs[idx].animProgress = 1.0f;
        activeLineStart = -1; activeLineEnd = -1; ApplySyntax(); SyncLineNumbers(true); UpdateUI(h);
        std::wstring backupsDir = GetBackupsDir();
        if (!backupsDir.empty()) {
            std::wstring backupPath = backupsDir + L"\\backup_" + std::to_wstring(idx) + L".txt";
            DeleteFileW(backupPath.c_str());
        }
        SaveSession();
        return true;
    }
    
    if (idx == activeTabIndex) {
        int nextActive = -1;
        for (size_t i = idx + 1; i < tabs.size(); ++i) {
            if (!tabs[i].isClosing) { nextActive = (int)i; break; }
        }
        if (nextActive < 0) {
            for (int i = (int)idx - 1; i >= 0; --i) {
                if (!tabs[i].isClosing) { nextActive = i; break; }
            }
        }
        if (nextActive >= 0) {
            SwitchToTab(h, (size_t)nextActive);
        }
    }
    
    if (!enableAnimations) {
        sptr_t docToRelease = tabs[idx].docPointer;
        tabs.erase(tabs.begin() + idx);
        if (activeTabIndex > idx) {
            activeTabIndex--;
        } else if (activeTabIndex >= tabs.size() && !tabs.empty()) {
            activeTabIndex = tabs.size() - 1;
        }
        Sci(SCI_SETDOCPOINTER, 0, tabs[activeTabIndex].docPointer);
        Sci(SCI_RELEASEDOCUMENT, 0, docToRelease);
        activeLineStart = -1; activeLineEnd = -1; ApplySyntax(); SyncLineNumbers(true); UpdateUI(h);
        SaveSession();
        return true;
    }

    tabs[idx].isClosing = true;
    tabs[idx].isOpening = false;
    QueryPerformanceCounter(&tabs[idx].animStartQPC);
    tabs[idx].animProgress = 1.0f;
    
    timeBeginPeriod(1);
    SetTimer(h, 5, 1, NULL);
    
    SaveSession();
    return true;
}

bool CloseAllTabs(HWND h) {
    for (int i = (int)tabs.size() - 1; i >= 0; --i) {
        if (i >= (int)tabs.size()) continue;
        if (tabs[i].isClosing) continue;
        if (!CloseTab(h, i)) {
            return false;
        }
    }
    return true;
}

bool SaveModifiedTabs(HWND h) {
    for (size_t i = 0; i < tabs.size(); ++i) {
        if (tabs[i].isClosing) continue;
        if (tabs[i].isModified || (i == activeTabIndex && Sci(SCI_GETMODIFY))) {
            SwitchToTab(h, i);
            int res = ShowCustomMessageBox(h, L"Save changes to " + tabs[i].title + L"?", L"Unsaved Changes", MB_YESNOCANCEL);
            if (res == IDYES) {
                DoFileSave(h);
                if (tabs[i].isModified || Sci(SCI_GETMODIFY)) return false;
            }
            else if (res == IDCANCEL) return false;
            else if (res == IDNO) {
                tabs[i].isModified = false;
                if (i == activeTabIndex) {
                    Sci(SCI_SETSAVEPOINT);
                }
                if (tabs[i].filePath.empty()) {
                    if (i == activeTabIndex) {
                        Sci(SCI_CLEARALL);
                        Sci(SCI_SETSAVEPOINT);
                    }
                    tabs[i].title = L"Untitled";
                }
                std::wstring backupsDir = GetBackupsDir();
                if (!backupsDir.empty()) {
                    std::wstring backupPath = backupsDir + L"\\backup_" + std::to_wstring(i) + L".txt";
                    DeleteFileW(backupPath.c_str());
                }
            }
        }
    }
    return true;
}

FILETIME GetFileLastWriteTime(const std::wstring& path) {
    FILETIME ft = {0, 0};
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        GetFileTime(hFile, NULL, NULL, &ft);
        CloseHandle(hFile);
    }
    return ft;
}

void LoadFileInActiveTab(HWND h, const wchar_t* path) {
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(hFile, NULL), read; std::vector<char> buf(size + 1, 0);
        if (ReadFile(hFile, buf.data(), size, &read, NULL)) {
            Sci(SCI_CLEARALL); Sci(SCI_APPENDTEXT, read, (LPARAM)buf.data());
            Sci(SCI_SETSAVEPOINT); Sci(SCI_EMPTYUNDOBUFFER);
            tabs[activeTabIndex] = { path, GetFileName(path), tabs[activeTabIndex].docPointer, false, L"", true, 0, GetFileLastWriteTime(path) };
            activeLineStart = -1; activeLineEnd = -1; ApplySyntax(); SyncLineNumbers(true);
            RecalculateScrollWidth();
            if (searchVisible) UpdateSearchMatches();
            UpdateUI(h);
            SaveSession();
        }
        CloseHandle(hFile);
    } else {
        tabs[activeTabIndex] = { path, GetFileName(path), tabs[activeTabIndex].docPointer, false, L"", true, 0, GetFileLastWriteTime(path) };
        Sci(SCI_CLEARALL); Sci(SCI_SETSAVEPOINT); Sci(SCI_EMPTYUNDOBUFFER);
        activeLineStart = -1; activeLineEnd = -1; ApplySyntax(); SyncLineNumbers(true);
        UpdateUI(h);
        SaveSession();
    }
}

void DoFileOpen(HWND h) {
    wchar_t szFile[260] = { 0 };
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = h;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = 260;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;

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
        tabs[activeTabIndex].lastWriteTime = GetFileLastWriteTime(tabs[activeTabIndex].filePath);
        Sci(SCI_SETSAVEPOINT); tabs[activeTabIndex].isModified = false; ApplySyntax(); UpdateUI(h);
        SaveSession();
    }
}

void DoFileSaveAs(HWND h) {
    ReleaseCapture();

    wchar_t szFile[260] = { 0 };
    if (!tabs[activeTabIndex].filePath.empty()) {
        wcscpy_s(szFile, tabs[activeTabIndex].filePath.c_str());
    } else {
        wcscpy_s(szFile, L"Untitled.txt");
    }

    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = h;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = 260;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"txt";

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

static std::string GetDocText(sptr_t docPointer) {
    sptr_t oldDoc = Sci(SCI_GETDOCPOINTER);
    Sci(SCI_SETDOCPOINTER, 0, docPointer);
    int len = Sci(SCI_GETLENGTH);
    std::vector<char> buf(len + 1, 0);
    Sci(SCI_GETTEXT, len + 1, (LPARAM)buf.data());
    Sci(SCI_SETDOCPOINTER, 0, oldDoc);
    return std::string(buf.data(), len);
}

static std::wstring GetBackupsDir() {
    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::wstring path = std::wstring(appDataPath) + L"\\Velo\\backups";
        CreateDirectoryW(path.c_str(), NULL);
        return path;
    }
    return L"";
}

static void CleanOldBackups(size_t startIdx) {
    std::wstring backupsDir = GetBackupsDir();
    if (backupsDir.empty()) return;
    int idx = (int)startIdx;
    while (true) {
        std::wstring backupPath = backupsDir + L"\\backup_" + std::to_wstring(idx) + L".txt";
        DWORD attrib = GetFileAttributesW(backupPath.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            DeleteFileW(backupPath.c_str());
            idx++;
        } else {
            break;
        }
    }
}

void SaveSession() {
    isSavingSession = true;
    std::wstring configPath = GetConfigPath();
    if (configPath.empty()) { isSavingSession = false; return; }
    std::ofstream out(configPath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) { isSavingSession = false; return; }
    
    std::vector<int> validTabs;
    int newActiveTab = -1;
    for (int i = 0; i < (int)tabs.size(); ++i) {
        bool isModified = false;
        if (i == activeTabIndex) {
            isModified = (Sci(SCI_GETMODIFY) != 0);
        } else {
            isModified = tabs[i].isModified;
        }
        
        bool isUntitled = tabs[i].filePath.empty();
        int docLen = (i == activeTabIndex) ? Sci(SCI_GETLENGTH) : 0;
        
        // Filter out completely blank untitled tabs
        if (isUntitled && !isModified) {
            // Wait, what if the tab is inactive, we don't check docLen but !isModified guarantees it's untouched.
            continue;
        }
        
        if (i == activeTabIndex) {
            newActiveTab = (int)validTabs.size();
        }
        validTabs.push_back(i);
    }
    
    if (newActiveTab == -1 && !validTabs.empty()) {
        newActiveTab = 0;
    } else if (validTabs.empty()) {
        newActiveTab = 0;
    }
    
    out << "[Settings]\n";
    out << "fontSize=" << editorFontSize << "\n";
    out << "tabWidth=" << editorTabWidth << "\n";
    out << "autoSaveOnSwitch=" << (autoSaveOnSwitch ? 1 : 0) << "\n";
    out << "autoCloseBraces=" << (autoCloseBraces ? 1 : 0) << "\n";
    out << "showIndentGuides=" << (showIndentGuides ? 1 : 0) << "\n";
    out << "showWhitespace=" << (showWhitespace ? 1 : 0) << "\n";
    out << "caretStyleBlock=" << (caretStyleBlock ? 1 : 0) << "\n";
    out << "showTopBar=" << (showTopBar ? 1 : 0) << "\n";
    out << "enableAnimations=" << (enableAnimations ? 1 : 0) << "\n";
    out << "activeTab=" << newActiveTab << "\n";
    
    out << "\n[Tabs]\n";
    out << "count=" << validTabs.size() << "\n";
    for (size_t outIdx = 0; outIdx < validTabs.size(); ++outIdx) {
        int i = validTabs[outIdx];
        
        std::wstring wpath = tabs[i].filePath;
        std::string sPath = "";
        if (!wpath.empty()) {
            int len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, NULL, 0, NULL, NULL);
            if (len > 0) {
                std::vector<char> u8path(len);
                WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, u8path.data(), len, NULL, NULL);
                sPath = u8path.data();
            }
        }
        
        std::wstring wtitle = tabs[i].title;
        std::string sTitle = "Untitled";
        int lenT = WideCharToMultiByte(CP_UTF8, 0, wtitle.c_str(), -1, NULL, 0, NULL, NULL);
        if (lenT > 0) {
            std::vector<char> u8title(lenT);
            WideCharToMultiByte(CP_UTF8, 0, wtitle.c_str(), -1, u8title.data(), lenT, NULL, NULL);
            sTitle = u8title.data();
        }
        
        bool isModified = false;
        if (i == activeTabIndex) {
            isModified = (Sci(SCI_GETMODIFY) != 0);
        } else {
            isModified = tabs[i].isModified;
        }
        
        std::string sBackup = "";
        if (isModified) {
            if (tabs[i].isLoaded) {
                std::string text = GetDocText(tabs[i].docPointer);
                std::wstring backupsDir = GetBackupsDir();
                if (!backupsDir.empty()) {
                    std::wstring backupPath = backupsDir + L"\\backup_" + std::to_wstring(i) + L".txt";
                    std::ofstream backupOut(backupPath, std::ios::out | std::ios::trunc | std::ios::binary);
                    if (backupOut.is_open()) {
                        backupOut.write(text.data(), text.size());
                        sBackup = "backup_" + std::to_string(i) + ".txt";
                    }
                }
            } else {
                int lenB = WideCharToMultiByte(CP_UTF8, 0, tabs[i].backupFile.c_str(), -1, NULL, 0, NULL, NULL);
                if (lenB > 0) {
                    std::vector<char> u8backup(lenB);
                    WideCharToMultiByte(CP_UTF8, 0, tabs[i].backupFile.c_str(), -1, u8backup.data(), lenB, NULL, NULL);
                    sBackup = u8backup.data();
                }
            }
        } else {
            std::wstring backupsDir = GetBackupsDir();
            if (!backupsDir.empty()) {
                std::wstring backupPath = backupsDir + L"\\backup_" + std::to_wstring(i) + L".txt";
                DeleteFileW(backupPath.c_str());
            }
        }
        
        int eolMode = (i == activeTabIndex) ? (int)Sci(SCI_GETEOLMODE) : tabs[i].eolMode;
        out << "tab_path_" << outIdx << "=" << sPath << "\n";
        out << "tab_title_" << outIdx << "=" << sTitle << "\n";
        out << "tab_modified_" << outIdx << "=" << (isModified ? 1 : 0) << "\n";
        out << "tab_backup_" << outIdx << "=" << sBackup << "\n";
        out << "tab_eol_" << outIdx << "=" << eolMode << "\n";
    }
    
    CleanOldBackups(tabs.size());
    isSavingSession = false;
}

void LoadSession(HWND hwndParent) {
    isSavingSession = true;
    std::wstring configPath = GetConfigPath();
    if (configPath.empty()) {
        isSavingSession = false;
        CreateNewTab(hwndParent);
        return;
    }
    std::ifstream in(configPath);
    if (!in.is_open()) {
        isSavingSession = false;
        CreateNewTab(hwndParent);
        return;
    }
    
    std::string line;
    int tabCount = 0;
    std::vector<std::wstring> tabPaths;
    std::vector<std::wstring> tabTitles;
    std::vector<bool> tabModifieds;
    std::vector<std::wstring> tabBackups;
    std::vector<int> tabEols;
    std::vector<std::wstring> oldPaths;
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
            else if (key == "showTopBar") showTopBar = (val == "1");
            else if (key == "enableAnimations" || key == "enableTabAnimations") enableAnimations = (val == "1");
            else if (key == "activeTab") loadedActiveTab = std::stoi(val);
            else if (key == "count") {
                tabCount = std::stoi(val);
                tabPaths.resize(tabCount);
                tabTitles.resize(tabCount);
                tabModifieds.resize(tabCount, false);
                tabBackups.resize(tabCount);
                tabEols.resize(tabCount, 0);
            }
            else if (key.rfind("tab_path_", 0) == 0) {
                int idx = std::stoi(key.substr(9));
                if (idx >= 0 && idx < tabCount) {
                    int len = MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, NULL, 0);
                    if (len > 0) {
                        std::vector<wchar_t> wval(len);
                        MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, wval.data(), len);
                        tabPaths[idx] = std::wstring(wval.data());
                    }
                }
            }
            else if (key.rfind("tab_title_", 0) == 0) {
                int idx = std::stoi(key.substr(10));
                if (idx >= 0 && idx < tabCount) {
                    int len = MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, NULL, 0);
                    if (len > 0) {
                        std::vector<wchar_t> wval(len);
                        MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, wval.data(), len);
                        tabTitles[idx] = std::wstring(wval.data());
                    }
                }
            }
            else if (key.rfind("tab_modified_", 0) == 0) {
                int idx = std::stoi(key.substr(13));
                if (idx >= 0 && idx < tabCount) {
                    tabModifieds[idx] = (val == "1");
                }
            }
            else if (key.rfind("tab_backup_", 0) == 0) {
                int idx = std::stoi(key.substr(11));
                if (idx >= 0 && idx < tabCount) {
                    int len = MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, NULL, 0);
                    if (len > 0) {
                        std::vector<wchar_t> wval(len);
                        MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, wval.data(), len);
                        tabBackups[idx] = std::wstring(wval.data());
                    }
                }
            }
            else if (key.rfind("tab_eol_", 0) == 0) {
                int idx = std::stoi(key.substr(8));
                if (idx >= 0 && idx < tabCount) {
                    tabEols[idx] = std::stoi(val);
                }
            }
            else if (key.rfind("tab", 0) == 0 && key != "tabWidth") {
                int len = MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, NULL, 0);
                if (len > 0) {
                    std::vector<wchar_t> wval(len);
                    MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, wval.data(), len);
                    std::wstring ws(wval.data());
                    if (!ws.empty()) {
                        oldPaths.push_back(ws);
                    }
                }
            }
        } catch (...) {}
    }
    
    if (tabCount > 0 && (tabPaths.empty() || tabPaths[0].empty()) && !oldPaths.empty()) {
        tabCount = (int)oldPaths.size();
        tabPaths = oldPaths;
        tabTitles.resize(tabCount);
        for (int i = 0; i < tabCount; ++i) {
            tabTitles[i] = GetFileName(tabPaths[i]);
        }
        tabModifieds.assign(tabCount, false);
        tabBackups.assign(tabCount, L"");
        tabEols.assign(tabCount, 0);
    }
    
    if (tabCount > 0) {
        for (int i = 0; i < tabCount; ++i) {
            std::wstring path = tabPaths[i];
            std::wstring title = tabTitles[i];
            sptr_t doc = Sci(SCI_CREATEDOCUMENT);
            std::wstring backup = tabBackups[i];
            bool modified = tabModifieds[i] || !backup.empty();
            int eol = tabEols[i];
            tabs.push_back({ path, title, doc, modified, backup, false, eol, GetFileLastWriteTime(path) });
        }
        
        isSavingSession = false;
        if (tabs.empty()) {
            CreateNewTab(hwndParent, L"", false);
        } else {
            if (loadedActiveTab >= (int)tabs.size()) {
                loadedActiveTab = (int)tabs.size() - 1;
            }
            if (loadedActiveTab < 0) loadedActiveTab = 0;
            SwitchToTab(hwndParent, loadedActiveTab);
        }
    } else if (!oldPaths.empty()) {
        isSavingSession = false;
        for (const auto& path : oldPaths) {
            DWORD attrib = GetFileAttributesW(path.c_str());
            if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                CreateNewTab(hwndParent, path, false);
                LoadFileInActiveTab(hwndParent, path.c_str());
            }
        }
        if (tabs.empty()) {
            CreateNewTab(hwndParent, L"", false);
        } else {
            if (loadedActiveTab >= (int)tabs.size()) {
                loadedActiveTab = (int)tabs.size() - 1;
            }
            if (loadedActiveTab < 0) loadedActiveTab = 0;
            SwitchToTab(hwndParent, loadedActiveTab);
        }
    } else {
        isSavingSession = false;
        CreateNewTab(hwndParent, L"", false);
    }
}

LRESULT CALLBACK TabRenameEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            if (tabRenameIndex >= 0 && tabRenameIndex < (int)tabs.size()) {
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
                    if (tabRenameIndex == (int)activeTabIndex) StyleScintilla(hwndScintilla);
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
bool TransferTabToWindow(HWND hSource, HWND hTarget, size_t tabIdx) {
    if (tabIdx >= tabs.size() || !IsWindow(hTarget) || hSource == hTarget) return false;
    
    Tab t = tabs[tabIdx];
    
    std::string text = "";
    if (tabIdx == activeTabIndex) {
        int len = (int)Sci(SCI_GETLENGTH);
        if (len > 0) {
            std::vector<char> buf(len + 1, 0);
            Sci(SCI_GETTEXT, len + 1, (LPARAM)buf.data());
            text = buf.data();
        }
    } else {
        if (t.isLoaded) {
            text = GetDocText(t.docPointer);
        } else if (!t.filePath.empty()) {
            HANDLE hFile = CreateFileW(t.filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD size = GetFileSize(hFile, NULL), read;
                std::vector<char> buf(size + 1, 0);
                if (ReadFile(hFile, buf.data(), size, &read, NULL)) {
                    text = std::string(buf.data(), read);
                }
                CloseHandle(hFile);
            }
        }
    }

    size_t totalBytes = sizeof(VeloTabTransferHeader) + text.size() + 1;
    std::vector<char> payload(totalBytes, 0);
    VeloTabTransferHeader* hdr = (VeloTabTransferHeader*)payload.data();
    wcscpy_s(hdr->title, t.title.c_str());
    wcscpy_s(hdr->filePath, t.filePath.c_str());
    hdr->isModified = t.isModified;
    hdr->eolMode = t.eolMode;
    hdr->textSize = (int)text.size();
    if (!text.empty()) {
        memcpy(payload.data() + sizeof(VeloTabTransferHeader), text.data(), text.size());
    }

    COPYDATASTRUCT cds = {};
    cds.dwData = VELO_COPYDATA_TAB_TRANSFER;
    cds.cbData = (DWORD)payload.size();
    cds.lpData = payload.data();
    
    DWORD_PTR dwRes = 0;
    LRESULT lRes = SendMessageTimeoutW(hTarget, WM_COPYDATA, (WPARAM)hSource, (LPARAM)&cds, SMTO_ABORTIFHUNG | SMTO_NORMAL, 3000, &dwRes);
    
    if (lRes != 0 && dwRes == TRUE) {
        if (tabs.size() == 1) {
            PostMessage(hSource, WM_CLOSE, 0, 0);
        } else {
            sptr_t docToRelease = tabs[tabIdx].docPointer;
            tabs.erase(tabs.begin() + tabIdx);
            if (activeTabIndex >= tabs.size()) activeTabIndex = tabs.size() - 1;
            Sci(SCI_SETDOCPOINTER, 0, tabs[activeTabIndex].docPointer);
            if (docToRelease) Sci(SCI_RELEASEDOCUMENT, 0, docToRelease);
            UpdateUI(hSource);
            SaveSession();
        }
        return true;
    }
    return false;
}

bool DetachTabToNewWindow(HWND hSource, size_t tabIdx) {
    if (tabIdx >= tabs.size()) return false;
    
    if (tabs.size() == 1 && tabs[0].filePath.empty() && !tabs[0].isModified && Sci(SCI_GETLENGTH) == 0) {
        return false;
    }
    
    Tab t = tabs[tabIdx];
    std::wstring backupDir = GetBackupsDir();
    if (backupDir.empty()) return false;
    
    std::wstring detachedFile = backupDir + L"\\detached_" + std::to_wstring(GetTickCount64()) + L"_" + std::to_wstring(tabIdx) + L".tmp";
    
    std::string text = "";
    if (tabIdx == activeTabIndex) {
        int len = (int)Sci(SCI_GETLENGTH);
        if (len > 0) {
            std::vector<char> buf(len + 1, 0);
            Sci(SCI_GETTEXT, len + 1, (LPARAM)buf.data());
            text = buf.data();
        }
    } else {
        if (t.isLoaded) {
            text = GetDocText(t.docPointer);
        } else if (!t.filePath.empty()) {
            HANDLE hFile = CreateFileW(t.filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD size = GetFileSize(hFile, NULL), read;
                std::vector<char> buf(size + 1, 0);
                if (ReadFile(hFile, buf.data(), size, &read, NULL)) {
                    text = std::string(buf.data(), read);
                }
                CloseHandle(hFile);
            }
        }
    }

    std::ofstream out(detachedFile, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open()) return false;

    std::string u8Title = "";
    int lenT = WideCharToMultiByte(CP_UTF8, 0, t.title.c_str(), -1, NULL, 0, NULL, NULL);
    if (lenT > 0) { std::vector<char> b(lenT); WideCharToMultiByte(CP_UTF8, 0, t.title.c_str(), -1, b.data(), lenT, NULL, NULL); u8Title = b.data(); }
    
    std::string u8Path = "";
    int lenP = WideCharToMultiByte(CP_UTF8, 0, t.filePath.c_str(), -1, NULL, 0, NULL, NULL);
    if (lenP > 0) { std::vector<char> b(lenP); WideCharToMultiByte(CP_UTF8, 0, t.filePath.c_str(), -1, b.data(), lenP, NULL, NULL); u8Path = b.data(); }

    POINT ptCursor = { 0, 0 };
    GetCursorPos(&ptCursor);

    out << u8Title << "\n";
    out << u8Path << "\n";
    out << (t.isModified ? 1 : 0) << "\n";
    out << t.eolMode << "\n";
    out << ptCursor.x << "\n";
    out << ptCursor.y << "\n";
    out << "---VELO_BODY---\n";
    if (!text.empty()) {
        out.write(text.data(), text.size());
    }
    out.close();

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    std::wstring cmdLine = L"\"" + std::wstring(exePath) + L"\" --detached \"" + detachedFile + L"\"";
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    BOOL created = CreateProcessW(NULL, (LPWSTR)cmdLine.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (created) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        if (tabs.size() == 1) {
            PostMessage(hSource, WM_CLOSE, 0, 0);
        } else {
            sptr_t docToRelease = tabs[tabIdx].docPointer;
            tabs.erase(tabs.begin() + tabIdx);
            if (activeTabIndex >= tabs.size()) activeTabIndex = tabs.size() - 1;
            Sci(SCI_SETDOCPOINTER, 0, tabs[activeTabIndex].docPointer);
            if (docToRelease) Sci(SCI_RELEASEDOCUMENT, 0, docToRelease);
            UpdateUI(hSource);
            SaveSession();
        }
        return true;
    }
    return false;
}
