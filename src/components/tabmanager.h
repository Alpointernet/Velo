#ifndef TABMANAGER_H
#define TABMANAGER_H

#include "../globals.h"

void SwitchToTab(HWND h, size_t idx);
void CreateNewTab(HWND h, std::wstring path = L"", bool animate = true);
void CloseTab(HWND h, size_t idx);
bool SaveModifiedTabs(HWND h);
void LoadFileInActiveTab(HWND h, const wchar_t* path);
void DoFileOpen(HWND h);
void DoFileSave(HWND h);
void DoFileSaveAs(HWND h);
int GetTabWidth(size_t index);
std::wstring GetFileName(const std::wstring& path);
bool DetachTabToNewWindow(HWND hSource, size_t tabIdx);
bool TransferTabToWindow(HWND hSource, HWND hTarget, size_t tabIdx);

#endif // TABMANAGER_H
