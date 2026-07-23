#include "theme.h"
#include "../globals.h"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <iomanip>

Theme theme;
static FILETIME lastThemeWriteTime = {0};

std::wstring GetThemePath() {
    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::wstring path = std::wstring(appDataPath) + L"\\Velo";
        CreateDirectoryW(path.c_str(), NULL);
        return path + L"\\theme.json";
    }
    return L"";
}

COLORREF ParseHexColor(const std::string& hexStr, COLORREF fallback) {
    std::string s = hexStr;
    // Remove quotes, spaces, and `#` if they exist in the extracted value
    size_t start = s.find_first_not_of(" \t\"#");
    if (start != std::string::npos) {
        size_t end = s.find_last_not_of(" \t\",");
        if (end != std::string::npos) s = s.substr(start, end - start + 1);
        else s = s.substr(start);
    } else {
        return fallback;
    }
    
    if (s.empty()) return fallback;
    int val = 0;
    try {
        val = std::stoi(s, nullptr, 16);
    } catch(...) { return fallback; }
    
    // JSON has #RRGGBB, COLORREF needs 0x00bbggrr
    int r = (val >> 16) & 0xFF;
    int g = (val >> 8) & 0xFF;
    int b = val & 0xFF;
    return RGB(r, g, b);
}

std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos != std::string::npos) {
        size_t colon = json.find(":", pos);
        if (colon != std::string::npos) {
            size_t endLine = json.find("\n", colon);
            if (endLine == std::string::npos) endLine = json.length();
            return json.substr(colon + 1, endLine - colon - 1);
        }
    }
    return "";
}

void SaveDefaultTheme() {
    std::wstring path = GetThemePath();
    if (path.empty()) return;
    
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;
    
    out << "{\n";
    out << "  \"bg\": \"#21252b\",\n";
    out << "  \"tabBg\": \"#181a1f\",\n";
    out << "  \"border\": \"#2c313c\",\n";
    out << "  \"accent\": \"#528bff\",\n";
    out << "  \"hoverBg\": \"#3e4451\",\n";
    out << "  \"textNormal\": \"#d4d4d4\",\n";
    out << "  \"textActive\": \"#ffffff\",\n";
    out << "  \"textDim\": \"#abb2bf\",\n";
    out << "  \"textDisabled\": \"#303644\",\n";
    out << "  \"tabGap\": \"#13151a\",\n\n";
    
    out << "  \"closeHover\": \"#e81123\",\n";
    out << "  \"closePress\": \"#c42b1c\",\n\n";
    
    out << "  \"popupBg\": \"#181a1f\",\n";
    out << "  \"popupBorder\": \"#2c313c\",\n";
    out << "  \"popupAccent\": \"#569cd6\",\n";
    out << "  \"popupText\": \"#abb2bf\",\n";
    out << "  \"popupTextActive\": \"#ffffff\",\n";
    out << "  \"popupHoverBg\": \"#2c313c\",\n";
    out << "  \"popupButtonBg\": \"#21252b\",\n\n";
    
    out << "  \"editorBg\": \"#21252b\",\n";
    out << "  \"editorFg\": \"#d4d4d4\",\n";
    out << "  \"editorLineNumberFg\": \"#495162\",\n";
    out << "  \"editorSelectionBg\": \"#3e4451\",\n";
    out << "  \"editorActiveLineBg\": \"#2c313a\",\n\n";
    
    out << "  \"synKeyword\": \"#569cd6\",\n";
    out << "  \"synString\": \"#ce9178\",\n";
    out << "  \"synNumber\": \"#b5cea8\",\n";
    out << "  \"synComment\": \"#6a9955\",\n";
    out << "  \"synType\": \"#4ec9b0\",\n";
    out << "  \"synFunction\": \"#dcdcaa\",\n";
    out << "  \"synVariable\": \"#9cdcfe\",\n";
    out << "  \"synConstant\": \"#c586c0\",\n\n";
    
    out << "  \"resetColors\": 0\n";
    out << "}\n";
    out.close();
}

void SetDefaultThemeValues() {
    theme.bg = RGB(0x21, 0x25, 0x2b);
    theme.tabBg = RGB(0x18, 0x1a, 0x1f);
    theme.border = RGB(0x2c, 0x31, 0x3c);
    theme.accent = RGB(0x52, 0x8b, 0xff);
    theme.hoverBg = RGB(0x3e, 0x44, 0x51);
    theme.textNormal = RGB(0xd4, 0xd4, 0xd4);
    theme.textActive = RGB(0xff, 0xff, 0xff);
    theme.textDim = RGB(0xab, 0xb2, 0xbf);
    theme.textDisabled = RGB(0x30, 0x36, 0x44);
    theme.tabGap = RGB(0x13, 0x15, 0x1a);
    
    theme.closeHover = RGB(0xe8, 0x11, 0x23);
    theme.closePress = RGB(0xc4, 0x2b, 0x1c);
    
    theme.popupBg = RGB(0x18, 0x1a, 0x1f);
    theme.popupBorder = RGB(0x2c, 0x31, 0x3c);
    theme.popupAccent = RGB(0x56, 0x9c, 0xd6);
    theme.popupText = RGB(0xab, 0xb2, 0xbf);
    theme.popupTextActive = RGB(0xff, 0xff, 0xff);
    theme.popupHoverBg = RGB(0x2c, 0x31, 0x3c);
    theme.popupButtonBg = RGB(0x21, 0x25, 0x2b);
    
    theme.editorBg = RGB(0x21, 0x25, 0x2b);
    theme.editorFg = RGB(0xd4, 0xd4, 0xd4);
    theme.editorLineNumberFg = RGB(0x49, 0x51, 0x62);
    theme.editorSelectionBg = RGB(0x3e, 0x44, 0x51);
    theme.editorActiveLineBg = RGB(0x2c, 0x31, 0x3a);
    
    theme.synKeyword = RGB(0x56, 0x9c, 0xd6);
    theme.synString = RGB(0xce, 0x91, 0x78);
    theme.synNumber = RGB(0xb5, 0xce, 0xa8);
    theme.synComment = RGB(0x6a, 0x99, 0x55);
    theme.synType = RGB(0x4e, 0xc9, 0xb0);
    theme.synFunction = RGB(0xdc, 0xdc, 0xaa);
    theme.synVariable = RGB(0x9c, 0xdc, 0xfe);
    theme.synConstant = RGB(0xc5, 0x86, 0xc0);
}

