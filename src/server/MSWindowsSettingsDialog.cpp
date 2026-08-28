/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 */

#include "MSWindowsSettingsDialog.h"
#include "resource.h"
#include "base/EventTypes.h"
#include "base/Log.h"

#include <windows.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <fstream>
#include <sstream>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace inputleap {

IEventQueue* MSWindowsSettingsDialog::s_events = nullptr;

std::string MSWindowsSettingsDialog::getIniPath()
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string path(exePath);
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        return path.substr(0, pos + 1) + "settings.ini";
    }
    return "settings.ini";
}

std::string MSWindowsSettingsDialog::getConfPath()
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string path(exePath);
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        return path.substr(0, pos + 1) + "input-leap.conf";
    }
    return "input-leap.conf";
}

void MSWindowsSettingsDialog::show(HWND parentWindow, IEventQueue* events)
{
    s_events = events;
    HINSTANCE hInst = GetModuleHandle(nullptr);
    DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_SETTINGS_DIALOG), parentWindow, dialogProc, 0);
}

INT_PTR CALLBACK MSWindowsSettingsDialog::dialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG:
        initDialog(hwnd);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_SAVE_APPLY:
        case IDOK:
            onSave(hwnd);
            EndDialog(hwnd, IDOK);
            return TRUE;

        case IDC_BTN_CANCEL:
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hwnd, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

void MSWindowsSettingsDialog::initDialog(HWND hwnd)
{
    // Apply Windows 10/11 Dark Mode if enabled in Windows
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(hwnd, 19 /* DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 */, &darkMode, sizeof(darkMode));

    // Center dialog on screen
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

    // Initialize Position ComboBox
    HWND cmb = GetDlgItem(hwnd, IDC_CMB_POSITION);
    SendMessageA(cmb, CB_ADDSTRING, 0, (LPARAM)"A esquerda do Servidor (Left)");
    SendMessageA(cmb, CB_ADDSTRING, 0, (LPARAM)"A direita do Servidor (Right)");
    SendMessageA(cmb, CB_ADDSTRING, 0, (LPARAM)"Acima do Servidor (Above)");
    SendMessageA(cmb, CB_ADDSTRING, 0, (LPARAM)"Abaixo do Servidor (Below)");

    // Load settings
    std::string serverName, clientName, position, address;
    int port = 24800;
    bool enableTls = true, disableClientCert = true, autoStart = false, clipboardSharing = false;

    loadSettings(serverName, clientName, position, address, port, enableTls, disableClientCert, autoStart, clipboardSharing);

    // Set control values
    SetDlgItemTextA(hwnd, IDC_EDT_SERVER_NAME, serverName.c_str());
    SetDlgItemTextA(hwnd, IDC_EDT_CLIENT_NAME, clientName.c_str());
    SetDlgItemTextA(hwnd, IDC_EDT_LISTEN_ADDR, address.c_str());
    SetDlgItemInt(hwnd, IDC_EDT_PORT, port, FALSE);

    if (position == "right") {
        SendMessage(cmb, CB_SETCURSEL, 1, 0);
    } else if (position == "above" || position == "up") {
        SendMessage(cmb, CB_SETCURSEL, 2, 0);
    } else if (position == "below" || position == "down") {
        SendMessage(cmb, CB_SETCURSEL, 3, 0);
    } else {
        SendMessage(cmb, CB_SETCURSEL, 0, 0); // Default left
    }

    CheckDlgButton(hwnd, IDC_CHK_TLS, enableTls ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_CHK_DISABLE_CLIENT_CERT, disableClientCert ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_CHK_AUTOSTART, autoStart ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_CHK_CLIPBOARD, clipboardSharing ? BST_CHECKED : BST_UNCHECKED);
}

