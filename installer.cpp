#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#include <winreg.h>
#include <shellapi.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <cwctype>
#include <cstdlib>

#include "resource.h"

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------
#define WM_APP_INSTALL_DONE  (WM_APP + 1)
#define WM_APP_PROGRESS      (WM_APP + 2)
#define IDC_EDIT_DIR         1001
#define IDT_PROGRESS         1002

// PCL2 下载地址
static const wchar_t* kPclUrl =
    L"https://pclpcl2.7677677.xyz/file/PCL2.exe";
// 下载到安装目录后的文件名
static const wchar_t* kPclFileName = L"PCL2.EXE";

#define STEP_COUNT 4

#define CLIENT_W 840
#define CLIENT_H 560
#define HEADER_H 60
#define SIDEBAR_W 200

// 配色：强调色灰色、背景黑色、字体白色
static const COLORREF CLR_HEADER      = RGB(0, 0, 0);         // #000000 黑色
static const COLORREF CLR_SIDEBAR     = RGB(12, 12, 12);      // 近黑色
static const COLORREF CLR_ACCENT      = RGB(128, 128, 128);   // 灰色强调色
static const COLORREF CLR_ACCENT_DARK = RGB(88, 88, 88);
static const COLORREF CLR_CONTENT     = RGB(0, 0, 0);         // 黑色背景
static const COLORREF CLR_WHITE       = RGB(255, 255, 255);
static const COLORREF CLR_TEXT        = RGB(255, 255, 255);   // 白色字体
static const COLORREF CLR_MUTED       = RGB(170, 170, 170);
static const COLORREF CLR_SIDE_TEXT   = RGB(180, 180, 180);
static const COLORREF CLR_BORDER      = RGB(90, 90, 90);
static const COLORREF CLR_OK          = RGB(34, 197, 94);
static const COLORREF CLR_FAIL        = RGB(239, 68, 68);
static const COLORREF CLR_TRACK       = RGB(45, 45, 45);

// ---------------------------------------------------------------------------
// 全局状态
// ---------------------------------------------------------------------------
static HINSTANCE g_hInst = nullptr;

// 字体
static HFONT g_fHeader = nullptr; // 顶栏标题
static HFONT g_fTitle  = nullptr; // 内容大标题
static HFONT g_fBody   = nullptr; // 正文
static HFONT g_fSide   = nullptr; // 侧边栏步骤

// 当前步骤
static int g_step = 0;

// 下载目录
static std::wstring g_installDir;

// 安装/下载状态
static bool g_installing = false;
static bool g_installSuccess = false;
static bool g_launchOnFinish = true; // 完成后是否运行 PCL2
static std::wstring g_installMessage;
static int g_progressPos = 0;
static int g_downloadPercent = -1; // 0..100；-1 表示不确定进度（滚动条）

// 命令行标志
static bool g_silent = false;       // --silent：完全不打开窗口，静默安装
static bool g_basicInstall = false; // --basicInstall：自动安装，完成后直接关闭
static bool g_openOnFinish = false; // --open：安装完成后启动 PCL2

// 背景音乐
static bool g_audioStarted = false;
static volatile bool g_stopAudio = false;

// 控件句柄
static HWND g_editDir = nullptr;

// 布局矩形
static RECT g_rcSteps[STEP_COUNT];
static RECT g_rcPrev;
static RECT g_rcNext;
static RECT g_rcBrowse;
static RECT g_rcEdit;
static RECT g_rcHeader;
static RECT g_rcSidebar;
static RECT g_rcContent;
static RECT g_rcLaunchCheck; // “完成后运行 PCL2”复选框可点击区域

// ---------------------------------------------------------------------------
// 辅助函数
// ---------------------------------------------------------------------------
static HFONT CreateAppFont(int height, int weight, const wchar_t* face)
{
    return CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

static void FillColor(HDC dc, const RECT& rc, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &rc, b);
    DeleteObject(b);
}

static void FillEllipse(HDC dc, const RECT& rc, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    HPEN p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, p);
    Ellipse(dc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

static void DrawStr(HDC dc, const wchar_t* s, RECT rc, UINT fmt,
                    HFONT f, COLORREF c)
{
    HFONT old = (HFONT)SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s, -1, &rc, fmt);
    SelectObject(dc, old);
}

