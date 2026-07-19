#ifndef EDITOR_H
#define EDITOR_H

#include "../globals.h"

// Inline Scintilla SendMessage wrapper
inline sptr_t Sci(UINT m, WPARAM w = 0, LPARAM l = 0) {
    return SendMessage(hwndScintilla, m, w, l);
}

void RecalculateScrollWidth();
void StyleScintilla(HWND hwndSci);
void ApplySyntax();
void SyncLineNumbers(bool rebuild = false);
void UpdateLineNumberWidth();
int GetTotalMarginWidth();
void FindNextText(const std::string& query, bool forward);

#endif // EDITOR_H
