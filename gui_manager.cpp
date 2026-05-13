#include "gui_manager.h"
#include "network_service.h"
#include "ui_windows.h"
#include <cstdio>

class IconCache {
    std::unordered_map<int, HICON> cache; int size = 0;
public:
    HICON Get(int p) {
        if (!size) size = GetSystemMetrics(SM_CXSMICON);
        int k = (p == -2) ? -2 : (p < 0 ? -1 : (p > 999 ? 1000 : p));
        if (cache.count(k)) return cache[k];
        HDC hS = GetDC(0), hD = CreateCompatibleDC(hS);
        HBITMAP b = CreateCompatibleBitmap(hS, size, size), bo = (HBITMAP)SelectObject(hD, b);
        COLORREF c;
        if (p == -2) c = RGB(70, 130, 180); // Azul
        else if (p < 0) c = RGB(220, 30, 30); // Vermelho (Perda)
        else if (p <= g_ctx.greenThreshold) c = RGB(30, 180, 30); // Verde (Bom)
        else if (p <= g_ctx.yellowThreshold) c = RGB(200, 180, 20); // Amarelo (OK)
        else if (p <= g_ctx.orangeThreshold) c = RGB(220, 120, 20); // Laranja (Ruim)
        else c = RGB(200, 30, 30); // Vermelho (Critico)

        HBRUSH br = CreateSolidBrush(c); 
        RECT r = {0, 0, size, size}; 
        FillRect(hD, &r, br); 
        DeleteObject(br);

        char t[8]; 
        if (p == -2) strcpy(t, "..."); 
        else if (p < 0) strcpy(t, "X"); 
        else if (p > 999) strcpy(t, ">1k"); 
        else snprintf(t, 8, "%d", p);

        SetTextColor(hD, RGB(255, 255, 255)); 
        SetBkMode(hD, TRANSPARENT);
        
        HFONT f = CreateFontA(-(size * (strlen(t) >= 3 ? 55 : 75) / 100), 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
        HFONT fo = (HFONT)SelectObject(hD, f); 
        DrawTextA(hD, t, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hD, fo); DeleteObject(f);
        HBITMAP m = CreateBitmap(size, size, 1, 1, NULL);
        SelectObject(hD, bo); DeleteDC(hD); ReleaseDC(0, hS);
        ICONINFO ii = {TRUE, 0, 0, m, b}; HICON hi = CreateIconIndirect(&ii);
        DeleteObject(b); DeleteObject(m); if (hi) cache[k] = hi; return hi;
    }
    ~IconCache() { for (auto& p : cache) DestroyIcon(p.second); }
} iconCache;

static int g_inputType = 0;
LRESULT CALLBACK InputDlgProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_CREATE) {
        const char* lbl = "Digite o Host ou IP:";
        if (g_inputType == 1) lbl = "Limite p/ Alerta (ms):";
        else if (g_inputType == 2) lbl = "Host para Scan Externo:";
        else if (g_inputType == 3) lbl = "Limite VERDE (Bom):";
        else if (g_inputType == 4) lbl = "Limite AMARELO (OK):";
        else if (g_inputType == 5) lbl = "Limite LARANJA (Alta):";
        CreateWindowA("STATIC", lbl, WS_CHILD|WS_VISIBLE, 10, 10, 200, 20, h, 0, 0, 0);
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 10, 35, 230, 25, h, (HMENU)101, 0, 0);
        CreateWindowA("BUTTON", "OK", WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON, 150, 70, 90, 30, h, (HMENU)IDOK, 0, 0);
    } else if (m == WM_COMMAND && LOWORD(wp) == IDOK) {
        char buf[256]; GetDlgItemTextA(h, 101, buf, 256);
        if (strlen(buf) > 0) {
            if (g_inputType == 1) g_ctx.alertThreshold = atoi(buf);
            else if (g_inputType == 2) NetworkService_ScanRemotePorts(buf);
            else if (g_inputType == 3) g_ctx.greenThreshold = atoi(buf);
            else if (g_inputType == 4) g_ctx.yellowThreshold = atoi(buf);
            else if (g_inputType == 5) g_ctx.orangeThreshold = atoi(buf);
            else NetworkService_ResolveHost(buf);
        }
        DestroyWindow(h);
    } else if (m == WM_CLOSE) DestroyWindow(h);
    return DefWindowProc(h, m, wp, lp);
}