// 计算布局矩形
static void ComputeLayout(int cw, int ch)
{
    g_rcHeader  = { 0, 0, cw, HEADER_H };
    g_rcSidebar = { 0, HEADER_H, SIDEBAR_W, ch };
    g_rcContent = { SIDEBAR_W, HEADER_H, cw, ch };

    for (int i = 0; i < STEP_COUNT; i++) {
        int y = HEADER_H + 20 + i * 52;
        g_rcSteps[i] = { 16, y, SIDEBAR_W - 16, y + 44 };
    }

    int margin = 28;
    int bw = 130, bh = 44;
    g_rcNext = { g_rcContent.right - margin - bw,
                 g_rcContent.bottom - margin - bh,
                 g_rcContent.right - margin,
                 g_rcContent.bottom - margin };
    g_rcPrev = { g_rcNext.left - 12 - 100, g_rcNext.top, g_rcNext.left - 12, g_rcNext.bottom };

    g_rcEdit   = { g_rcContent.left + 32, g_rcContent.top + 96,
                   g_rcContent.right - 32 - 12 - 100, g_rcContent.top + 96 + 34 };
    g_rcBrowse = { g_rcEdit.right + 12, g_rcEdit.top, g_rcEdit.right + 12 + 100, g_rcEdit.bottom };
}

// ---------------------------------------------------------------------------
// 命令行解析
// ---------------------------------------------------------------------------
static void ParseCommandLine()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return;

    for (int i = 1; i < argc; i++) {
        if (lstrcmpiW(argv[i], L"--silent") == 0 ||
            lstrcmpiW(argv[i], L"/silent") == 0 ||
            lstrcmpiW(argv[i], L"/slient") == 0) {
            g_silent = true;
        } else if (lstrcmpiW(argv[i], L"--basicInstall") == 0 ||
                   lstrcmpiW(argv[i], L"/basicInstall") == 0) {
            g_basicInstall = true;
        } else if (lstrcmpiW(argv[i], L"--open") == 0 ||
                   lstrcmpiW(argv[i], L"/open") == 0) {
            g_openOnFinish = true;
        }
    }
    LocalFree(argv);
}

// ---------------------------------------------------------------------------
// 背景音乐：从嵌入的 MP3 资源流式解码，通过 waveOut 循环播放
// ---------------------------------------------------------------------------
static bool GetResourceBytes(int id, std::vector<uint8_t>& out)
{
    HRSRC h = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!h) return false;
    HGLOBAL g = LoadResource(nullptr, h);
    if (!g) return false;
    const void* p = LockResource(g);
    DWORD size = SizeofResource(nullptr, h);
    if (!p || size == 0) return false;
    out.assign((const uint8_t*)p, (const uint8_t*)p + size);
    return true;
}

