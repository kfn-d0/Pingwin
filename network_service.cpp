#include "network_service.h"
#include <ctime>
#include <set>
#include <thread>

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

static std::string GetTimeStr() {
    time_t n = time(0); 
    struct tm l;
    localtime_s(&l, &n);
    char b[16]; 
    strftime(b, 16, "%H:%M:%S", &l);
    return b;
}

void ShowToast(const std::string& title, const std::string& msg) {
    uint64_t now = GetTickCount64();
    if (now - g_ctx.lastNotifyTime < 60000) return; 
    g_ctx.lastNotifyTime = now;

    NOTIFYICONDATAA nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_ctx.hwndMain;
    nid.uID = 1;
    nid.uFlags = NIF_INFO | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAYICON;
    
    snprintf(nid.szInfoTitle, 64, "%s", title.c_str());
    snprintf(nid.szInfo, 256, "%s", msg.c_str());
    nid.dwInfoFlags = NIIF_INFO;
    
    Shell_NotifyIconA(NIM_MODIFY, &nid);
}

static std::string FetchPublicIPFromURL(const wchar_t* host, const wchar_t* path = NULL) {
    HINTERNET hS = WinHttpOpen(L"PingWin/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hS) return "";
    HINTERNET hC = WinHttpConnect(hS, host, INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hC) { WinHttpCloseHandle(hS); return ""; }
    HINTERNET hR = WinHttpOpenRequest(hC, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hR) { WinHttpCloseHandle(hC); WinHttpCloseHandle(hS); return ""; }
    
    std::string res = "";
    if (WinHttpSendRequest(hR, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        if (WinHttpReceiveResponse(hR, NULL)) {
            DWORD sz = 0; 
            while (WinHttpQueryDataAvailable(hR, &sz) && sz > 0) {
                std::vector<char> buffer(sz + 1);
                DWORD rd = 0; 
                if (WinHttpReadData(hR, buffer.data(), sz, &rd)) { 
                    buffer[rd] = 0; 
                    res += buffer.data(); 
                }
            }
        }
    }
    WinHttpCloseHandle(hR); WinHttpCloseHandle(hC); WinHttpCloseHandle(hS);
    if (!res.empty() && res.back() == '\n') res.pop_back();
    return res;
}

std::string FetchPublicIP() {
    std::string ip = FetchPublicIPFromURL(L"api.ipify.org");
    if (ip.empty() || ip.find('.') == std::string::npos) {
        ip = FetchPublicIPFromURL(L"checkip.amazonaws.com");
    }
    return ip.empty() ? "N/A" : ip;
}

struct NetPersistentState {
    std::mutex mtx;
    uint64_t lastIn = 0, lastOut = 0;
    std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
    int pubCounter = 0;
    std::string lastPub = "N/A";
    std::string lastLocal = "";
};
static NetPersistentState g_netState;

void CollectNetworkData() {
    auto ni = std::make_shared<NetworkInfo>();
    
    std::lock_guard<std::mutex> lock(g_netState.mtx);

    ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX, sz = 16384;
    std::vector<char> buf(sz);
    if (GetAdaptersAddresses(AF_UNSPEC, flags, 0, (PIP_ADAPTER_ADDRESSES)buf.data(), &sz) == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES p = (PIP_ADAPTER_ADDRESSES)buf.data(), best = 0;
        DWORD bestIdx = 0;
        auto target = g_ctx.currentTarget.load();
        if (target) GetBestInterface(target->ip.s_addr, &bestIdx);

        while (p) {
            std::wstring desc = p->Description;
            std::string sDesc(desc.begin(), desc.end());
            ni->allInterfaces.push_back({p->IfIndex, sDesc});
            
            if (p->IfIndex == bestIdx) best = p;
            else if (!best && p->IfType != IF_TYPE_SOFTWARE_LOOPBACK && p->OperStatus == IfOperStatusUp) {
                if (!best || p->FirstGatewayAddress) best = p;
            }

            if (p->IfType != IF_TYPE_SOFTWARE_LOOPBACK && p->OperStatus == IfOperStatusUp) {
                PIP_ADAPTER_UNICAST_ADDRESS u = p->FirstUnicastAddress;
                while (u) {
                    if (u->Address.lpSockaddr->sa_family == AF_INET) {
                        char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &((sockaddr_in*)u->Address.lpSockaddr)->sin_addr, ip, INET_ADDRSTRLEN);
                        ni->allIpv4.push_back(ip);
                    }
                    u = u->Next;
                }
            }
            p = p->Next;
        }
        if (best) {
            ni->currentIfIndex = best->IfIndex;
            PIP_ADAPTER_UNICAST_ADDRESS u = best->FirstUnicastAddress;
            while (u) {
                if (u->Address.lpSockaddr->sa_family == AF_INET) {
                    char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &((sockaddr_in*)u->Address.lpSockaddr)->sin_addr, ip, INET_ADDRSTRLEN);
                    ni->ipv4Local = ip; break;
                }
                u = u->Next;
            }
            PIP_ADAPTER_GATEWAY_ADDRESS_LH g = best->FirstGatewayAddress;
            if (g && g->Address.lpSockaddr->sa_family == AF_INET) {
                char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &((sockaddr_in*)g->Address.lpSockaddr)->sin_addr, ip, INET_ADDRSTRLEN);
                ni->ipv4Gateway = ip;
            }
            
            MIB_IF_ROW2 row = {0}; row.InterfaceIndex = best->IfIndex;
            if (GetIfEntry2(&row) == NO_ERROR) {
                auto now = std::chrono::steady_clock::now();
                double dt = std::chrono::duration<double>(now - g_netState.lastTime).count();
                if (dt > 0.5) {
                    ni->downloadSpeedKbps = (row.InOctets - g_netState.lastIn) * 8.0 / 1024.0 / dt;
                    ni->uploadSpeedKbps = (row.OutOctets - g_netState.lastOut) * 8.0 / 1024.0 / dt;
                    ni->linkSpeedBps = row.ReceiveLinkSpeed;
                    g_netState.lastIn = row.InOctets; g_netState.lastOut = row.OutOctets; g_netState.lastTime = now;
                }
            }
            
            char mac[32]; 
            snprintf(mac, 32, "%02X-%02X-%02X-%02X-%02X-%02X", best->PhysicalAddress[0], best->PhysicalAddress[1], best->PhysicalAddress[2], best->PhysicalAddress[3], best->PhysicalAddress[4], best->PhysicalAddress[5]);
            ni->macAddress = mac;

            PIP_ADAPTER_DNS_SERVER_ADDRESS d = best->FirstDnsServerAddress;
            if (d) {
                char ip[INET6_ADDRSTRLEN];
                if (d->Address.lpSockaddr->sa_family == AF_INET) inet_ntop(AF_INET, &((sockaddr_in*)d->Address.lpSockaddr)->sin_addr, ip, INET6_ADDRSTRLEN);
                else inet_ntop(AF_INET6, &((sockaddr_in6*)d->Address.lpSockaddr)->sin6_addr, ip, INET6_ADDRSTRLEN);
                ni->dnsServers = ip;
            }
        }
    }

    if (g_netState.pubCounter++ % 150 == 0) {
        std::thread([](std::string oldIp){ 
            std::string nPub = FetchPublicIP();
            if (nPub != "N/A" && oldIp != "N/A" && nPub != oldIp) {
                g_ctx.AddEvent("IP Publico mudou: " + oldIp + " -> " + nPub, true);
                ShowToast("Pingwin - Mudanca de IP", "Seu IP Publico mudou para: " + nPub);
            }
            std::lock_guard<std::mutex> l(g_netState.mtx);
            g_netState.lastPub = nPub; 
        }, g_netState.lastPub).detach();
    }
    ni->ipv4Public = g_netState.lastPub;

    if (!ni->ipv4Local.empty() && !g_netState.lastLocal.empty() && ni->ipv4Local != g_netState.lastLocal) {
        g_ctx.AddEvent("IP Local mudou: " + g_netState.lastLocal + " -> " + ni->ipv4Local, true);
        ShowToast("Pingwin - Rede Local", "Seu IP mudou para: " + ni->ipv4Local);
    }
    g_netState.lastLocal = ni->ipv4Local;

    // Wifi
    HANDLE hW; DWORD v;
    if (WlanOpenHandle(2, 0, &v, &hW) == 0) {
        PWLAN_INTERFACE_INFO_LIST l;
        if (WlanEnumInterfaces(hW, 0, &l) == 0) {
            for (DWORD i = 0; i < l->dwNumberOfItems; i++) {
                if (l->InterfaceInfo[i].isState == wlan_interface_state_connected) {
                    PWLAN_CONNECTION_ATTRIBUTES a; DWORD s;
                    if (WlanQueryInterface(hW, &l->InterfaceInfo[i].InterfaceGuid, wlan_intf_opcode_current_connection, 0, &s, (PVOID*)&a, 0) == 0) {
                        char ssid[33]={0}; memcpy(ssid, a->wlanAssociationAttributes.dot11Ssid.ucSSID, a->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
                        ni->wifiSSID = ssid; ni->wifiSignal = std::to_string(a->wlanAssociationAttributes.wlanSignalQuality) + "%";
                        WlanFreeMemory(a); break;
                    }
                }
            }
            WlanFreeMemory(l);
        }
        WlanCloseHandle(hW, 0);
    }

    // portas locais
    MIB_TCPTABLE_OWNER_PID* tbl = 0; ULONG tsz = 0;
    GetExtendedTcpTable(0, &tsz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
    std::vector<char> tbuf(tsz);
    if (GetExtendedTcpTable(tbuf.data(), &tsz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) == NO_ERROR) {
        tbl = (MIB_TCPTABLE_OWNER_PID*)tbuf.data();
        ni->activeLocalPorts = tbl->dwNumEntries;
        std::set<int> pset;
        for (DWORD i=0; i<tbl->dwNumEntries; i++) pset.insert(ntohs((u_short)tbl->table[i].dwLocalPort));
        ni->activePortsList.clear();
        for (int p : pset) { ni->activePortsList.push_back(p); if (ni->activePortsList.size() >= 50) break; }
    }

    g_ctx.netInfo.store(ni);
}

void NetworkService_ScanRemotePorts(std::string host) {
    std::thread([host]() {
        {
            std::lock_guard<std::mutex> l(g_ctx.scanMtx);
            g_ctx.externalScanHost = host;
            g_ctx.externalScanResults.clear();
        }
        
        g_ctx.AddEvent("Port Scan iniciado para " + host, true);

        IN_ADDR ip;
        if (InetPtonA(AF_INET, host.c_str(), &ip) != 1) {
            addrinfo h = {0};
            h.ai_family = AF_INET;
            addrinfo* r = 0;
            if (getaddrinfo(host.c_str(), 0, &h, &r) == 0) {
                ip = ((sockaddr_in*)r->ai_addr)->sin_addr;
                freeaddrinfo(r);
            } else {
                return;
            }
        }
        
        int ports[] = {21, 22, 23, 25, 53, 80, 110, 135, 139, 143, 443, 445, 587, 993, 995, 1723, 3306, 3389, 5900, 8080};
        for (int p : ports) {
            SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (sock == INVALID_SOCKET) continue;

            unsigned long mode = 1;
            ioctlsocket(sock, FIONBIO, &mode);
            
            sockaddr_in sa = {0};
            sa.sin_family = AF_INET;
            sa.sin_addr = ip;
            sa.sin_port = htons(p);
            
            connect(sock, (sockaddr*)&sa, sizeof(sa));
            
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(sock, &writeSet);
            
            timeval tv = {0, 500000}; // 500ms
            if (select(0, NULL, &writeSet, NULL, &tv) > 0) {
                std::lock_guard<std::mutex> l(g_ctx.scanMtx);
                std::string svc = (p == 80 ? "HTTP" : (p == 443 ? "HTTPS" : (p == 22 ? "SSH" : (p == 3389 ? "RDP" : "Servico"))));
                g_ctx.externalScanResults.push_back({p, svc});
                g_ctx.AddEvent("Porta " + std::to_string(p) + " ABERTA no alvo " + host, false);
            }
            
            closesocket(sock);
            if (!g_ctx.running) break;
        }
        g_ctx.AddEvent("Port Scan concluido para " + host, true);
    }).detach();
}

void NetworkService_RunTraceroute(std::string host) {
    HANDLE hI = IcmpCreateFile(); if (hI == INVALID_HANDLE_VALUE) return;
    IN_ADDR ia; InetPtonA(AF_INET, host.c_str(), &ia);
    char sd[32] = "PingWinScan", rb[sizeof(ICMP_ECHO_REPLY) + 32];
    IP_OPTION_INFORMATION opt = {0};
    g_ctx.AddEvent("Traceroute iniciado para " + host, true);
    for (int ttl = 1; ttl <= 20; ttl++) {
        opt.Ttl = ttl;
        DWORD ret = IcmpSendEcho(hI, ia.s_addr, sd, 32, &opt, rb, sizeof(rb), 1000);
        if (ret > 0) {
            PICMP_ECHO_REPLY pr = (PICMP_ECHO_REPLY)rb;
            char ipb[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &pr->Address, ipb, INET_ADDRSTRLEN);
            g_ctx.AddEvent("Hop " + std::to_string(ttl) + ": " + ipb + " (" + std::to_string(pr->RoundTripTime) + "ms)", false);
            if (pr->Address == ia.s_addr) break;
        } else { 
            g_ctx.AddEvent("Hop " + std::to_string(ttl) + ": * (Timeout)", false); 
        }
        if (!g_ctx.running) break;
    }
    IcmpCloseHandle(hI);
}

int TcpPing(IN_ADDR addr, int port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return -1;
    DWORD timeout = 1000; setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    sockaddr_in sa = {0}; sa.sin_family = AF_INET; sa.sin_addr = addr; sa.sin_port = htons(port);
    auto start = std::chrono::steady_clock::now();
    if (connect(s, (sockaddr*)&sa, sizeof(sa)) == 0) {
        auto end = std::chrono::steady_clock::now();
        closesocket(s);
        return (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
    closesocket(s); return -1;
}

int UdpPing(IN_ADDR addr, int port) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return -1;
    DWORD timeout = 1000; setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    
    BOOL bNewBehavior = FALSE; DWORD dwBytesReturned = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);

    sockaddr_in sa = {0}; sa.sin_family = AF_INET; sa.sin_addr = addr; sa.sin_port = htons(port);
    if (connect(s, (sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR) { closesocket(s); return -1; }

    char buf[1] = {0}, rb[1];
    auto start = std::chrono::steady_clock::now();
    if (send(s, buf, 1, 0) == SOCKET_ERROR) { closesocket(s); return -1; }
    
    int res = recv(s, rb, 1, 0);
    auto end = std::chrono::steady_clock::now();
    int rtt = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (res == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAECONNRESET || err == WSAECONNABORTED) { closesocket(s); return -1; }
        closesocket(s); return -1; 
    }

    closesocket(s); return rtt > 0 ? rtt : 1; 
}

void NetworkThread() {
    while (g_ctx.running) {
        CollectNetworkData();
        std::unique_lock<std::mutex> l(g_ctx.netMtx);
        g_ctx.netCv.wait_for(l, std::chrono::seconds(2), [&]{ return !g_ctx.running.load(); });
    }
}

void PingThread() {
    HANDLE hI = IcmpCreateFile();
    char sd[32] = "PingWin", rb[sizeof(ICMP_ECHO_REPLY) + 32];
    while (g_ctx.running) {
        auto t = g_ctx.currentTarget.load(); int p = -1;
        if (t && t->ip.s_addr != INADDR_NONE) {
            int type = g_ctx.pingType;
            if (type == 1) p = TcpPing(t->ip, t->port);
            else if (type == 2) p = UdpPing(t->ip, t->port);
            else {
                if (IcmpSendEcho(hI, t->ip.s_addr, sd, 32, 0, rb, sizeof(rb), 1000))
                    p = ((PICMP_ECHO_REPLY)rb)->RoundTripTime;
            }
        }
        {
            std::lock_guard<std::mutex> lock(g_ctx.statsMtx);
            auto s = g_ctx.stats.load(); auto ns = std::make_shared<PingStats>(*s); ns->totalSent++;
            if (p == -1) { 
                ns->totalLost++; ns->history.push_back(-1); 
                if (s->lastRtt != -1) ns->eventLog.push_back({GetTimeStr(), "Timeout!", false}); 
                g_ctx.highLatencyCount = 0;
            }
            else {
                ns->sumLatency += p; ns->sumSqLatency += (double)p*p; if (s->lastRtt >= 0) ns->sumJitter += abs(p - s->lastRtt);
                if (p < ns->minRtt) ns->minRtt = p; if (p > ns->maxRtt) ns->maxRtt = p; ns->history.push_back(p);
                if (s->lastRtt == -1 && s->totalSent > 0) ns->eventLog.push_back({GetTimeStr(), "Reconectado (" + std::to_string(p) + "ms)", false});
                if (s->lastRtt > 0 && p > s->lastRtt * 2 && p > 50) {
                    ns->eventLog.push_back({GetTimeStr(), "Latencia dobrou: " + std::to_string(s->lastRtt) + "ms -> " + std::to_string(p) + "ms", true});
                }
                
                if (p > g_ctx.spikeThreshold) {
                    g_ctx.highLatencyCount++;
                    if (g_ctx.highLatencyCount >= 30) {
                        g_ctx.highLatencyCount = 0;
                        ns->eventLog.push_back({GetTimeStr(), "Alta latencia persistente detectada (30 pings). Iniciando Traceroute...", true});
                        std::thread(NetworkService_RunTraceroute, t->host).detach();
                    }
                } else {
                    g_ctx.highLatencyCount = 0;
                }

                if (g_ctx.soundAlert && p > g_ctx.alertThreshold) {
                    MessageBeep(MB_ICONWARNING);
                }
            }
            ns->lastRtt = p; if (ns->history.size() > 100) ns->history.pop_front(); if (ns->eventLog.size() > 200) ns->eventLog.pop_front();
            g_ctx.stats.store(ns);
        }
        auto fr = std::make_shared<PingResult>(); fr->ping = p; fr->host = t->host; fr->port = t->port; g_ctx.latestResult.store(fr);
        std::unique_lock<std::mutex> l(g_ctx.pingMtx);
        g_ctx.pingCv.wait_for(l, std::chrono::milliseconds(g_ctx.intervalMs), [&]{ return !g_ctx.running.load(); });
    }
    IcmpCloseHandle(hI);
}

static std::jthread tPing, tNet;

void NetworkService_Start() {
    if (tPing.joinable()) tPing.request_stop();
    if (tNet.joinable()) tNet.request_stop();

    tPing = std::jthread(PingThread);
    tNet = std::jthread(NetworkThread);
}

void NetworkService_Stop() {
    g_ctx.running = false;
    g_ctx.pingCv.notify_all();
    g_ctx.netCv.notify_all();
    
}

void NetworkService_ResolveHost(std::string input) {
    std::thread([input]() {
        g_ctx.resolving = true;
        
        std::string host = input;
        int port = 80;
        size_t colon = input.find(':');
        
        if (colon != std::string::npos) {
            host = input.substr(0, colon);
            port = atoi(input.substr(colon + 1).c_str());
        }

        auto res = std::make_shared<PingResult>();
        res->host = host;
        res->isResolving = true;
        g_ctx.latestResult.store(res);

        IN_ADDR addr;
        if (InetPtonA(AF_INET, host.c_str(), &addr) == 1) {
            auto nt = std::make_shared<Target>();
            nt->host = host;
            nt->ip = addr;
            nt->port = port;
            g_ctx.currentTarget.store(nt);
        } else {
            addrinfo h = {0};
            h.ai_family = AF_INET;
            addrinfo* r = 0;
            if (getaddrinfo(host.c_str(), 0, &h, &r) == 0) {
                auto nt = std::make_shared<Target>();
                nt->host = host;
                nt->ip = ((sockaddr_in*)r->ai_addr)->sin_addr;
                nt->port = port;
                g_ctx.currentTarget.store(nt);
                freeaddrinfo(r);
            }
        }
        
        g_ctx.resolving = false;
        g_ctx.pingCv.notify_all();
    }).detach();
}
