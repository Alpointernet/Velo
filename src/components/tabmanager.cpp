#include "../globals.h"
#include "tabmanager.h"
#include "editor.h"
#include "ui_drawing.h"
#include "dialogs.h"
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
    
    // Save current active tab modified state
    if (activeTabIndex < tabs.size() && tabs[activeTabIndex].isLoaded) {
        tabs[activeTabIndex].isModified = (Sci(SCI_GETMODIFY) != 0);
    }
    
    activeTabIndex = idx; 
    Sci(SCI_SETDOCPOINTER, 0, tabs[activeTabIndex].docPointer);
    
    // Lazy load the tab content if not loaded yet!
    if (!tabs[activeTabIndex].isLoaded) {
        bool loaded = false;
        std::wstring backup = tabs[activeTabIndex].backupFile;
        std::wstring path = tabs[activeTabIndex].filePath;
        bool modified = tabs[activeTabIndex].isModified;
        
        if (!backup.empty()) {
            std::wstring backupsDir = GetBackupsDir();
            if (!backupsDir.empty()) {
                std::wstring backupPath = backupsDir + L"\\" + backup;
                HANDLE hFile = CreateFileW(backupPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD size = GetFileSize(hFile, NULL), read;
                    std::vector<char> buf(size + 1, 0);
                    if (ReadFile(hFile, buf.data(), size, &read, NULL)) {
                        Sci(SCI_CLEARALL);
                        Sci(SCI_APPENDTEXT, read, (LPARAM)buf.data());
                        if (!modified) Sci(SCI_SETSAVEPOINT);
                        Sci(SCI_EMPTYUNDOBUFFER);
                        loaded = true;
                    }
                    CloseHandle(hFile);
                }
            }
        }
        
        if (!loaded && !path.empty()) {
            HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD size = GetFileSize(hFile, NULL), read;
                std::vector<char> buf(size + 1, 0);
                if (ReadFile(hFile, buf.data(), size, &read, NULL)) {
                    Sci(SCI_CLEARALL);
                    Sci(SCI_APPENDTEXT, read, (LPARAM)buf.data());
                    if (!modified) Sci(SCI_SETSAVEPOINT);
                    Sci(SCI_EMPTYUNDOBUFFER);
                    loaded = true;
                }
                CloseHandle(hFile);
            }
        }
        
        tabs[activeTabIndex].isLoaded = true;
    }
    
    activeLineStart = -1; activeLineEnd = -1; ApplySyntax(); SyncLineNumbers(true);
    RecalculateScrollWidth();
    if (searchVisible) UpdateSearchMatches();
    UpdateUI(h);
    SetFocus(hwndScintilla);
    SaveSession();
}

void CreateNewTab(HWND h, std::wstring path) {
    tabs.push_back({ path, path.empty() ? L"Untitled" : GetFileName(path), Sci(SCI_CREATEDOCUMENT), false, L"", true });
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
            tabs[activeTabIndex] = { path, GetFileName(path), tabs[activeTabIndex].docPointer, false, L"", true };
            activeLineStart = -1; activeLineEnd = -1; ApplySyntax(); SyncLineNumbers(true);
            RecalculateScrollWidth();
            if (searchVisible) UpdateSearchMatches();
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
    out << "count=" << tabs.size() << "\n";
    for (size_t i = 0; i < tabs.size(); ++i) {
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
        if (isModified || wpath.empty()) {
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
        
        out << "tab_path_" << i << "=" << sPath << "\n";
        out << "tab_title_" << i << "=" << sTitle << "\n";
        out << "tab_modified_" << i << "=" << (isModified ? 1 : 0) << "\n";
        out << "tab_backup_" << i << "=" << sBackup << "\n";
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
            else if (key == "activeTab") loadedActiveTab = std::stoi(val);
            else if (key == "count") {
                tabCount = std::stoi(val);
                tabPaths.resize(tabCount);
                tabTitles.resize(tabCount);
                tabModifieds.resize(tabCount, false);
                tabBackups.resize(tabCount);
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
    }
    
    if (tabCount > 0) {
        for (int i = 0; i < tabCount; ++i) {
            std::wstring path = tabPaths[i];
            std::wstring title = tabTitles[i];
            sptr_t doc = Sci(SCI_CREATEDOCUMENT);
            bool modified = tabModifieds[i];
            std::wstring backup = tabBackups[i];
            tabs.push_back({ path, title, doc, modified, backup, false });
        }
        
        isSavingSession = false;
        if (tabs.empty()) {
            CreateNewTab(hwndParent);
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
    } else {
        isSavingSession = false;
        CreateNewTab(hwndParent);
    }
}