static DWORD WINAPI AudioThreadProc(LPVOID)
{
    std::vector<uint8_t> mp3;
    if (!GetResourceBytes(IDR_AUDIO, mp3)) return 0;

    mp3dec_t dec;
    mp3dec_init(&dec);

    std::vector<mp3d_sample_t> pcm;
    pcm.reserve(mp3.size() * 3);
    int channels = 0;
    int hz = 0;
    mp3d_sample_t frame[MINIMP3_MAX_SAMPLES_PER_FRAME];
    size_t off = 0;
    for (;;) {
        mp3dec_frame_info_t fi;
        int s = mp3dec_decode_frame(&dec, mp3.data() + off,
                                    (int)(mp3.size() - off), frame, &fi);
        if (s <= 0) break;
        off += (size_t)fi.frame_bytes;
        if (channels == 0) { channels = fi.channels; hz = fi.hz; }
        pcm.insert(pcm.end(), frame, frame + (size_t)s * fi.channels);
    }
    if (channels == 0 || pcm.empty()) return 0;

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = (WORD)channels;
    wfx.nSamplesPerSec = hz;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (WORD)(channels * 2);
    wfx.nAvgBytesPerSec = (DWORD)(hz * wfx.nBlockAlign);

    HWAVEOUT hwo = nullptr;
    if (waveOutOpen(&hwo, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        return 0;

    const int NUM_BUFS = 4;
    const int BLOCK_SAMPLES = hz / 10;
    const size_t totalFrames = pcm.size() / channels;

    struct Block {
        std::vector<mp3d_sample_t> buf;
        WAVEHDR hdr;
    } blocks[NUM_BUFS];
    for (auto& b : blocks) b.buf.resize((size_t)BLOCK_SAMPLES * channels);

    size_t pos = 0;
    auto fillBlock = [&](mp3d_sample_t* dst) {
        size_t rest = totalFrames - pos;
        size_t firstN = rest < (size_t)BLOCK_SAMPLES ? rest : (size_t)BLOCK_SAMPLES;
        memcpy(dst, pcm.data() + pos * channels,
               firstN * channels * sizeof(mp3d_sample_t));
        pos += firstN;
        if (firstN < (size_t)BLOCK_SAMPLES) {
            size_t secondN = (size_t)BLOCK_SAMPLES - firstN;
            memcpy(dst + firstN * channels, pcm.data(),
                   secondN * channels * sizeof(mp3d_sample_t));
            pos = secondN;
        }
    };

    for (int i = 0; i < NUM_BUFS; i++) {
        fillBlock(blocks[i].buf.data());
        ZeroMemory(&blocks[i].hdr, sizeof(WAVEHDR));
        blocks[i].hdr.lpData = (LPSTR)blocks[i].buf.data();
        blocks[i].hdr.dwBufferLength = (DWORD)(BLOCK_SAMPLES * channels * sizeof(mp3d_sample_t));
        if (waveOutPrepareHeader(hwo, &blocks[i].hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            break;
        if (waveOutWrite(hwo, &blocks[i].hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            break;
    }

    int cur = 0;
    while (!g_stopAudio) {
        Block& blk = blocks[cur % NUM_BUFS];
        while (!(blk.hdr.dwFlags & WHDR_DONE) && !g_stopAudio) Sleep(10);
        if (g_stopAudio) break;
        waveOutUnprepareHeader(hwo, &blk.hdr, sizeof(WAVEHDR));
        fillBlock(blk.buf.data());
        ZeroMemory(&blk.hdr, sizeof(WAVEHDR));
        blk.hdr.lpData = (LPSTR)blk.buf.data();
        blk.hdr.dwBufferLength = (DWORD)(BLOCK_SAMPLES * channels * sizeof(mp3d_sample_t));
        waveOutPrepareHeader(hwo, &blk.hdr, sizeof(WAVEHDR));
        waveOutWrite(hwo, &blk.hdr, sizeof(WAVEHDR));
        cur++;
    }

    waveOutReset(hwo);
    for (int i = 0; i < NUM_BUFS; i++) {
        waveOutUnprepareHeader(hwo, &blocks[i].hdr, sizeof(WAVEHDR));
    }
    waveOutClose(hwo);
    return 0;
}

// 进入「下载」页时调用；进程内仅启动一次
static void StartInstallAudio()
{
    if (g_audioStarted) return;
    g_audioStarted = true;
    g_stopAudio = false;
    CreateThread(nullptr, 0, AudioThreadProc, nullptr, 0, nullptr);
}

// ---------------------------------------------------------------------------
// 下载与配置
// ---------------------------------------------------------------------------
// 确保目录存在（含多级创建）
static bool EnsureDir(const std::wstring& dir)
{
    if (dir.empty()) return false;
    if (CreateDirectoryW(dir.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS)
        return true;
    return false;
}

// 从指定 URL 下载 PCL2 到 destPath，伪装 curl UA 绕过 WAF；上报下载进度
static bool DownloadPCL2(HWND hwnd, const wchar_t* url, const wchar_t* destPath)
{
    // 解析 URL：host 与 path(?query)
    std::wstring u = url;
    size_t sp = u.find(L"://");
    std::wstring rest = (sp == std::wstring::npos) ? u : u.substr(sp + 3);
    size_t slash = rest.find(L'/');
    std::wstring host = (slash == std::wstring::npos) ? rest : rest.substr(0, slash);
    std::wstring obj  = (slash == std::wstring::npos) ? L"/" : rest.substr(slash);

    HINTERNET hNet = InternetOpenW(L"curl/8.23.0",
                                   INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hNet) return false;

    HINTERNET hCon = InternetConnectW(hNet, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT,
                                      nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hCon) { InternetCloseHandle(hNet); return false; }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE;
    HINTERNET hReq = HttpOpenRequestW(hCon, L"GET", obj.c_str(), nullptr, nullptr,
                                      nullptr, flags, 0);
    if (!hReq) {
        InternetCloseHandle(hCon);
        InternetCloseHandle(hNet);
        return false;
    }

    // 伪装 User-Agent 以绕过 SafeLineWAF
    const wchar_t* headers = L"User-Agent: curl/8.23.0\r\n";
    if (!HttpSendRequestW(hReq, headers, (DWORD)-1, nullptr, 0)) {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hCon);
        InternetCloseHandle(hNet);
        return false;
    }

    // HTTP 状态码与总大小
    DWORD status = 0;
    ULONGLONG total = 0;
    {
        wchar_t buf[64] = { 0 };
        DWORD sz = sizeof(buf), index = 0;
        if (HttpQueryInfoW(hReq, HTTP_QUERY_STATUS_CODE, buf, &sz, &index))
            status = wcstoul(buf, nullptr, 10);

        wchar_t lenBuf[64] = { 0 };
        DWORD lenSize = sizeof(lenBuf);
        index = 0;
        if (HttpQueryInfoW(hReq, HTTP_QUERY_CONTENT_LENGTH, lenBuf, &lenSize, &index))
            total = wcstoull(lenBuf, nullptr, 10);
    }
    if (status >= 400) {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hCon);
        InternetCloseHandle(hNet);
        return false;
    }

    HANDLE hf = CreateFileW(destPath, GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hCon);
        InternetCloseHandle(hNet);
        return false;
    }

    BYTE buf[64 * 1024];
    bool ok = true;
    ULONGLONG got = 0;
    for (;;) {
        DWORD read = 0;
        if (!InternetReadFile(hReq, buf, sizeof(buf), &read)) { ok = false; break; }
        if (read == 0) break;
        DWORD written = 0;
        if (!WriteFile(hf, buf, read, &written, nullptr) || written != read) {
            ok = false;
            break;
        }
        got += read;
        if (total > 0) {
            int pct = (int)(got * 100 / total);
            if (pct > 100) pct = 100;
            if (hwnd) PostMessageW(hwnd, WM_APP_PROGRESS, 0, (LPARAM)pct);
        }
    }

    CloseHandle(hf);
    InternetCloseHandle(hReq);
    InternetCloseHandle(hCon);
    InternetCloseHandle(hNet);

    if (!ok) {
        DeleteFileW(destPath);
        return false;
    }
    return true;
}

// 在安装目录的 PCL 子目录写入 Setup.ini，内容为 "SystemSystemUpdate:3"
static bool WritePclConfig()
{
    std::wstring dir = g_installDir + L"\\PCL";
    if (!EnsureDir(dir)) return false;

    std::wstring ini = dir + L"\\Setup.ini";
    HANDLE hf = CreateFileW(ini.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return false;

    const char content[] = "SystemSystemUpdate:3";
    DWORD written = 0;
    BOOL r = WriteFile(hf, content, sizeof(content) - 1, &written, nullptr);
    CloseHandle(hf);
    return r && written == sizeof(content) - 1;
}

// 写入注册表安装位置（机器级），供后续更新/重装时定位并覆盖文件
static void WriteInstallRegistry()
{
    HKEY hk = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\PCL2", 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hk, L"InstallLocation", 0, REG_SZ,
                       (const BYTE*)g_installDir.c_str(),
                       (DWORD)((g_installDir.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hk);
    }
}

// 创建快捷方式 .lnk（需在已初始化 COM 的线程上调用）
static bool CreateLnk(const wchar_t* lnkPath, const wchar_t* target, const wchar_t* workDir)
{
    bool ok = false;
    IShellLinkW* psl = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_IShellLinkW, (void**)&psl))) {
        psl->SetPath(target);
        psl->SetWorkingDirectory(workDir);
        psl->SetDescription(L"PCL2");
        psl->SetIconLocation(target, 0);

        // 兼容性：以管理员身份运行（LinkFlags 的 RunAsUser 位 0x2000）
        IShellLinkDataList* psdl = nullptr;
        if (SUCCEEDED(psl->QueryInterface(IID_IShellLinkDataList, (void**)&psdl))) {
            DWORD flags = 0;
            psdl->GetFlags(&flags);
            psdl->SetFlags(flags | 0x2000);
            psdl->Release();
        }

        IPersistFile* ppf = nullptr;
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
            if (SUCCEEDED(ppf->Save(lnkPath, TRUE))) ok = true;
            ppf->Release();
        }
        psl->Release();
    }
    return ok;
}

// 安装完成后：写注册表安装位置 + 生成桌面/开始菜单快捷方式（尽力而为，失败不阻断）
static void PostInstall()
{
    WriteInstallRegistry();

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    std::wstring target = g_installDir + L"\\PCL2.EXE";

    wchar_t desktop[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_DESKTOPDIRECTORY, nullptr, 0, desktop))) {
        CreateLnk((std::wstring(desktop) + L"\\PCL2.lnk").c_str(),
                  target.c_str(), g_installDir.c_str());
    }

    wchar_t programs[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, programs))) {
        CreateLnk((std::wstring(programs) + L"\\PCL2.lnk").c_str(),
                  target.c_str(), g_installDir.c_str());
    }

    CoUninitialize();
}

