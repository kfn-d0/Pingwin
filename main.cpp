#include "app_context.h"
#include "network_service.h"
#include "gui_manager.h"

#include <ctime>

AppContext g_ctx;

void AppContext::AddEvent(const std::string& desc, bool isSpike) {
    std::lock_guard<std::mutex> l(statsMtx);
    auto s = stats.load(); if (!s) return;
    auto ns = std::make_shared<PingStats>(*s);
    time_t n = time(0); tm* lt = localtime(&n);
    char b[16]; strftime(b, 16, "%H:%M:%S", lt);
    ns->eventLog.push_back({b, desc, isSpike});
    if (ns->eventLog.size() > 200) ns->eventLog.pop_front();
    stats.store(ns);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR lp, int n) {
    HANDLE hM = CreateMutexA(0, 1, "PingWinInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    
    WSADATA w; WSAStartup(0x0202, &w);
    
    g_ctx.stats.store(std::make_shared<PingStats>());
    g_ctx.netInfo.store(std::make_shared<NetworkInfo>());
    auto t = std::make_shared<Target>(); 
    InetPtonA(AF_INET, t->host.c_str(), &t->ip); 
    g_ctx.currentTarget.store(t);
    g_ctx.latestResult.store(std::make_shared<PingResult>());

    GUIManager_Init(hi);
    NetworkService_Start();

    MSG m;
    while (GetMessage(&m, 0, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }

    // limpezaa
    NetworkService_Stop();
    GUIManager_Cleanup();
    WSACleanup();
    CloseHandle(hM);
    return 0;
}
