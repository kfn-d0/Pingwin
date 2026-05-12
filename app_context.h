#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <winhttp.h>
#include <wlanapi.h>
#include <shellapi.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <deque>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <unordered_map>

#define WM_TRAYICON (WM_USER + 1)
#define IDI_ICON1 101

// menu
enum MenuIDs {
    ID_EXIT = 100,
    ID_CHANGE_HOST,
    ID_VIEW_STATS,
    ID_CLEAR_STATS,
    ID_TOGGLE_FLOAT,
    ID_PING_TYPE_ICMP,
    ID_PING_TYPE_TCP,
    ID_PING_TYPE_UDP,
    ID_INTERVAL_1S,
    ID_INTERVAL_3S,
    ID_INTERVAL_10S,
    ID_ICON_SHOW_LAST,
    ID_ICON_SHOW_AVG,
    ID_CLIPBOARD_MONITOR,
    ID_SPEED_MONITOR,
    ID_SCAN_PORTS,
    ID_ABOUT,
    ID_COPY_DNS,
    ID_ALERT_SOUND,
    ID_ALERT_THRESHOLD_BASE = 4000,
    ID_INTERFACE_BASE = 2000,
    ID_IPV4_BASE = 3000,
    ID_IPV4_GATEWAY = 3001,
    ID_IPV4_LOCAL = 3002,
    ID_IPV4_PUBLIC = 3003,
    ID_PORTS_BASE = 5000,
    ID_MENU_TARGET_GOOGLE = 6000,
    ID_MENU_TARGET_CLOUDFLARE = 6001,
    ID_AUTO_CLOSE_3S = 7003,
    ID_AUTO_CLOSE_10S = 7010,
    ID_AUTO_CLOSE_30S = 7030,
    ID_AUTO_CLOSE_NEVER = 7000,
    ID_COPY_MAC = 8000,
    ID_ALERT_THRESHOLD_CUSTOM = 8001,
    ID_REFRESH_PORTS = 8002,
    ID_SCAN_REMOTE_PORTS = 8003,
    ID_SCAN_REMOTE_START = 8004,
    ID_OPEN_LOGS = 8005,
    ID_THRESHOLD_GREEN = 8006,
    ID_THRESHOLD_YELLOW = 8007,
    ID_THRESHOLD_ORANGE = 8008
};

struct NetworkInfo {
    std::string ipv4Local = "N/A";
    std::vector<std::string> allIpv4;
    std::string ipv4Gateway = "N/A";
    std::string ipv4Public = "N/A";
    std::string dnsServers = "N/A";
    std::string wifiSSID = "N/A";
    std::string wifiSignal = "N/A";
    std::string macAddress = "N/A";
    int activeLocalPorts = 0;
    std::vector<int> activePortsList;
    double downloadSpeedKbps = 0.0;
    double uploadSpeedKbps = 0.0;
    uint64_t linkSpeedBps = 0;
    NET_IFINDEX currentIfIndex = 0;
    std::vector<std::pair<NET_IFINDEX, std::string>> allInterfaces;
};

struct PingStats {
    uint64_t totalSent = 0;
    uint64_t totalLost = 0;
    double sumLatency = 0.0;
    double sumSqLatency = 0.0;
    double sumJitter = 0.0;
    int lastRtt = -1;
    int minRtt = 2147483647;
    int maxRtt = 0;
    std::deque<int> history;
    struct Event { std::string timestamp; std::string description; bool isSpike; };
    std::deque<Event> eventLog;
};

struct Target {
    std::string host = "8.8.8.8";
    IN_ADDR ip{};
    int port = 80;
};

struct PingResult {
    int ping = -1;
    std::string host = "8.8.8.8";
    int port = 80;
    bool isResolving = false;
};

struct AppContext {
    std::atomic<std::shared_ptr<Target>> currentTarget;
    std::atomic<std::shared_ptr<PingResult>> latestResult;
    std::atomic<std::shared_ptr<NetworkInfo>> netInfo;
    std::atomic<std::shared_ptr<PingStats>> stats;

    std::atomic<bool> running{true};
    std::atomic<bool> resolving{false};
    std::atomic<int> pingType{0}; // 0 é icmp 1 para tcp 2 para udp
    std::atomic<int> intervalMs{1000};
    std::atomic<bool> showLastPing{true}; 
    std::atomic<bool> monitorClipboard{false};
    std::atomic<bool> showFloatingWindow{false};
    std::atomic<int> autoCloseSeconds{10};
    std::atomic<bool> soundAlert{false};
    std::atomic<int> alertThreshold{300};
    std::atomic<int> spikeThreshold{250};
    std::atomic<int> greenThreshold{40};
    std::atomic<int> yellowThreshold{80};
    std::atomic<int> orangeThreshold{100};

    HWND hwndMain = NULL;
    HWND hStatsWnd = NULL;
    HWND hFloatWnd = NULL;
    HWND hLogWnd = NULL;
    NOTIFYICONDATAA nid = {};

    std::condition_variable pingCv, netCv;
    std::mutex pingMtx, netMtx, statsMtx;

    int highLatencyCount = 0;
    uint64_t lastNotifyTime = 0;

    // Scan externo
    std::mutex scanMtx;
    std::string externalScanHost = "";
    std::vector<std::pair<int, std::string>> externalScanResults;

    template<typename T>
    struct GdiObj {
        T h;
        GdiObj(T obj) : h(obj) {}
        ~GdiObj() { if (h) DeleteObject(h); }
        operator T() const { return h; }
        bool operator!() const { return !h; }
    };

    void AddEvent(const std::string& desc, bool isSpike = false);
};

extern AppContext g_ctx;

#endif