// 启动 PCL2.EXE
static void LaunchPCL2()
{
    std::wstring target = g_installDir + L"\\PCL2.EXE";
    ShellExecuteW(nullptr, L"open", target.c_str(), nullptr,
                  g_installDir.c_str(), SW_SHOWNORMAL);
}

static DWORD WINAPI InstallThreadProc(LPVOID lp)
{
    HWND hwnd = (HWND)lp;

    if (!EnsureDir(g_installDir)) {
        PostMessageW(hwnd, WM_APP_INSTALL_DONE, 0, 2);
        return 0;
    }

    g_installMessage = L"正在下载 PCL2…";

    std::wstring dest = g_installDir + L"\\" + kPclFileName;
    if (!DownloadPCL2(hwnd, kPclUrl, dest.c_str())) {
        PostMessageW(hwnd, WM_APP_INSTALL_DONE, 0, 1);
        return 0;
    }

    if (!WritePclConfig()) {
        PostMessageW(hwnd, WM_APP_INSTALL_DONE, 0, 2);
        return 0;
    }

    PostInstall();

    PostMessageW(hwnd, WM_APP_INSTALL_DONE, 0, 0);
    return 0;
}

// 静默安装：不创建窗口，直接下载 + 写配置 + 收尾
// 返回码：0 成功；1 目录创建失败；2 下载失败；3 写配置失败
static int RunSilentInstall()
{
    if (!EnsureDir(g_installDir)) return 1;

    std::wstring dest = g_installDir + L"\\" + kPclFileName;
    if (!DownloadPCL2(nullptr, kPclUrl, dest.c_str())) return 2;

    if (!WritePclConfig()) return 3;

    PostInstall();
    return 0;
}