bool LoadTheme() {
    std::wstring path = GetThemePath();
    if (path.empty()) { SetDefaultThemeValues(); return false; }
    
    std::ifstream in(path);
    if (!in.is_open()) {
        SaveDefaultTheme();
        SetDefaultThemeValues();
        return false;
    }
    
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string json = buffer.str();
    in.close();
    
    std::string resetStr = ExtractJsonString(json, "resetColors");
    if (resetStr.find("1") != std::string::npos) {
        SetDefaultThemeValues();
        SaveDefaultTheme();
        
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            GetFileTime(hFile, NULL, NULL, &lastThemeWriteTime);
            CloseHandle(hFile);
        }
        return true;
    }
    
    SetDefaultThemeValues(); // fallback
    
    theme.bg = ParseHexColor(ExtractJsonString(json, "bg"), theme.bg);
    theme.tabBg = ParseHexColor(ExtractJsonString(json, "tabBg"), theme.tabBg);
    theme.border = ParseHexColor(ExtractJsonString(json, "border"), theme.border);
    theme.accent = ParseHexColor(ExtractJsonString(json, "accent"), theme.accent);
    theme.hoverBg = ParseHexColor(ExtractJsonString(json, "hoverBg"), theme.hoverBg);
    theme.textNormal = ParseHexColor(ExtractJsonString(json, "textNormal"), theme.textNormal);
    theme.textActive = ParseHexColor(ExtractJsonString(json, "textActive"), theme.textActive);
    theme.textDim = ParseHexColor(ExtractJsonString(json, "textDim"), theme.textDim);
    theme.textDisabled = ParseHexColor(ExtractJsonString(json, "textDisabled"), theme.textDisabled);
    theme.tabGap = ParseHexColor(ExtractJsonString(json, "tabGap"), theme.tabGap);
    
    theme.closeHover = ParseHexColor(ExtractJsonString(json, "closeHover"), theme.closeHover);
    theme.closePress = ParseHexColor(ExtractJsonString(json, "closePress"), theme.closePress);
    
    theme.popupBg = ParseHexColor(ExtractJsonString(json, "popupBg"), theme.popupBg);
    theme.popupBorder = ParseHexColor(ExtractJsonString(json, "popupBorder"), theme.popupBorder);
    theme.popupAccent = ParseHexColor(ExtractJsonString(json, "popupAccent"), theme.popupAccent);
    theme.popupText = ParseHexColor(ExtractJsonString(json, "popupText"), theme.popupText);
    theme.popupTextActive = ParseHexColor(ExtractJsonString(json, "popupTextActive"), theme.popupTextActive);
    theme.popupHoverBg = ParseHexColor(ExtractJsonString(json, "popupHoverBg"), theme.popupHoverBg);
    theme.popupButtonBg = ParseHexColor(ExtractJsonString(json, "popupButtonBg"), theme.popupButtonBg);
    
    theme.editorBg = ParseHexColor(ExtractJsonString(json, "editorBg"), theme.editorBg);
    theme.editorFg = ParseHexColor(ExtractJsonString(json, "editorFg"), theme.editorFg);
    theme.editorLineNumberFg = ParseHexColor(ExtractJsonString(json, "editorLineNumberFg"), theme.editorLineNumberFg);
    theme.editorSelectionBg = ParseHexColor(ExtractJsonString(json, "editorSelectionBg"), theme.editorSelectionBg);
    theme.editorActiveLineBg = ParseHexColor(ExtractJsonString(json, "editorActiveLineBg"), theme.editorActiveLineBg);
    
    theme.synKeyword = ParseHexColor(ExtractJsonString(json, "synKeyword"), theme.synKeyword);
    theme.synString = ParseHexColor(ExtractJsonString(json, "synString"), theme.synString);
    theme.synNumber = ParseHexColor(ExtractJsonString(json, "synNumber"), theme.synNumber);
    theme.synComment = ParseHexColor(ExtractJsonString(json, "synComment"), theme.synComment);
    theme.synType = ParseHexColor(ExtractJsonString(json, "synType"), theme.synType);
    theme.synFunction = ParseHexColor(ExtractJsonString(json, "synFunction"), theme.synFunction);
    theme.synVariable = ParseHexColor(ExtractJsonString(json, "synVariable"), theme.synVariable);
    theme.synConstant = ParseHexColor(ExtractJsonString(json, "synConstant"), theme.synConstant);
    
    // Save last write time
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        GetFileTime(hFile, NULL, NULL, &lastThemeWriteTime);
        CloseHandle(hFile);
    }
    return false;
}

int CheckThemeUpdate() {
    std::wstring path = GetThemePath();
    if (path.empty()) return 0;
    
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
        if (CompareFileTime(&fileInfo.ftLastWriteTime, &lastThemeWriteTime) > 0) {
            Sleep(50);
            bool wasReset = LoadTheme();
            return wasReset ? 2 : 1;
        }
    } else {
        SaveDefaultTheme();
        SetDefaultThemeValues();
        LoadTheme();
        return 1;
    }
    return 0;
}
