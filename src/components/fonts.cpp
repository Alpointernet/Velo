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
        std::wstring fontLight = exeDir + L"fonts\\JetBrainsMono-Light.ttf";
        std::wstring fontMedium = exeDir + L"fonts\\JetBrainsMono-Medium.ttf";
        std::wstring fontBold = exeDir + L"fonts\\JetBrainsMono-Bold.ttf";
        std::wstring interLight = exeDir + L"fonts\\Inter-Light.ttf";
        std::wstring interMedium = exeDir + L"fonts\\Inter-Medium.ttf";
        std::wstring interRegular = exeDir + L"fonts\\Inter-Regular.ttf";

        AddFontResourceExW(fontLight.c_str(), FR_PRIVATE, NULL);
        AddFontResourceExW(fontMedium.c_str(), FR_PRIVATE, NULL);
        AddFontResourceExW(fontBold.c_str(), FR_PRIVATE, NULL);
        AddFontResourceExW(interLight.c_str(), FR_PRIVATE, NULL);
        AddFontResourceExW(interMedium.c_str(), FR_PRIVATE, NULL);
        AddFontResourceExW(interRegular.c_str(), FR_PRIVATE, NULL);
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
        std::wstring fontLight = exeDir + L"fonts\\JetBrainsMono-Light.ttf";
        std::wstring fontMedium = exeDir + L"fonts\\JetBrainsMono-Medium.ttf";
        std::wstring fontBold = exeDir + L"fonts\\JetBrainsMono-Bold.ttf";
        std::wstring interLight = exeDir + L"fonts\\Inter-Light.ttf";
        std::wstring interMedium = exeDir + L"fonts\\Inter-Medium.ttf";
        std::wstring interRegular = exeDir + L"fonts\\Inter-Regular.ttf";

        RemoveFontResourceExW(fontLight.c_str(), FR_PRIVATE, NULL);
        RemoveFontResourceExW(fontMedium.c_str(), FR_PRIVATE, NULL);
        RemoveFontResourceExW(fontBold.c_str(), FR_PRIVATE, NULL);
        RemoveFontResourceExW(interLight.c_str(), FR_PRIVATE, NULL);
        RemoveFontResourceExW(interMedium.c_str(), FR_PRIVATE, NULL);
        RemoveFontResourceExW(interRegular.c_str(), FR_PRIVATE, NULL);
    }
}