// ---------------------------------------------------------------------------
// 步骤切换
// ---------------------------------------------------------------------------
static void CaptureEditDir()
{
    if (g_editDir) {
        wchar_t buf[MAX_PATH] = { 0 };
        GetWindowTextW(g_editDir, buf, MAX_PATH);
        if (buf[0]) g_installDir = buf;
        DestroyWindow(g_editDir);
        g_editDir = nullptr;
    }
}

static void SetStep(HWND hwnd, int step)
{
    if (step < 0 || step >= STEP_COUNT) return;
    if (g_installing && step != 3) return; // 安装中禁止跳转（3 = 完成）

    if (g_step == 1) CaptureEditDir();
    if (g_step == 2) KillTimer(hwnd, IDT_PROGRESS);

    g_step = step;

    if (g_step == 1) {
        g_editDir = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                    g_rcEdit.left, g_rcEdit.top,
                                    g_rcEdit.right - g_rcEdit.left,
                                    g_rcEdit.bottom - g_rcEdit.top,
                                    hwnd, (HMENU)IDC_EDIT_DIR, g_hInst, nullptr);
        SendMessageW(g_editDir, WM_SETFONT, (WPARAM)g_fBody, TRUE);
        SetWindowTextW(g_editDir, g_installDir.c_str());
        SetFocus(g_editDir);
    }
    if (g_step == 2) {
        g_progressPos = 0;
        g_downloadPercent = -1;
        StartInstallAudio(); // 进入「下载」页时开始播放背景音乐
    }

    InvalidateRect(hwnd, nullptr, TRUE);
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------
static void DrawButton(HDC dc, const RECT& rc, const wchar_t* text,
                       bool primary, bool disabled)
{
    COLORREF bg = primary ? (disabled ? CLR_ACCENT_DARK : CLR_ACCENT) : CLR_CONTENT;
    if (!primary) {
        FillColor(dc, rc, bg);
        HPEN p = CreatePen(PS_SOLID, 1, CLR_BORDER);
        HGDIOBJ op = SelectObject(dc, p);
        HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(dc, op);
        SelectObject(dc, ob);
        DeleteObject(p);
    } else {
        FillColor(dc, rc, bg);
    }
    RECT tr = rc;
    tr.left += 8; tr.right -= 8;
    tr.top += 2; tr.bottom -= 2;
    DrawStr(dc, text, tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE, g_fBody, CLR_WHITE);
}

static void DrawHeader(HDC dc)
{
    FillColor(dc, g_rcHeader, CLR_HEADER);
    RECT line = { g_rcHeader.left, g_rcHeader.bottom - 2, g_rcHeader.right, g_rcHeader.bottom };
    FillColor(dc, line, CLR_ACCENT);

    RECT rc = g_rcHeader;
    rc.left += 20;
    DrawStr(dc, L"PCL2 下载器", rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_fHeader, CLR_WHITE);
    RECT rsub = g_rcHeader;
    rsub.left += 20 + 10 + 180;
    DrawStr(dc, L"下载向导", rsub, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_fSide, CLR_SIDE_TEXT);
}

static void DrawSidebar(HDC dc)
{
    FillColor(dc, g_rcSidebar, CLR_SIDEBAR);

    // 右侧分隔线
    RECT sep = { g_rcSidebar.right - 1, g_rcSidebar.top, g_rcSidebar.right, g_rcSidebar.bottom };
    FillColor(dc, sep, CLR_BORDER);

    static const wchar_t* names[STEP_COUNT] = {
        L"开始", L"选择位置", L"下载", L"完成"
    };

    for (int i = 0; i < STEP_COUNT; i++) {
        RECT rc = g_rcSteps[i];
        if (i == g_step) {
            FillColor(dc, rc, CLR_ACCENT);
            RECT arrow = { rc.left + 14, rc.top, rc.left + 14 + 20, rc.bottom };
            DrawStr(dc, L">", arrow, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_fBody, CLR_WHITE);
            RECT rt = { arrow.right, rc.top, rc.right, rc.bottom };
            DrawStr(dc, names[i], rt, DT_CENTER | DT_VCENTER | DT_SINGLELINE, g_fBody, CLR_WHITE);
        } else {
            DrawStr(dc, names[i], rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE, g_fBody, CLR_SIDE_TEXT);
        }
    }
}