LRESULT CALLBACK ThresholdsDlgProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_CREATE) {
        CreateWindowA("STATIC", "Bom (Verde) ms:", WS_CHILD|WS_VISIBLE, 10, 15, 120, 20, h, 0, 0, 0);
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", std::to_string(g_ctx.greenThreshold.load()).c_str(), WS_CHILD|WS_VISIBLE|ES_NUMBER, 130, 12, 60, 25, h, (HMENU)201, 0, 0);
        
        CreateWindowA("STATIC", "OK (Amarelo) ms:", WS_CHILD|WS_VISIBLE, 10, 45, 120, 20, h, 0, 0, 0);
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", std::to_string(g_ctx.yellowThreshold.load()).c_str(), WS_CHILD|WS_VISIBLE|ES_NUMBER, 130, 42, 60, 25, h, (HMENU)202, 0, 0);
        
        CreateWindowA("STATIC", "Alta (Laranja) ms:", WS_CHILD|WS_VISIBLE, 10, 75, 120, 20, h, 0, 0, 0);
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", std::to_string(g_ctx.orangeThreshold.load()).c_str(), WS_CHILD|WS_VISIBLE|ES_NUMBER, 130, 72, 60, 25, h, (HMENU)203, 0, 0);
        
        CreateWindowA("BUTTON", "Salvar", WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON, 60, 110, 100, 30, h, (HMENU)IDOK, 0, 0);
    } else if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        auto DrawBox = [&](int y, COLORREF c) {
            AppContext::GdiObj<HBRUSH> br(CreateSolidBrush(c));
            RECT r = {200, y, 220, y + 20};
            FillRect(hdc, &r, br);
        };
        DrawBox(15, RGB(30, 180, 30));
        DrawBox(45, RGB(200, 180, 20));
        DrawBox(75, RGB(220, 120, 20));
        EndPaint(h, &ps);
    } else if (m == WM_COMMAND && LOWORD(wp) == IDOK) {
        char b1[16], b2[16], b3[16];
        GetDlgItemTextA(h, 201, b1, 16); g_ctx.greenThreshold = atoi(b1);
        GetDlgItemTextA(h, 202, b2, 16); g_ctx.yellowThreshold = atoi(b2);
        GetDlgItemTextA(h, 203, b3, 16); g_ctx.orangeThreshold = atoi(b3);
        DestroyWindow(h);
    } else if (m == WM_CLOSE) DestroyWindow(h);
    return DefWindowProc(h, m, wp, lp);
}

struct PortEntry { int port; const char* service; const char* desc; };

static void OpenInputDialog(int type) {
    g_inputType = type;
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = InputDlgProc;
    wc.hInstance = GetModuleHandle(0);
    wc.lpszClassName = "PingWinInput";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON1));
    RegisterClassA(&wc);
    CreateWindowExA(WS_EX_TOPMOST, "PingWinInput", "Entrada de dados", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 265, 150, 0, 0, GetModuleHandle(0), 0);
}

