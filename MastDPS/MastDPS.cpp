#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <thread>
#include <vector>
#include <chrono>
#include <windows.h>
#include <commdlg.h>
#include <tchar.h>
#include <sstream>
#include <locale>
#include <codecvt>
#include <iomanip>

using namespace std;

std::wstring stringToWstring(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}

void showIntroScreen() {
    HWND hwnd = CreateWindowExW(0, L"STATIC", L"Hi Mast!", WS_POPUP | WS_VISIBLE | SS_CENTER,
        100, 100, 600, 400, nullptr, nullptr, nullptr, nullptr);
    HBITMAP hBitmap = (HBITMAP)LoadImageW(NULL, L"background.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (!hBitmap) {
        MessageBoxW(NULL, L"Failed to load background.bmp!", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    HWND hStatic = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_BITMAP,
        0, 0, 600, 400, hwnd, NULL, NULL, NULL);
    SendMessageW(hStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap);
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
    Sleep(3000);
    DestroyWindow(hwnd);
}

class DAoCDamageParser {
private:
    bool running;
    double total_damage;
    double start_time;
    HWND overlayWindow;
    vector<double> time_log;
    vector<int> damage_log;
    wstring log_file;

public:
    DAoCDamageParser();
    wstring selectLogFile();
    void startParser();
    void stopParser();
    void parseLog();
    void createOverlay();
    void updateOverlay(double total_damage, double elapsed_time);
};

DAoCDamageParser::DAoCDamageParser() : running(false), total_damage(0),
start_time(chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count()) {
    log_file = selectLogFile();
}

wstring DAoCDamageParser::selectLogFile() {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = L"Log Files\0*.log\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        return wstring(filename);
    }
    return L"";
}

void DAoCDamageParser::startParser() {
    if (log_file.empty()) {
        MessageBoxW(NULL, L"No log file selected!", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    running = true;
    thread logThread(&DAoCDamageParser::parseLog, this);
    logThread.detach();
    createOverlay();
}

void DAoCDamageParser::stopParser() {
    running = false;
}

void DAoCDamageParser::parseLog() {
    regex meleeDamagePattern(R"(You attack (.+?) with your (.+?) and hit for (\d+) \(-\d+\) damage!)");
    regex criticalHitPattern(R"(You critically hit (.+?) for an additional (\d+) damage!)");
    regex spellDamagePattern(R"(You perform your (.+?) perfectly! \(\+\d+, Growth Rate: \d+\.\d+\))");
    smatch match;
    while (running) {
        wifstream file(log_file);
        file.imbue(locale(locale(), new codecvt_utf8_utf16<wchar_t>));
        wstring line;
        while (getline(file, line)) {
            string narrowLine = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(line);
            if (regex_search(narrowLine, match, meleeDamagePattern)) {
                int damage = stoi(match[3].str());
                total_damage += damage;
            }
            else if (regex_search(narrowLine, match, criticalHitPattern)) {
                int damage = stoi(match[2].str());
                total_damage += damage;
            }
        }
        this_thread::sleep_for(chrono::seconds(1));
    }
}

void DAoCDamageParser::createOverlay() {
    overlayWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        L"STATIC", L"Damage Overlay",
        WS_POPUP | WS_VISIBLE,
        100, 100, 300, 100,
        nullptr, nullptr, nullptr, nullptr);
    SetLayeredWindowAttributes(overlayWindow, 0, 255, LWA_ALPHA);
}

void DAoCDamageParser::updateOverlay(double total_damage, double elapsed_time) {
    double dps = (elapsed_time > 0) ? total_damage / elapsed_time : 0.0;
    std::wostringstream ss;
    ss << L"Damage: " << std::fixed << std::setprecision(0) << static_cast<long long>(total_damage)
        << L"\nDPS: " << std::fixed << std::setprecision(2) << static_cast<double>(dps);
    std::wstring overlayText = ss.str();
    SetWindowTextW(overlayWindow, overlayText.c_str());
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    showIntroScreen();
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DamageParserWindow";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, L"DamageParserWindow", L"DAoC Damage Parser", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    DAoCDamageParser parser;
    parser.startParser();
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
