#include "ui_windows.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>
#include <thread>
#include <ctime>

static auto g_dashboardStartTime = std::chrono::steady_clock::now();

static std::string GetUptime() {
    auto now = std::chrono::steady_clock::now();
    auto d = std::chrono::duration_cast<std::chrono::seconds>(now - g_dashboardStartTime).count();
    int h = d / 3600, m = (d % 3600) / 60, s = d % 60;
    char buf[32]; sprintf(buf, "%02d:%02d:%02d", h, m, s);
    return buf;
}

static void DrawDashboard(HWND h, HDC hdc) {
    auto s = g_ctx.stats.load();
    auto t = g_ctx.currentTarget.load();
    if (!s || !t) return;

    RECT rc;
    GetClientRect(h, &rc);

    HDC mdc = CreateCompatibleDC(hdc);
    if (!mdc) return;

    HBITMAP mb = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    if (!mb) {
        DeleteDC(mdc);
        return;
    }
    
    HBITMAP oldBmp = (HBITMAP)SelectObject(mdc, mb);

    COLORREF bg = RGB(32, 34, 45);
    COLORREF cardBg = RGB(42, 44, 55);
    COLORREF textMain = RGB(255, 255, 255);
    COLORREF textSec = RGB(160, 165, 180);

    {
        AppContext::GdiObj<HBRUSH> hbg(CreateSolidBrush(bg));
        FillRect(mdc, &rc, hbg);
    }

    static HFONT fT = CreateFontA(26, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    static HFONT fS = CreateFontA(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    static HFONT fC_L = CreateFontA(12, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    static HFONT fC_V = CreateFontA(36, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");

    SetBkMode(mdc, TRANSPARENT);

    auto Card = [&](int x, int y, int w, int h, const char* label, std::string value, COLORREF accent) {
        {
            AppContext::GdiObj<HBRUSH> cb(CreateSolidBrush(cardBg));
            RECT cr = {x, y, x + w, y + h};
            FillRect(mdc, &cr, cb);
        }
        {
            AppContext::GdiObj<HBRUSH> ab(CreateSolidBrush(accent));
            RECT ar = {x, y, x + w, y + 4};
            FillRect(mdc, &ar, ab);
        }
        
        SetTextColor(mdc, textSec);
        SelectObject(mdc, fC_L);
        TextOutA(mdc, x + 15, y + 15, label, (int)strlen(label));
        
        SetTextColor(mdc, textMain);
        SelectObject(mdc, fC_V);
        TextOutA(mdc, x + 15, y + 35, value.c_str(), (int)value.length());
    };

    double samples = (double)(s->totalSent - s->totalLost);
    double avg = samples > 0 ? s->sumLatency / samples : 0;
    double jitter = samples > 1 ? s->sumJitter / (samples - 1) : 0;
    double loss = s->totalSent > 0 ? (s->totalLost * 100.0 / s->totalSent) : 0;

    double success = s->totalSent > 0 ? (samples * 100.0 / s->totalSent) : 0;
    double stdDev = 0;
    if (samples > 0) {
        double m = s->sumLatency / samples;
        stdDev = sqrt(std::max(0.0, (s->sumSqLatency / samples) - m * m));
    }

    int cw = (rc.right - 80) / 3;
    int ch = 80;

    Card(25, 25, cw, ch, "TOTAL PINGS", std::to_string(s->totalSent), RGB(0, 180, 255));
    
    char ss[16]; 
    sprintf(ss, "%.1f%%", success); 
    Card(40 + cw, 25, cw, ch, "TAXA SUCESSO", ss, RGB(0, 220, 100));
    
    char ls[16]; 
    sprintf(ls, "%.1f%%", loss); 
    Card(55 + cw * 2, 25, cw, ch, "PERDA PACOTES", ls, RGB(255, 60, 80));
    
    char ds[16]; 
    sprintf(ds, "%.1fms", stdDev); 
    Card(25, 115, cw, ch, "DESVIO PADRAO", ds, RGB(160, 100, 255));
    
    char js[16]; 
    sprintf(js, "%.1fms", jitter); 
    Card(40 + cw, 115, cw, ch, "JITTER", js, RGB(255, 180, 50));
    
    Card(55 + cw * 2, 115, cw, ch, "ULTIMO PING", (s->lastRtt < 0 ? "X" : std::to_string(s->lastRtt) + "ms"), RGB(255, 255, 255));

    // Grafico
    RECT grr = {25, 235, rc.right - 25, 365};
    {
        AppContext::GdiObj<HBRUSH> gbb(CreateSolidBrush(RGB(25, 27, 35)));
        FillRect(mdc, &grr, gbb);
    }

    SelectObject(mdc, fC_L);
    SetTextColor(mdc, textSec);
    TextOutA(mdc, 25, 210, "HISTORICO DE LATENCIA", 21);

    if (!s->history.empty()) {
        int mY = std::max(100, s->maxRtt + 50);
        std::vector<POINT> pts;
        pts.push_back({grr.left, grr.bottom});
        
        for (int i = 0; i < (int)s->history.size(); i++) {
            int x = grr.left + (i * (grr.right - grr.left) / 100);
            int v = s->history[i];
            if (v < 0) v = mY;
            int y = grr.bottom - (v * (grr.bottom - grr.top) / mY);
            if (y < grr.top) y = grr.top;
            pts.push_back({x, y});
        }
        
        pts.push_back({grr.left + (int)((s->history.size() - 1) * (grr.right - grr.left) / 100), grr.bottom});
        
        AppContext::GdiObj<HBRUSH> gbru(CreateSolidBrush(RGB(0, 120, 200)));
        AppContext::GdiObj<HPEN> gpn(CreatePen(PS_SOLID, 2, RGB(0, 180, 255)));
        
        HBRUSH oldBrush = (HBRUSH)SelectObject(mdc, gbru);
        HPEN oldPen = (HPEN)SelectObject(mdc, gpn);
        
        Polygon(mdc, pts.data(), (int)pts.size());
        
        for (auto& p : pts) {
            if (p.y < grr.bottom && p.y > grr.top) {
                AppContext::GdiObj<HBRUSH> wh(CreateSolidBrush(RGB(255, 255, 255)));
                RECT pr = {p.x - 2, p.y - 2, p.x + 2, p.y + 2};
                FillRect(mdc, &pr, wh);
            }
        }
        
        SelectObject(mdc, oldBrush);
        SelectObject(mdc, oldPen);
    }

    SelectObject(mdc, fS);
    SetTextColor(mdc, textSec);
    char h1[64], h2[64];
    sprintf(h1, "Alvo: %s", t->host.c_str());
    sprintf(h2, "Sessao: %s", GetUptime().c_str());
    TextOutA(mdc, 25, 375, h1, (int)strlen(h1));
    
    RECT sesR = {25, 375, rc.right - 25, 395};
    DrawTextA(mdc, h2, -1, &sesR, DT_RIGHT | DT_SINGLELINE);

    int by = 410;
    SelectObject(mdc, fC_L);
    SetTextColor(mdc, textSec);
    TextOutA(mdc, 25, by, "LATENCIA", 8);

    auto Bar = [&](int y, const char* l, int val, int maxV, COLORREF c) {
        SelectObject(mdc, fS);
        SetTextColor(mdc, textSec);
        TextOutA(mdc, 100, y, l, (int)strlen(l));
        
        {
            AppContext::GdiObj<HBRUSH> fb(CreateSolidBrush(RGB(45, 47, 58)));
            RECT fr = {150, y + 4, rc.right - 100, y + 18};
            FillRect(mdc, &fr, fb);
        }
        
        int bw = (val * (rc.right - 100 - 150) / std::max(1, maxV));
        {
            AppContext::GdiObj<HBRUSH> bb(CreateSolidBrush(c));
            RECT brr = {150, y + 4, 150 + bw, y + 18};
            FillRect(mdc, &brr, bb);
        }
        
        SetTextColor(mdc, textMain);
        SelectObject(mdc, fC_L);
        char vs[16];
        sprintf(vs, "%dms", val);
        TextOutA(mdc, rc.right - 85, y, vs, (int)strlen(vs));
    };

    int maxL = std::max(100, s->maxRtt);
    Bar(by + 25, "Min", s->minRtt == 2147483647 ? 0 : s->minRtt, maxL, RGB(0, 200, 100));
    Bar(by + 55, "Media", (int)avg, maxL, RGB(0, 150, 255));
    Bar(by + 85, "Max", s->maxRtt, maxL, RGB(255, 60, 80));

    char ftr[128];
    sprintf(ftr, "Amostras: %d | Sucesso: %llu | Falhas: %llu", (int)s->history.size(), s->totalSent - s->totalLost, s->totalLost);
    SelectObject(mdc, fS);
    SetTextColor(mdc, textSec);
    TextOutA(mdc, 25, rc.bottom - 90, ftr, (int)strlen(ftr));

    auto Btn = [&](int x, int y, int w, int h, const char* l, COLORREF c) {
        AppContext::GdiObj<HBRUSH> b(CreateSolidBrush(c));
        RECT r = {x, y, x + w, y + h};
        FillRect(mdc, &r, b);
        SetTextColor(mdc, textMain);
        SelectObject(mdc, fC_L);
        DrawTextA(mdc, l, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    Btn(25, rc.bottom - 65, 140, 45, "Resetar", RGB(220, 50, 70));
    Btn(rc.right - 165, rc.bottom - 65, 140, 45, "Fechar", RGB(0, 120, 255));

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mdc, 0, 0, SRCCOPY);
    
    SelectObject(mdc, oldBmp);
    DeleteObject(mb);
    DeleteDC(mdc);
}

LRESULT CALLBACK DashboardProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_CREATE) SetTimer(h, 1, 750, 0);
    else if (m == WM_TIMER) InvalidateRect(h, 0, 0);
    else if (m == WM_LBUTTONDOWN) {
        int x = LOWORD(lp), y = HIWORD(lp); RECT rc; GetClientRect(h, &rc);
        if (y > rc.bottom-65 && y < rc.bottom-20) {
            if (x > 25 && x < 165) g_ctx.stats.store(std::make_shared<PingStats>());
            else if (x > rc.right-165 && x < rc.right-25) ShowWindow(h, SW_HIDE);
        }
    }
    else if (m == WM_PAINT) { PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps); DrawDashboard(h, hdc); EndPaint(h, &ps); }
    else if (m == WM_CLOSE) ShowWindow(h, SW_HIDE);
    else if (m == WM_DESTROY) g_ctx.hStatsWnd = 0;
    else return DefWindowProc(h, m, wp, lp); return 0;
}

void Dashboard_Open(HINSTANCE hi) {
    if (g_ctx.hStatsWnd) { ShowWindow(g_ctx.hStatsWnd, SW_SHOW); SetForegroundWindow(g_ctx.hStatsWnd); return; }
    WNDCLASSA wc = {0}; 
    wc.lpfnWndProc = DashboardProc; 
    wc.hInstance = hi; 
    wc.lpszClassName = "PingWinStats"; 
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1); 
    wc.hIcon = LoadIcon(hi, MAKEINTRESOURCE(IDI_ICON1));
    RegisterClassA(&wc);
    
    g_ctx.hStatsWnd = CreateWindowExA(WS_EX_TOPMOST, "PingWinStats", "Pingwin - Dashboard", WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE, 100, 100, 620, 660, 0, 0, hi, 0);
}

struct TempPing { std::string time; int rtt; };
struct FloatingWindowData {
    std::deque<TempPing> history;
    std::string host;
    std::atomic<bool> pinging{false};
    HFONT font = 0;
    int elapsed = 0;
    std::mutex mtx;

    FloatingWindowData(const std::string& h) : host(h) {}
    ~FloatingWindowData() {
        if (font) DeleteObject(font);
    }
};

static LRESULT CALLBACK FloatingWndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_NCCREATE) {
        CREATESTRUCTA* cs = (CREATESTRUCTA*)lp;
        auto data = new std::shared_ptr<FloatingWindowData>(new FloatingWindowData((const char*)cs->lpCreateParams));
        SetWindowLongPtr(h, GWLP_USERDATA, (LONG_PTR)data);
        return TRUE;
    }

    auto dataPtr = (std::shared_ptr<FloatingWindowData>*)GetWindowLongPtr(h, GWLP_USERDATA);
    if (!dataPtr) return DefWindowProc(h, m, wp, lp);
    auto data = *dataPtr;

    switch (m) {
        case WM_CREATE:
            SetLayeredWindowAttributes(h, 0, 220, LWA_ALPHA);
            SetTimer(h, 1, 1000, 0);
            data->font = CreateFontA(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, "Consolas");
            break;

        case WM_LBUTTONDOWN:
            PostMessage(h, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            break;

        case WM_TIMER:
            if (wp == 1) {
                data->elapsed++;
                if (!data->pinging.load()) {
                    data->pinging.store(true);
                    std::thread([data, hW = h]() {
                        HANDLE hI = IcmpCreateFile();
                        if (hI == INVALID_HANDLE_VALUE) {
                            data->pinging.store(false);
                            return;
                        }

                        IN_ADDR ia;
                        bool ok = false;
                        if (InetPtonA(AF_INET, data->host.c_str(), &ia) == 1) {
                            ok = true;
                        } else {
                            addrinfo ht = {0};
                            ht.ai_family = AF_INET;
                            addrinfo* r = 0;
                            if (getaddrinfo(data->host.c_str(), 0, &ht, &r) == 0) {
                                ia = ((sockaddr_in*)r->ai_addr)->sin_addr;
                                freeaddrinfo(r);
                                ok = true;
                            }
                        }

                        int p = -1;
                        if (ok) {
                            char rb[sizeof(ICMP_ECHO_REPLY) + 32];
                            if (IcmpSendEcho(hI, ia.s_addr, (LPVOID)"ISP", 3, 0, rb, sizeof(rb), 1000)) {
                                p = ((PICMP_ECHO_REPLY)rb)->RoundTripTime;
                            }
                        }
                        IcmpCloseHandle(hI);

                        time_t n = time(0);
                        struct tm l;
                        localtime_s(&l, &n);
                        char ts[16];
                        strftime(ts, 16, "%H:%M:%S", &l);
                        
                        {
                            std::lock_guard<std::mutex> lock(data->mtx);
                            data->history.push_back({ts, p});
                            if (data->history.size() > 3) data->history.pop_front();
                        }
                        
                        InvalidateRect(hW, 0, 0);
                        data->pinging.store(false);
                    }).detach();
                }

                if (g_ctx.autoCloseSeconds > 0 && data->elapsed >= g_ctx.autoCloseSeconds) {
                    DestroyWindow(h);
                }
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(h, &ps);
            RECT r; GetClientRect(h, &r);
            {
                AppContext::GdiObj<HBRUSH> br(CreateSolidBrush(RGB(10, 10, 10)));
                FillRect(hdc, &r, br);
            }
            SetTextColor(hdc, RGB(0, 255, 100));
            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, data->font);
            
            int y = 5;
            {
                std::lock_guard<std::mutex> lock(data->mtx);
                for (auto& item : data->history) {
                    char b[128];
                    if (item.rtt < 0) snprintf(b, 128, "[%s] ping: %s - TIMEOUT", item.time.c_str(), data->host.c_str());
                    else snprintf(b, 128, "[%s] ping: %s - %dms", item.time.c_str(), data->host.c_str(), item.rtt);
                    TextOutA(hdc, 10, y, b, (int)strlen(b));
                    y += 18;
                }
            }
            EndPaint(h, &ps);
            break;
        }

        case WM_NCDESTROY:
            delete dataPtr;
            SetWindowLongPtr(h, GWLP_USERDATA, 0);
            return DefWindowProc(h, m, wp, lp);

        case WM_DESTROY:
            g_ctx.hFloatWnd = 0;
            break;

        default:
            return DefWindowProc(h, m, wp, lp);
    }
    return 0;
}