static void DrawStep0(HDC dc)
{
    RECT rc = g_rcContent;
    rc.left += 32; rc.right -= 32; rc.top += 40;
    DrawStr(dc, L"欢迎使用 PCL2 下载器", rc, DT_LEFT | DT_SINGLELINE, g_fTitle, CLR_TEXT);

    RECT body = rc;
    body.top += 60;
    body.bottom += 220;
    DrawStr(dc,
            L"本向导将引导您下载并准备 PCL2（Plain Craft Launcher 2）。\n\n"
            L"过程中将依次执行：\n"
            L"  1. 选择下载位置\n"
            L"  2. 下载 PCL2\n"
            L"  3. 完成\n\n"
            L"请单击“下一步”继续。",
            body, DT_LEFT | DT_WORDBREAK, g_fBody, CLR_MUTED);
}

static void DrawStep1(HDC dc)
{
    RECT rc = g_rcContent;
    rc.left += 32; rc.right -= 32; rc.top += 40;
    DrawStr(dc, L"选择下载位置", rc, DT_LEFT | DT_SINGLELINE, g_fTitle, CLR_TEXT);

    RECT label = { g_rcEdit.left, g_rcEdit.top - 34, g_rcEdit.right, g_rcEdit.top };
    DrawStr(dc, L"下载目录：", label, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_fBody, CLR_TEXT);

    RECT hint = { g_rcEdit.left, g_rcEdit.bottom + 12, g_rcContent.right - 32, g_rcEdit.bottom + 60 };
    DrawStr(dc, L"PCL2.EXE 将被下载到上述目录，并自动禁止更新。\r\n操你妈的中国家长！",
            hint, DT_LEFT | DT_WORDBREAK, g_fSide, CLR_MUTED);
}

static void DrawStep2(HDC dc)
{
    RECT rc = g_rcContent;
    rc.left += 32; rc.right -= 32; rc.top += 40;

    if (g_installing) {
        DrawStr(dc, L"正在下载 PCL2…", rc, DT_LEFT | DT_SINGLELINE, g_fTitle, CLR_TEXT);

        RECT track = { g_rcContent.left + 32, g_rcContent.top + 150,
                       g_rcContent.right - 32, g_rcContent.top + 164 };
        FillColor(dc, track, CLR_TRACK);

        if (g_downloadPercent >= 0) {
            int tw = track.right - track.left;
            int fw = tw * g_downloadPercent / 100;
            if (fw > 0) {
                RECT fill = { track.left, track.top, track.left + fw, track.bottom };
                FillColor(dc, fill, CLR_ACCENT);
            }
        } else {
            int w = 120;
            int max = (track.right - track.left) - w;
            int period = max * 2;
            int r = g_progressPos % period;
            int offset = (r <= max) ? r : (period - r);
            RECT block = { track.left + offset, track.top,
                           track.left + offset + w, track.bottom };
            FillColor(dc, block, CLR_ACCENT);
        }

        RECT status = { g_rcContent.left + 32, track.bottom + 20,
                        g_rcContent.right - 32, track.bottom + 60 };
        std::wstring msg = g_installMessage;
        if (g_downloadPercent >= 0) {
            wchar_t pct[32];
            swprintf(pct, 32, L"已下载 %d%%", g_downloadPercent);
            if (!msg.empty()) msg += L"\n";
            msg += pct;
        }
        DrawStr(dc, msg.c_str(), status, DT_LEFT | DT_WORDBREAK, g_fBody, CLR_MUTED);
    } else {
        DrawStr(dc, L"准备下载", rc, DT_LEFT | DT_SINGLELINE, g_fTitle, CLR_TEXT);

        RECT info = { g_rcContent.left + 32, g_rcContent.top + 70,
                      g_rcContent.right - 32, g_rcContent.top + 200 };
        wchar_t buf[512];
        swprintf(buf, 512,
                 L"即将下载 PCL2（Plain Craft Launcher 2）。\n\n"
                 L"下载目录：%s\n\n"
                 L"点击“开始下载”即可开始。",
                 g_installDir.c_str());
        DrawStr(dc, buf, info, DT_LEFT | DT_WORDBREAK, g_fBody, CLR_MUTED);
    }
}

