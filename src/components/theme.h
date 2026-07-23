#ifndef THEME_H
#define THEME_H

#include <windows.h>
#include <string>

struct Theme {
    COLORREF bg;
    COLORREF tabBg;
    COLORREF border;
    COLORREF accent;
    COLORREF hoverBg;
    COLORREF textNormal;
    COLORREF textActive;
    COLORREF textDim;
    COLORREF textDisabled;
    COLORREF tabGap;
    
    // Close button specific native colors
    COLORREF closeHover;
    COLORREF closePress;
    
    // Editor specific colors
    COLORREF editorBg;
    COLORREF editorFg;
    COLORREF editorLineNumberFg;
    COLORREF editorSelectionBg;
    COLORREF editorActiveLineBg;
    
    // Lexer syntax colors
    COLORREF synKeyword;
    COLORREF synString;
    COLORREF synNumber;
    COLORREF synComment;
    COLORREF synType;
    COLORREF synFunction;
    COLORREF synVariable;
    COLORREF synConstant;
};

extern Theme theme;

std::wstring GetThemePath();
bool LoadTheme();
void SaveDefaultTheme();
int CheckThemeUpdate();

#endif // THEME_H
