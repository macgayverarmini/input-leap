/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 */

#pragma once

#include "arch/win32/ArchMiscWindows.h"
#include "base/IEventQueue.h"
#include <string>

namespace inputleap {

class MSWindowsSettingsDialog {
public:
    static void show(HWND parentWindow, IEventQueue* events);
    static bool loadSettings(std::string& serverName, std::string& clientName,
                             std::string& position, std::string& address,
                             int& port, bool& enableTls, bool& disableClientCert,
                             bool& autoStart, bool& clipboardSharing);
    static bool saveSettings(const std::string& serverName, const std::string& clientName,
                             const std::string& position, const std::string& address,
                             int port, bool enableTls, bool disableClientCert,
                             bool autoStart, bool clipboardSharing);
    static void generateConfigFile(const std::string& serverName, const std::string& clientName,
                                   const std::string& position, bool clipboardSharing);
    static void setAutoStart(bool enable);
    static bool isAutoStartEnabled();

private:
    static INT_PTR CALLBACK dialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void initDialog(HWND hwnd);
    static void onSave(HWND hwnd);
    static std::string getIniPath();
    static std::string getConfPath();

    static IEventQueue* s_events;
};

} // namespace inputleap