static void DrawStep3(HDC dc)
{
    RECT rc = g_rcContent;
    rc.left += 32; rc.right -= 32; rc.top += 60;

    RECT icon = { g_rcContent.left + 32, g_rcContent.top + 60,
                  g_rcContent.left + 32 + 72, g_rcContent.top + 60 + 72 };

    COLORREF ic = g_installSuccess ? CLR_OK : CLR_FAIL;
    const wchar_t* mark = g_installSuccess ? L"\u2713" : L"\u2717";
    const wchar_t* title = g_installSuccess ? L"下载完成" : L"下载失败";

    FillEllipse(dc, icon, ic);
    RECT m = icon;
    m.left += 12; m.right -= 12; m.top += 6; m.bottom -= 6;
    DrawStr(dc, mark, m, DT_CENTER | DT_VCENTER | DT_SINGLELINE, g_fHeader, CLR_WHITE);

    RECT tr = { icon.right + 24, icon.top, g_rcContent.right - 32, icon.top + 40 };
    DrawStr(dc, title, tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_fTitle, CLR_TEXT);

    RECT body = { tr.left, tr.bottom + 4, g_rcContent.right - 32, tr.bottom + 120 };
    DrawStr(dc, g_installMessage.c_str(), body, DT_LEFT | DT_WORDBREAK, g_fBody, CLR_MUTED);

    // “完成后运行 PCL2”复选框（仅成功时显示）
    if (g_installSuccess) {
        RECT box = { g_rcContent.left + 32, g_rcNext.top + 12,
                     g_rcContent.left + 32 + 20, g_rcNext.top + 32 };
        FillColor(dc, box, CLR_CONTENT);
        HPEN p = CreatePen(PS_SOLID, 1, g_launchOnFinish ? CLR_ACCENT : CLR_MUTED);
        HGDIOBJ op = SelectObject(dc, p);
        HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, box.left, box.top, box.right, box.bottom);
        SelectObject(dc, op);
        SelectObject(dc, ob);
        DeleteObject(p);
        if (g_launchOnFinish) {
            RECT mk = { box.left + 2, box.top + 1, box.right - 2, box.bottom };
            DrawStr(dc, L"\u2713", mk, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
                    g_fBody, CLR_WHITE);
        }

        RECT text = { box.right + 12, box.top, g_rcContent.right - 32, box.bottom };
        DrawStr(dc, L"完成后运行 PCL2", text, DT_LEFT | DT_VCENTER | DT_SINGLELINE,
                g_fBody, CLR_TEXT);

        g_rcLaunchCheck = { box.left - 4, box.top - 4, g_rcContent.right - 32, box.bottom + 4 };
    }
}

static void OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);

    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
    HGDIOBJ old = SelectObject(mem, bmp);

    FillColor(mem, { 0, 0, rc.right, rc.bottom }, CLR_CONTENT);
    DrawHeader(mem);
    DrawSidebar(mem);
    FillColor(mem, g_rcContent, CLR_CONTENT);

    switch (g_step) {
    case 0: DrawStep0(mem); break;
    case 1: DrawStep1(mem); break;
    case 2: DrawStep2(mem); break;
    case 3: DrawStep3(mem); break;
    }

    // 底部按钮
    bool showPrev = (g_step >= 1 && g_step <= 2) && !g_installing;
    bool showNext = (g_step != 3);
    if (showPrev) DrawButton(mem, g_rcPrev, L"上一步", false, false);
    if (showNext) {
        const wchar_t* label = L"下一步";
        if (g_step == 2) label = g_installing ? L"正在下载…" : L"开始下载";
        DrawButton(mem, g_rcNext, label, true, g_installing);
    } else {
        DrawButton(mem, g_rcNext, L"完成", true, false);
    }
    if (g_step == 1 && !g_installing) {
        DrawButton(mem, g_rcBrowse, L"浏览…", false, false);
    }

    BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);

    EndPaint(hwnd, &ps);
}

// ---------------------------------------------------------------------------
// 事件处理
// ---------------------------------------------------------------------------
static void OnBrowse(HWND hwnd)
{
    BROWSEINFOW bi = {};
    bi.hwndOwner = hwnd;
    bi.lpszTitle = L"请选择下载目录";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH] = { 0 };
        if (SHGetPathFromIDListW(pidl, path)) {
            g_installDir = path;
            if (g_editDir) SetWindowTextW(g_editDir, path);
        }
        CoTaskMemFree(pidl);
    }
}

static void OnNext(HWND hwnd)
{
    if (g_step == 3) {
        // 完成后：若勾选了“运行 PCL2”，则启动
        if (g_installSuccess && g_launchOnFinish) {
            LaunchPCL2();
        }
        DestroyWindow(hwnd);
        return;
    }
    if (g_step == 2) {
        if (g_installing) return;
        g_installing = true;
        g_downloadPercent = 0;
        g_installMessage = L"正在下载 PCL2…";
        SetTimer(hwnd, IDT_PROGRESS, 30, nullptr);
        InvalidateRect(hwnd, nullptr, TRUE);
        CreateThread(nullptr, 0, InstallThreadProc, (LPVOID)hwnd, 0, nullptr);
        return;
    }
    SetStep(hwnd, g_step + 1);
}

static void OnPrev(HWND hwnd)
{
    if (g_step > 0 && g_step < 3 && !g_installing) {
        SetStep(hwnd, g_step - 1);
    }
}

