#include "../globals.h"
#include "fonts.h"

void LoadFonts() {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
        std::wstring exeDir = exePath;
        size_t lastSlash = exeDir.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            exeDir = exeDir.substr(0, lastSlash + 1);
        }
        std::wstring fontMedium = exeDir + L"fonts\\JetBrainsMono-Medium.ttf";
        std::wstring interMedium = exeDir + L"fonts\\Inter-Medium.ttf";

        AddFontResourceExW(fontMedium.c_str(), FR_PRIVATE, NULL);
        AddFontResourceExW(interMedium.c_str(), FR_PRIVATE, NULL);
    }
}

void UnloadFonts() {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
        std::wstring exeDir = exePath;
        size_t lastSlash = exeDir.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            exeDir = exeDir.substr(0, lastSlash + 1);
        }
        std::wstring fontMedium = exeDir + L"fonts\\JetBrainsMono-Medium.ttf";
        std::wstring interMedium = exeDir + L"fonts\\Inter-Medium.ttf";

        RemoveFontResourceExW(fontMedium.c_str(), FR_PRIVATE, NULL);
        RemoveFontResourceExW(interMedium.c_str(), FR_PRIVATE, NULL);
    }
}
