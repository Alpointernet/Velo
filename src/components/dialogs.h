#ifndef DIALOGS_H
#define DIALOGS_H

#include "../globals.h"

int ShowCustomMessageBox(HWND hwndParent, const std::wstring& message, const std::wstring& title, UINT type = MB_YESNOCANCEL);
int ShowCustomPopupMenu(HWND hwndParent, int x, int y, const std::vector<PopupMenuItem>& items, bool bottomAlign = false);
void ShowSettingsDialog(HWND hwndParent);

#endif // DIALOGS_H
