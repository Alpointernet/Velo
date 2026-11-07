#ifndef TABMANAGER_H
#define TABMANAGER_H

#include "../globals.h"

void SwitchToTab(HWND h, size_t idx);
void CreateNewTab(HWND h, std::wstring path = L"", bool animate = true);
bool CloseTab(HWND h, size_t idx);
bool CloseAllTabs(HWND h);
bool SaveModifiedTabs(HWND h);
void SaveSession();
void LoadSession(HWND hwndParent);
void LoadFileInActiveTab(HWND h, const wchar_t* path);
void CheckExternalFileChanges(HWND h);
bool ReloadTabFromDisk(HWND h, size_t idx);
void KeepActiveFileEdits(HWND h);
void DoFileOpen(HWND h);
void DoFileSave(HWND h);
void DoFileSaveAs(HWND h);
struct TabLayout {
    int width, contentWidth;
};
std::vector<TabLayout> GetTabLayout();
std::wstring GetFileName(const std::wstring& path);
FILETIME GetFileLastWriteTime(const std::wstring& path);
LRESULT CALLBACK TabRenameEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool DetachTabToNewWindow(HWND hSource, size_t tabIdx);
bool TransferTabToWindow(HWND hSource, HWND hTarget, size_t tabIdx);

#endif // TABMANAGER_H
