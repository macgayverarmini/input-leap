/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "inputleap/ServerApp.h"
#include "arch/Arch.h"
#include "base/Log.h"
#include "base/EventQueue.h"

#if WINAPI_MSWINDOWS
#include "MSWindowsServerTaskBarReceiver.h"
#include "MSWindowsSettingsDialog.h"
#include <vector>
#endif

namespace inputleap {

#if WINAPI_XWINDOWS || WINAPI_LIBEI || WINAPI_CARBON
CreateTaskBarReceiverFunc createTaskBarReceiver = nullptr;
#endif

static std::vector<std::string> s_autoArgs;
static std::vector<const char*> s_autoArgv;

int server_main(int argc, char** argv)
{
#if SYSAPI_WIN32
    // record window instance for tray icon, etc
    ArchMiscWindows::setInstanceWin32(GetModuleHandle(nullptr));

    if (argc <= 1) {
        // Standalone Tray launch without arguments: load settings from INI/Conf
        std::string serverName, clientName, position, address;
        int port = 24800;
        bool enableTls = true, disableClientCert = true, autoStart = false, clipboardSharing = false;

        MSWindowsSettingsDialog::loadSettings(serverName, clientName, position, address, port, enableTls, disableClientCert, autoStart, clipboardSharing);

        // Ensure input-leap.conf exists
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string dir(exePath);
        size_t p = dir.find_last_of("\\/");
        std::string confPath = (p != std::string::npos) ? dir.substr(0, p + 1) + "input-leap.conf" : "input-leap.conf";

        FILE* f = fopen(confPath.c_str(), "r");
        if (!f) {
            MSWindowsSettingsDialog::generateConfigFile(serverName, clientName, position, clipboardSharing);
        } else {
            fclose(f);
        }

        s_autoArgs.clear();
        s_autoArgv.clear();

        s_autoArgs.push_back(argv ? argv[0] : "input-leaps.exe");
        s_autoArgs.push_back("-f");
        s_autoArgs.push_back("--restart");
        if (!enableTls) {
            s_autoArgs.push_back("--disable-crypto");
        }
        if (disableClientCert) {
            s_autoArgs.push_back("--disable-client-cert-checking");
        }
        s_autoArgs.push_back("--debug");
        s_autoArgs.push_back("INFO");
        
        std::string logPath = (p != std::string::npos) ? dir.substr(0, p + 1) + "input-leaps.log" : "input-leaps.log";
        s_autoArgs.push_back("--log");
        s_autoArgs.push_back(logPath);

        s_autoArgs.push_back("-c");
        s_autoArgs.push_back(confPath);
        s_autoArgs.push_back("--name");
        s_autoArgs.push_back(serverName);
        s_autoArgs.push_back("-a");
        s_autoArgs.push_back(address + ":" + std::to_string(port));

        for (const auto& a : s_autoArgs) {
            s_autoArgv.push_back(a.c_str());
        }
        argc = static_cast<int>(s_autoArgv.size());
        argv = const_cast<char**>(s_autoArgv.data());
    }
#endif

#ifdef __APPLE__
    /* Silence "is calling TIS/TSM in non-main thread environment" as it is a red
    herring that causes a lot of issues to be filed for the MacOS client/server.
    */
    setenv("OS_ACTIVITY_DT_MODE", "NO", true);
#endif

    Arch arch;
    arch.init();

    Log log;
    EventQueue events;

    ServerApp app(&events, createTaskBarReceiver);
    int result = app.run(argc, argv);
#if SYSAPI_WIN32
    if (IsDebuggerPresent()) {
        printf("\n\nHit a key to close...\n");
        getchar();
    }
#endif
    return result;
}

} // namespace inputleap

#if SYSAPI_WIN32
#include <shellapi.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int numArgs = 0;
    LPWSTR* wideArgv = CommandLineToArgvW(GetCommandLineW(), &numArgs);
    std::vector<std::string> utf8Args;
    std::vector<char*> utf8Argv;

    if (wideArgv != nullptr) {
        for (int i = 0; i < numArgs; ++i) {
            int len = WideCharToMultiByte(CP_UTF8, 0, wideArgv[i], -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                std::string arg(len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, wideArgv[i], -1, &arg[0], len, nullptr, nullptr);
                if (!arg.empty() && arg.back() == '\0') {
                    arg.pop_back();
                }
                utf8Args.push_back(arg);
            }
        }
        LocalFree(wideArgv);
    }

    for (auto& a : utf8Args) {
        utf8Argv.push_back(&a[0]);
    }

    return inputleap::server_main(static_cast<int>(utf8Argv.size()), utf8Argv.data());
}
#endif

int main(int argc, char** argv)
{
    return inputleap::server_main(argc, argv);
}