static void OnInstallDone(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    g_installing = false;
    KillTimer(hwnd, IDT_PROGRESS);

    int fail = (int)lParam;
    if (fail == 0) {
        g_installSuccess = true;
        g_installMessage = L"PCL2 已成功下载到：\n" + g_installDir +
                           L"\n\n配置文件 PCL\\Setup.ini 已生成。\n"
                           L"已创建桌面与开始菜单快捷方式，并写入注册表安装位置。\n"
                           L"您可以在该目录中启动 PCL2.EXE。";
    } else if (fail == 1) {
        g_installSuccess = false;
        g_installMessage = L"PCL2 下载失败，请检查网络连接后重试。\n\n"
                           L"下载地址：https://pclpcl2.7677677.xyz/file/PCL2.exe";
    } else {
        g_installSuccess = false;
        g_installMessage = L"无法创建目录或写入 PCL\\Setup.ini，请检查目录权限后重试。";
    }

    if (g_basicInstall) {
        // 自动模式：完成后直接关闭，--open 则先启动
        if (g_installSuccess && g_openOnFinish) LaunchPCL2();
        DestroyWindow(hwnd);
        return;
    }

    g_downloadPercent = -1;
    SetStep(hwnd, 3);
}

// ---------------------------------------------------------------------------
// 窗口过程
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        ComputeLayout(CLIENT_W, CLIENT_H);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        OnPaint(hwnd);
        return 0;

    case WM_TIMER:
        if (wParam == IDT_PROGRESS) {
            if (g_downloadPercent < 0) {
                g_progressPos += 12;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        if (!g_installing && PtInRect(&g_rcPrev, pt) && g_step >= 1 && g_step <= 2) {
            OnPrev(hwnd);
            return 0;
        }
        if (PtInRect(&g_rcNext, pt)) {
            OnNext(hwnd);
            return 0;
        }
        if (g_step == 1 && !g_installing && PtInRect(&g_rcBrowse, pt)) {
            OnBrowse(hwnd);
            return 0;
        }
        // “完成后运行 PCL2”复选框
        if (g_step == 3 && g_installSuccess && PtInRect(&g_rcLaunchCheck, pt)) {
            g_launchOnFinish = !g_launchOnFinish;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }

    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        if (id == IDC_EDIT_DIR && HIWORD(wParam) == EN_KILLFOCUS) {
            wchar_t buf[MAX_PATH] = { 0 };
            if (g_editDir) GetWindowTextW(g_editDir, buf, MAX_PATH);
            if (buf[0]) g_installDir = buf;
        }
        return 0;
    }

    case WM_APP_INSTALL_DONE:
        OnInstallDone(hwnd, wParam, lParam);
        return 0;

    case WM_APP_PROGRESS:
        g_downloadPercent = (int)lParam;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_CLOSE: {
        if (g_installing)
            return 0;

        if (g_step == 3)
            return DefWindowProcW(hwnd, msg, wParam, lParam);

        int r = MessageBoxW(hwnd, L"是否退出下载？", L"PCL2 下载器",
                            MB_OKCANCEL | MB_ICONQUESTION);
        if (r == IDOK)
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        return 0;
    }

    case WM_DESTROY:
        g_stopAudio = true;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// 入口
// ---------------------------------------------------------------------------
int main()
{
    g_hInst = GetModuleHandleW(nullptr);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // 默认下载目录：C:\Program Files\PCL2
    wchar_t prof[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"ProgramFiles", prof, MAX_PATH) > 0) {
        g_installDir = std::wstring(prof) + L"\\PCL2";
    } else {
        g_installDir = L"C:\\Program Files\\PCL2";
    }

    ParseCommandLine();

    // --silent：完全不打开窗口，静默安装后退出
    if (g_silent) {
        return RunSilentInstall();
    }

    // 字体
    g_fHeader = CreateAppFont(22, FW_SEMIBOLD, L"Microsoft YaHei UI");
    g_fTitle  = CreateAppFont(20, FW_SEMIBOLD, L"Microsoft YaHei UI");
    g_fBody   = CreateAppFont(15, FW_NORMAL, L"Microsoft YaHei UI");
    g_fSide   = CreateAppFont(15, FW_NORMAL, L"Microsoft YaHei UI");

    // 窗口类
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"PCL2DownloaderWnd";
    RegisterClassExW(&wc);

    RECT wr = { 0, 0, CLIENT_W, CLIENT_H };
    AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                       FALSE, 0);
    int w = wr.right - wr.left;
    int h = wr.bottom - wr.top;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"PCL2 下载器",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                (GetSystemMetrics(SM_CXSCREEN) - w) / 2,
                                (GetSystemMetrics(SM_CYSCREEN) - h) / 2,
                                w, h, nullptr, nullptr, g_hInst, nullptr);
    if (!hwnd) return 1;

    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    // --basicInstall：跳过向导，直接进入下载并开始安装
    if (g_basicInstall) {
        SetStep(hwnd, 2);
        OnNext(hwnd);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_fHeader) DeleteObject(g_fHeader);
    if (g_fTitle) DeleteObject(g_fTitle);
    if (g_fBody) DeleteObject(g_fBody);
    if (g_fSide) DeleteObject(g_fSide);
    return (int)msg.wParam;
}