void BuildContextMenu(HWND h) {
    POINT pt; 
    GetCursorPos(&pt);
    HMENU hm = CreatePopupMenu(); 
    auto n = g_ctx.netInfo.load(); 
    auto curT = g_ctx.currentTarget.load();
    if (!n || !curT) return;

    std::string ifName = "N/A";
    for (auto& ifc : n->allInterfaces) {
        if (ifc.first == n->currentIfIndex) ifName = ifc.second;
    }

    char connStr[256]; 
    snprintf(connStr, 256, "Conexao: %s (%llu Mbps)", ifName.c_str(), n->linkSpeedBps / 1000000);
    AppendMenuA(hm, MF_GRAYED | MF_STRING, 0, connStr);
    AppendMenuA(hm, MF_SEPARATOR, 0, 0);

    AppendMenuA(hm, MF_STRING, ID_VIEW_STATS, "Dashboard de estatisticas");
    AppendMenuA(hm, MF_SEPARATOR, 0, 0);

    // alvo
    HMENU hmTarget = CreatePopupMenu();
    AppendMenuA(hmTarget, (curT && curT->host=="8.8.8.8"?MF_CHECKED:0)|MF_STRING, ID_MENU_TARGET_GOOGLE, "Google (8.8.8.8)");
    AppendMenuA(hmTarget, (curT && curT->host=="1.1.1.1"?MF_CHECKED:0)|MF_STRING, ID_MENU_TARGET_CLOUDFLARE, "Cloudflare (1.1.1.1)");
    AppendMenuA(hmTarget, MF_STRING, ID_CHANGE_HOST, "Personalizado...");
    AppendMenuA(hm, MF_POPUP, (UINT_PTR)hmTarget, "Alvo do ping");

    HMENU hmType = CreatePopupMenu();
    AppendMenuA(hmType, (g_ctx.pingType==0?MF_CHECKED:0)|MF_STRING, ID_PING_TYPE_ICMP, "ICMP (padrao)");
    
    HMENU hmTcp = CreatePopupMenu();
    PortEntry tcpList[] = {
        {80, "HTTP", "Sites sem HTTPS"}, {443, "HTTPS", "Porta geral"}, {22, "SSH", "Linux/networking"}, {3389, "RDP", "Windows remoto"},
        {25, "SMTP", "Email"}, {587, "SMTP Sub", "Email autenticado"}, {110, "POP3", "Email antigo"}, {143, "IMAP", "Email"},
        {53, "DNS TCP", "Fallback DNS"}, {445, "SMB", "Compartilhamento Windows"}, {8080, "HTTP alt", "APIs/paineis"}, {8443, "HTTPS alt", "Paineis/admin"}
    };
    for (auto& p : tcpList) {
        char buf[256]; sprintf(buf, ":%-5d  %-16s \t%s", p.port, p.service, p.desc);
        AppendMenuA(hmTcp, (g_ctx.pingType==1 && curT && curT->port==p.port?MF_CHECKED:0)|MF_STRING, ID_PORTS_BASE + 10000 + p.port, buf);
    }
    AppendMenuA(hmTcp, MF_SEPARATOR, 0, 0);
    AppendMenuA(hmTcp, MF_STRING, ID_PORTS_BASE + 20000, "Personalizado...");
    AppendMenuA(hmType, (g_ctx.pingType==1?MF_CHECKED:0)|MF_POPUP, (UINT_PTR)hmTcp, "TCP");

    HMENU hmUdp = CreatePopupMenu();
    PortEntry udpList[] = {
        {53, "DNS", "Melhor opcao UDP"}, {123, "NTP", "Relogio"}, {67, "DHCP", "Rede local"}, {161, "SNMP", "Equipamentos de Rede"},
        {500, "IPsec/IKE", "VPN"}, {4500, "NAT-T IPsec", "VPN"}, {514, "Syslog", "Logs"}, {3478, "STUN", "VoIP/WebRTC"},
        {5060, "SIP", "VoIP"}, {33434, "Traceroute", "Linux classico"}
    };
    for (auto& p : udpList) {
        char buf[256]; sprintf(buf, ":%-5d  %-16s \t%s", p.port, p.service, p.desc);
        AppendMenuA(hmUdp, (g_ctx.pingType==2 && curT && curT->port==p.port?MF_CHECKED:0)|MF_STRING, ID_PORTS_BASE + 30000 + p.port, buf);
    }
    AppendMenuA(hmUdp, MF_SEPARATOR, 0, 0);
    AppendMenuA(hmUdp, MF_STRING, ID_PORTS_BASE + 40000, "Personalizado...");
    AppendMenuA(hmType, (g_ctx.pingType==2?MF_CHECKED:0)|MF_POPUP, (UINT_PTR)hmUdp, "UDP");

    AppendMenuA(hm, MF_POPUP, (UINT_PTR)hmType, "Tipo de ping");

    // exibicao do ping
    HMENU hmDisp = CreatePopupMenu();
    AppendMenuA(hmDisp, (g_ctx.showLastPing?MF_CHECKED:0)|MF_STRING, ID_ICON_SHOW_LAST, "Ultimo ping");
    AppendMenuA(hmDisp, (!g_ctx.showLastPing?MF_CHECKED:0)|MF_STRING, ID_ICON_SHOW_AVG, "Media (ultimos 3)");
    AppendMenuA(hm, MF_POPUP, (UINT_PTR)hmDisp, "Exibicao do ping");

    // intervalo de ping
    HMENU hmInt = CreatePopupMenu();
    AppendMenuA(hmInt, (g_ctx.intervalMs==1000?MF_CHECKED:0)|MF_STRING, ID_INTERVAL_1S, "1 segundo");
    AppendMenuA(hmInt, (g_ctx.intervalMs==3000?MF_CHECKED:0)|MF_STRING, ID_INTERVAL_3S, "3 segundos");
    AppendMenuA(hmInt, (g_ctx.intervalMs==10000?MF_CHECKED:0)|MF_STRING, ID_INTERVAL_10S, "10 segundos");
    AppendMenuA(hm, MF_POPUP, (UINT_PTR)hmInt, "Intervalo de ping");

    AppendMenuA(hm, MF_SEPARATOR, 0, 0);

    // portas locais
    HMENU hmPorts = CreatePopupMenu();
    AppendMenuA(hmPorts, MF_STRING, ID_REFRESH_PORTS, "Escanear novamente");
    AppendMenuA(hmPorts, MF_SEPARATOR, 0, 0);
    for (int p : n->activePortsList) AppendMenuA(hmPorts, MF_STRING, ID_PORTS_BASE + p, (":" + std::to_string(p)).c_str());
    AppendMenuA(hm, MF_POPUP, (UINT_PTR)hmPorts, ("Portas locais (" + std::to_string(n->activeLocalPorts) + " ativas)").c_str());

    // scan alvo externo
    HMENU hmExtScan = CreatePopupMenu();
    AppendMenuA(hmExtScan, MF_STRING, ID_SCAN_REMOTE_START, "Iniciar novo scan...");
    AppendMenuA(hmExtScan, MF_SEPARATOR, 0, 0);
    {
        std::lock_guard<std::mutex> l(g_ctx.scanMtx);
        if (g_ctx.externalScanHost.empty()) {
            AppendMenuA(hmExtScan, MF_GRAYED | MF_STRING, 0, "Nenhum scan ativo");
        } else {
            char hostLbl[256]; sprintf(hostLbl, "Alvo: %s", g_ctx.externalScanHost.c_str());
            AppendMenuA(hmExtScan, MF_GRAYED | MF_STRING, 0, hostLbl);
            if (g_ctx.externalScanResults.empty()) {
                AppendMenuA(hmExtScan, MF_GRAYED | MF_STRING, 0, "Buscando portas abertas...");
            } else {
                for (auto& r : g_ctx.externalScanResults) {
                    char resLbl[128];
                    snprintf(resLbl, 128, "Porta %d: %s", r.first, r.second.c_str());
                    AppendMenuA(hmExtScan, MF_STRING, ID_PORTS_BASE + 50000 + r.first, resLbl);
                }
            }
        }
    }
    AppendMenuA(hm, MF_POPUP, (UINT_PTR)hmExtScan, "Scan de alvo externo");

    AppendMenuA(hm, MF_SEPARATOR, 0, 0);
    AppendMenuA(hm, (g_ctx.monitorClipboard?MF_CHECKED:0)|MF_STRING, ID_CLIPBOARD_MONITOR, "Monitorar ips (copiar) para Ping rapido.");

    HMENU hmAuto = CreatePopupMenu();
    AppendMenuA(hmAuto, (g_ctx.autoCloseSeconds==3?MF_CHECKED:0)|MF_STRING, ID_AUTO_CLOSE_3S, "3 segundos");
    AppendMenuA(hmAuto, (g_ctx.autoCloseSeconds==10?MF_CHECKED:0)|MF_STRING, ID_AUTO_CLOSE_10S, "10 segundos");
    AppendMenuA(hmAuto, (g_ctx.autoCloseSeconds==30?MF_CHECKED:0)|MF_STRING, ID_AUTO_CLOSE_30S, "30 segundos");
    AppendMenuA(hmAuto, (g_ctx.autoCloseSeconds==0?MF_CHECKED:0)|MF_STRING, ID_AUTO_CLOSE_NEVER, "Nunca");
    AppendMenuA(hm, MF_POPUP, (UINT_PTR)hmAuto, "Auto-fechar janelas flutuantes de ping");

    AppendMenuA(hm, MF_SEPARATOR, 0, 0);

    HMENU hmIf = CreatePopupMenu();
    for (size_t i=0; i<n->allInterfaces.size(); i++) AppendMenuA(hmIf, (n->currentIfIndex == n->allInterfaces[i].first ? MF_CHECKED : 0) | MF_STRING, ID_INTERFACE_BASE + i, n->allInterfaces[i].second.c_str());
    AppendMenuA(hm, MF_POPUP, (UINT_PTR)hmIf, "Interface de rede");

    AppendMenuA(hm, MF_SEPARATOR, 0, 0);
    AppendMenuA(hm, MF_STRING, ID_COPY_DNS, ("DNS: " + n->dnsServers).c_str());
    
    HMENU hmIpv4 = CreatePopupMenu();
    AppendMenuA(hmIpv4, MF_STRING, ID_IPV4_GATEWAY, ("v4 Gateway: " + n->ipv4Gateway).c_str());
    AppendMenuA(hmIpv4, MF_STRING, ID_IPV4_LOCAL, ("v4 Local: " + n->ipv4Local).c_str());
    AppendMenuA(hmIpv4, MF_STRING, ID_IPV4_PUBLIC, ("v4 Publico: " + n->ipv4Public).c_str());
    AppendMenuA(hm, MF_POPUP, (UINT_PTR)hmIpv4, "Enderecos IPv4");

    AppendMenuA(hm, MF_STRING, ID_COPY_MAC, ("MAC: " + n->macAddress).c_str());
    AppendMenuA(hm, MF_SEPARATOR, 0, 0);

    HMENU hmConfig = CreatePopupMenu();
    
    // Submenu notif
    HMENU hmNotify = CreatePopupMenu();
    AppendMenuA(hmNotify, (g_ctx.soundAlert ? MF_CHECKED : 0) | MF_STRING, ID_ALERT_SOUND, "Alerta sonoro (Beep)");
    char alertLbl[64]; 
    snprintf(alertLbl, 64, "Gatilho de alerta (Toast): %dms", g_ctx.alertThreshold.load());
    AppendMenuA(hmNotify, MF_STRING, ID_ALERT_THRESHOLD_CUSTOM, alertLbl);
    AppendMenuA(hmConfig, MF_POPUP, (UINT_PTR)hmNotify, "Notificacoes");

    // Submenu
    AppendMenuA(hmConfig, MF_STRING, ID_THRESHOLD_GREEN, "Ajustar limites e cores...");

    AppendMenuA(hm, MF_POPUP, (UINT_PTR)hmConfig, "Configurar");

    // Logs
    AppendMenuA(hm, MF_STRING, ID_OPEN_LOGS, "Logs de Eventos...");

    AppendMenuA(hm, MF_SEPARATOR, 0, 0);
    AppendMenuA(hm, MF_STRING, ID_EXIT, "Sair");

    SetForegroundWindow(h);
    int cmd = TrackPopupMenu(hm, TPM_RETURNCMD, pt.x, pt.y, 0, h, 0);
    DestroyMenu(hm);

    auto CopyToClipboard = [&](std::string s) {
        if (OpenClipboard(h)) { EmptyClipboard(); HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, s.size()+1); if (hg) { memcpy(GlobalLock(hg), s.c_str(), s.size()+1); GlobalUnlock(hg); SetClipboardData(CF_TEXT, hg); } CloseClipboard(); }
    };

    if (cmd == ID_EXIT) PostMessage(h, WM_CLOSE, 0, 0);
    else if (cmd == ID_INTERVAL_1S) g_ctx.intervalMs = 1000;
    else if (cmd == ID_INTERVAL_3S) g_ctx.intervalMs = 3000;
    else if (cmd == ID_INTERVAL_10S) g_ctx.intervalMs = 10000;
    else if (cmd == ID_REFRESH_PORTS) CollectNetworkData();
    else if (cmd == ID_SCAN_REMOTE_START) OpenInputDialog(2);
    else if (cmd == ID_OPEN_LOGS) LogWindow_Open(GetModuleHandle(0));
    else if (cmd == ID_VIEW_STATS) Dashboard_Open(GetModuleHandle(0));
    else if (cmd == ID_PING_TYPE_ICMP) g_ctx.pingType = 0;
    else if (cmd == ID_PORTS_BASE + 20000) { g_ctx.pingType = 1; OpenInputDialog(0); }
    else if (cmd >= ID_PORTS_BASE + 30000 && cmd < ID_PORTS_BASE + 40000) { g_ctx.pingType = 2; auto t = g_ctx.currentTarget.load(); auto nt = std::make_shared<Target>(*t); nt->port = cmd - (ID_PORTS_BASE + 30000); g_ctx.currentTarget.store(nt); }
    else if (cmd == ID_PORTS_BASE + 40000) { g_ctx.pingType = 2; OpenInputDialog(0); }
    else if (cmd >= ID_PORTS_BASE + 50000 && cmd < ID_PORTS_BASE + 130000) {
        int p = cmd - (ID_PORTS_BASE + 50000);
        auto nt = std::make_shared<Target>();
        nt->host = g_ctx.externalScanHost;
        nt->port = p;
        InetPtonA(AF_INET, g_ctx.externalScanHost.c_str(), &nt->ip);
        g_ctx.currentTarget.store(nt);
        g_ctx.pingType = 1;
    }
    else if (cmd == ID_ICON_SHOW_LAST) g_ctx.showLastPing = true;
    else if (cmd == ID_ICON_SHOW_AVG) g_ctx.showLastPing = false;
    else if (cmd == ID_CLIPBOARD_MONITOR) { g_ctx.monitorClipboard = !g_ctx.monitorClipboard; if (g_ctx.monitorClipboard) AddClipboardFormatListener(h); else RemoveClipboardFormatListener(h); }
    else if (cmd == ID_COPY_DNS) CopyToClipboard(n->dnsServers);
    else if (cmd == ID_COPY_MAC) CopyToClipboard(n->macAddress);
    else if (cmd == ID_IPV4_GATEWAY) CopyToClipboard(n->ipv4Gateway);
    else if (cmd == ID_IPV4_LOCAL) CopyToClipboard(n->ipv4Local);
    else if (cmd == ID_IPV4_PUBLIC) CopyToClipboard(n->ipv4Public);
    else if (cmd >= ID_PORTS_BASE && cmd < ID_INTERFACE_BASE) CopyToClipboard(std::to_string(cmd - ID_PORTS_BASE));
    else if (cmd == ID_ALERT_SOUND) g_ctx.soundAlert = !g_ctx.soundAlert;
    else if (cmd == ID_THRESHOLD_GREEN) {
        WNDCLASSA wc = {0}; 
        if (!GetClassInfoA(GetModuleHandle(0), "PingWinThresholds", &wc)) {
            wc.lpfnWndProc = ThresholdsDlgProc; 
            wc.hInstance = GetModuleHandle(0); 
            wc.lpszClassName = "PingWinThresholds"; 
            wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1); 
            wc.hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON1));
            RegisterClassA(&wc);
        }
        CreateWindowExA(WS_EX_TOPMOST, "PingWinThresholds", "Limites e Cores", WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE, 300, 300, 245, 190, 0, 0, GetModuleHandle(0), 0);
    }
    else if (cmd == ID_MENU_TARGET_GOOGLE) NetworkService_ResolveHost("8.8.8.8");
    else if (cmd == ID_MENU_TARGET_CLOUDFLARE) NetworkService_ResolveHost("1.1.1.1");
    else if (cmd == ID_CHANGE_HOST || cmd == ID_ALERT_THRESHOLD_CUSTOM) {
        if (cmd == ID_ALERT_THRESHOLD_CUSTOM) OpenInputDialog(1);
        else OpenInputDialog(0);
    }
}