void FloatingWindow_StartForTarget(const std::string& host) {
    if (g_ctx.hFloatWnd) DestroyWindow(g_ctx.hFloatWnd);
    HINSTANCE hi = GetModuleHandle(0);
    WNDCLASSA wc = {0}; 
    wc.lpfnWndProc = FloatingWndProc; 
    wc.hInstance = hi; 
    wc.lpszClassName = "PingWinFloat"; 
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); 
    wc.hIcon = LoadIcon(hi, MAKEINTRESOURCE(IDI_ICON1));
    RegisterClassA(&wc);
    
    g_ctx.hFloatWnd = CreateWindowExA(WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TOOLWINDOW, "PingWinFloat", "PingWinFloat", WS_POPUP|WS_VISIBLE, 100, 100, 350, 65, 0, 0, hi, (LPVOID)host.c_str());
}

static HWND g_hLogEditCtrl = NULL;

static LRESULT CALLBACK LogWindowProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
        case WM_CREATE: {
            g_hLogEditCtrl = CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0, 0, 0, h, NULL, (HINSTANCE)GetWindowLongPtr(h, GWLP_HINSTANCE), NULL);
            SendMessage(g_hLogEditCtrl, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            SetTimer(h, 1, 1000, NULL);
            break;
        }
        case WM_SIZE: {
            MoveWindow(g_hLogEditCtrl, 0, 0, LOWORD(lp), HIWORD(lp), TRUE);
            break;
        }
        case WM_TIMER: {
            std::shared_ptr<PingStats> s;
            {
                std::lock_guard<std::mutex> lock(g_ctx.statsMtx);
                auto p = g_ctx.stats.load();
                if (p) s = std::make_shared<PingStats>(*p);
            }
            if (s) {
                std::string allLogs = "";
                for (auto& ev : s->eventLog) {
                    allLogs += "[" + ev.timestamp + "] " + ev.description + "\r\n";
                }
                static size_t lastSize = 0;
                if (s->eventLog.size() != lastSize) {
                    SetWindowTextA(g_hLogEditCtrl, allLogs.c_str());
                    SendMessage(g_hLogEditCtrl, WM_VSCROLL, SB_BOTTOM, 0);
                    lastSize = s->eventLog.size();
                }
            }
            break;
        }
        case WM_CLOSE:
            ShowWindow(h, SW_HIDE);
            return 0;
        case WM_DESTROY:
            g_ctx.hLogWnd = NULL;
            break;
    }
    return DefWindowProc(h, m, wp, lp);
}

void LogWindow_Open(HINSTANCE hi) {
    if (g_ctx.hLogWnd) {
        ShowWindow(g_ctx.hLogWnd, SW_SHOW);
        SetForegroundWindow(g_ctx.hLogWnd);
        return;
    }

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = LogWindowProc;
    wc.hInstance = hi;
    wc.lpszClassName = "PingWinLogWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(hi, MAKEINTRESOURCE(IDI_ICON1));
    RegisterClassA(&wc);

    g_ctx.hLogWnd = CreateWindowExA(WS_EX_TOPMOST, "PingWinLogWindow", "Pingwin - Logs de Eventos", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 400, NULL, NULL, hi, NULL);
    ShowWindow(g_ctx.hLogWnd, SW_SHOW);
}