void MSWindowsSettingsDialog::onSave(HWND hwnd)
{
    char serverName[256] = {0};
    char clientName[256] = {0};
    char address[256] = {0};

    GetDlgItemTextA(hwnd, IDC_EDT_SERVER_NAME, serverName, sizeof(serverName));
    GetDlgItemTextA(hwnd, IDC_EDT_CLIENT_NAME, clientName, sizeof(clientName));
    GetDlgItemTextA(hwnd, IDC_EDT_LISTEN_ADDR, address, sizeof(address));
    int port = GetDlgItemInt(hwnd, IDC_EDT_PORT, nullptr, FALSE);
    if (port <= 0 || port > 65535) port = 24800;

    HWND cmb = GetDlgItem(hwnd, IDC_CMB_POSITION);
    int sel = (int)SendMessage(cmb, CB_GETCURSEL, 0, 0);
    std::string position = "left";
    if (sel == 1) position = "right";
    else if (sel == 2) position = "above";
    else if (sel == 3) position = "below";

    bool enableTls = (IsDlgButtonChecked(hwnd, IDC_CHK_TLS) == BST_CHECKED);
    bool disableClientCert = (IsDlgButtonChecked(hwnd, IDC_CHK_DISABLE_CLIENT_CERT) == BST_CHECKED);
    bool autoStart = (IsDlgButtonChecked(hwnd, IDC_CHK_AUTOSTART) == BST_CHECKED);
    bool clipboardSharing = (IsDlgButtonChecked(hwnd, IDC_CHK_CLIPBOARD) == BST_CHECKED);

    // Save INI and Conf
    saveSettings(serverName, clientName, position, address, port, enableTls, disableClientCert, autoStart, clipboardSharing);
    generateConfigFile(serverName, clientName, position, clipboardSharing);
    setAutoStart(autoStart);

    LOG_INFO("Configuracoes salvas com sucesso! Reiniciando servidor...");

    // Trigger server reset / restart
    if (s_events != nullptr) {
        s_events->add_event(EventType::SERVER_APP_RESET_SERVER, s_events->getSystemTarget());
    }
}

bool MSWindowsSettingsDialog::loadSettings(std::string& serverName, std::string& clientName,
                                          std::string& position, std::string& address,
                                          int& port, bool& enableTls, bool& disableClientCert,
                                          bool& autoStart, bool& clipboardSharing)
{
    std::string ini = getIniPath();
    char buf[256];

    // Default server name = Windows Computer Name
    char defServer[MAX_COMPUTERNAME_LENGTH + 1] = "DESKTOP-QF10AN0";
    DWORD size = sizeof(defServer);
    GetComputerNameA(defServer, &size);

    GetPrivateProfileStringA("Server", "ServerName", defServer, buf, sizeof(buf), ini.c_str());
    serverName = buf;

    GetPrivateProfileStringA("Server", "ClientName", "Ubuntu", buf, sizeof(buf), ini.c_str());
    clientName = buf;

    GetPrivateProfileStringA("Server", "Position", "left", buf, sizeof(buf), ini.c_str());
    position = buf;

    GetPrivateProfileStringA("Server", "ListenAddress", "0.0.0.0", buf, sizeof(buf), ini.c_str());
    address = buf;

    port = GetPrivateProfileIntA("Server", "Port", 24800, ini.c_str());
    enableTls = (GetPrivateProfileIntA("Server", "EnableTLS", 1, ini.c_str()) != 0);
    disableClientCert = (GetPrivateProfileIntA("Server", "DisableClientCertCheck", 1, ini.c_str()) != 0);
    clipboardSharing = (GetPrivateProfileIntA("Server", "ClipboardSharing", 0, ini.c_str()) != 0);
    autoStart = isAutoStartEnabled();

    return true;
}