void Dashboard_Open(HINSTANCE hi);
void GUIManager_ShowStats() { Dashboard_Open(GetModuleHandle(0)); }

LRESULT CALLBACK WindowProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_CLIPBOARDUPDATE && g_ctx.monitorClipboard) {
        if (OpenClipboard(h)) {
            HANDLE hD = GetClipboardData(CF_TEXT);
            if (hD) {
                char* p = (char*)GlobalLock(hD);
                if (p) {
                    std::string s(p); GlobalUnlock(hD);
                    if (s.length() > 0 && s.length() < 128) {
                        bool hasDot = s.find('.') != std::string::npos;
                        bool hasLetter = false; for(char c : s) if(isalpha(c)) hasLetter=true;
                        if (hasDot || (s.length() > 3 && !hasLetter)) FloatingWindow_StartForTarget(s);
                    }
                }
            }
            CloseClipboard();
        }
    }
    static UINT taskbarMsg = RegisterWindowMessageA("TaskbarCreated");
    if (m == taskbarMsg) { Shell_NotifyIconA(NIM_ADD, &g_ctx.nid); return 0; }
    switch (m) {
        case WM_TRAYICON: if (LOWORD(lp) == WM_RBUTTONUP) BuildContextMenu(h); break;
        case WM_TIMER: if (wp == 1001) GUIManager_UpdateTray(); break;
        case WM_CLOSE: DestroyWindow(h); break;
        case WM_DESTROY: g_ctx.running = false; PostQuitMessage(0); break;
        default: return DefWindowProc(h, m, wp, lp);
    }
    return 0;
}

