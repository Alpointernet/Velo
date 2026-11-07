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
void UpdateSearchMatches();
void UpdateCurrentMatchIndex();
void SearchNext();
void SearchPrev();
void SearchSelectAll();
void SearchReplace();
void SearchReplaceAll();
void UpdateBraceMatch();
void UpdateCustomIndicators(HWND hwndSci);
void RunCurrentFile(HWND hwnd, bool runAsAdmin = false);
void CleanupUserChoiceAssociations();
LRESULT CALLBACK SearchEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ReplaceEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ScrollbarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SciSubProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // EDITOR_H