bool MSWindowsSettingsDialog::saveSettings(const std::string& serverName, const std::string& clientName,
                                          const std::string& position, const std::string& address,
                                          int port, bool enableTls, bool disableClientCert,
                                          bool autoStart, bool clipboardSharing)
{
    std::string ini = getIniPath();
    WritePrivateProfileStringA("Server", "ServerName", serverName.c_str(), ini.c_str());
    WritePrivateProfileStringA("Server", "ClientName", clientName.c_str(), ini.c_str());
    WritePrivateProfileStringA("Server", "Position", position.c_str(), ini.c_str());
    WritePrivateProfileStringA("Server", "ListenAddress", address.c_str(), ini.c_str());
    WritePrivateProfileStringA("Server", "Port", std::to_string(port).c_str(), ini.c_str());
    WritePrivateProfileStringA("Server", "EnableTLS", enableTls ? "1" : "0", ini.c_str());
    WritePrivateProfileStringA("Server", "DisableClientCertCheck", disableClientCert ? "1" : "0", ini.c_str());
    WritePrivateProfileStringA("Server", "ClipboardSharing", clipboardSharing ? "1" : "0", ini.c_str());
    WritePrivateProfileStringA("Server", "AutoStart", autoStart ? "1" : "0", ini.c_str());
    return true;
}

void MSWindowsSettingsDialog::generateConfigFile(const std::string& serverName, const std::string& clientName,
                                                const std::string& position, bool clipboardSharing)
{
    std::string sName = serverName.empty() ? "DESKTOP-QF10AN0" : serverName;
    std::string cName = clientName.empty() ? "Ubuntu" : clientName;

    std::string serverToClient, clientToServer;
    if (position == "right") {
        serverToClient = "right";
        clientToServer = "left";
    } else if (position == "above" || position == "up") {
        serverToClient = "up";
        clientToServer = "down";
    } else if (position == "below" || position == "down") {
        serverToClient = "down";
        clientToServer = "up";
    } else {
        serverToClient = "left";
        clientToServer = "right";
    }

    std::ofstream out(getConfPath());
    if (!out.is_open()) return;

    out << "section: screens\n";
    out << "\t" << sName << ":\n";
    out << "\t\thalfDuplexCapsLock = false\n";
    out << "\t\thalfDuplexNumLock = false\n";
    out << "\t\thalfDuplexScrollLock = false\n";
    out << "\t\txtestIsXineramaUnaware = false\n";
    out << "\t\tpreserveFocus = false\n";
    out << "\t\tswitchCorners = none\n";
    out << "\t\tswitchCornerSize = 0\n";
    out << "\t" << cName << ":\n";
    out << "\t\thalfDuplexCapsLock = false\n";
    out << "\t\thalfDuplexNumLock = false\n";
    out << "\t\thalfDuplexScrollLock = false\n";
    out << "\t\txtestIsXineramaUnaware = false\n";
    out << "\t\tpreserveFocus = false\n";
    out << "\t\tswitchCorners = none\n";
    out << "\t\tswitchCornerSize = 0\n";
    out << "end\n\n";

    out << "section: links\n";
    out << "\t" << sName << ":\n";
    out << "\t\t" << serverToClient << " = " << cName << "\n";
    out << "\t" << cName << ":\n";
    out << "\t\t" << clientToServer << " = " << sName << "\n";
    out << "end\n\n";

    out << "section: options\n";
    out << "\trelativeMouseMoves = false\n";
    out << "\tscreenSaverSync = true\n";
    out << "\twin32KeepForeground = false\n";
    out << "\tclipboardSharing = " << (clipboardSharing ? "true" : "false") << "\n";
    out << "\tswitchCorners = none\n";
    out << "\tswitchCornerSize = 0\n";
    out << "end\n";

    out.close();
}

void MSWindowsSettingsDialog::setAutoStart(bool enable)
{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            char exePath[MAX_PATH];
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            std::string cmd = "\"" + std::string(exePath) + "\"";
            RegSetValueExA(hKey, "InputLeapServer", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)cmd.length() + 1);
        } else {
            RegDeleteValueA(hKey, "InputLeapServer");
        }
        RegCloseKey(hKey);
    }
}

bool MSWindowsSettingsDialog::isAutoStartEnabled()
{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buf[MAX_PATH];
        DWORD size = sizeof(buf);
        DWORD type = 0;
        LSTATUS st = RegQueryValueExA(hKey, "InputLeapServer", nullptr, &type, (LPBYTE)buf, &size);
        RegCloseKey(hKey);
        return (st == ERROR_SUCCESS);
    }
    return false;
}

} // namespace inputleap