void GUIManager_Init(HINSTANCE hi) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hi;
    wc.lpszClassName = "PingWinMain";
    wc.hIcon = LoadIcon(hi, MAKEINTRESOURCE(IDI_ICON1));
    
    if (!RegisterClassA(&wc)) {
        return;
    }

    g_ctx.hwndMain = CreateWindowExA(0, "PingWinMain", "Pingwin", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hi, 0);
    
    if (!g_ctx.hwndMain) {
        return;
    }

    ShowWindow(g_ctx.hwndMain, SW_HIDE);

    g_ctx.nid.cbSize = sizeof(g_ctx.nid);
    g_ctx.nid.hWnd = g_ctx.hwndMain;
    g_ctx.nid.uID = 1;
    g_ctx.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_ctx.nid.uCallbackMessage = WM_TRAYICON;
    g_ctx.nid.hIcon = LoadIcon(hi, MAKEINTRESOURCE(IDI_ICON1));
    
    Shell_NotifyIconA(NIM_ADD, &g_ctx.nid);
    SetTimer(g_ctx.hwndMain, 1001, 500, 0);
}

void GUIManager_Cleanup() { Shell_NotifyIconA(NIM_DELETE, &g_ctx.nid); }

void GUIManager_UpdateTray() {
    auto r = g_ctx.latestResult.load(); if (!r) return;
    auto s = g_ctx.stats.load(); int p = r->ping;
    if (!g_ctx.showLastPing && s && s->history.size() >= 3) {
        int sum = 0, count = 0;
        for (auto it = s->history.rbegin(); it != s->history.rend() && count < 3; ++it) { if (*it >= 0) { sum += *it; count++; } }
        if (count > 0) p = sum / count;
    }
    g_ctx.nid.hIcon = iconCache.Get(p);
    char t[128]; sprintf(t, "Ping: %dms - %s:%d", p, r->host.c_str(), r->port);
    strncpy(g_ctx.nid.szTip, t, 127); Shell_NotifyIconA(NIM_MODIFY, &g_ctx.nid);
}
