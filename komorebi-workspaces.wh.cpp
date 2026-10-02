// ==WindhawkMod==
// @id              komorebi-workspaces
// @name            Komorebi Workspaces
// @description     Show komorebi workspaces and the windows they contain directly in the Windows 11 taskbar, no separate status bar needed
// @version         1.0
// @author          Alexandre Wavelet
// @github          https://github.com/alexandrewavelet
// @homepage        https://github.com/alexandrewavelet/windhawk-komorebi-workspaces
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -lgdi32 -lshell32 -lwindowscodecs
// @license         MIT
// ==/WindhawkMod==

// Source code is published under The MIT License.
//
// For bug reports and feature requests, please open an issue here:
// https://github.com/alexandrewavelet/windhawk-komorebi-workspaces/issues

// ==WindhawkModReadme==
/*
# Komorebi Workspaces

Shows the workspaces of the [komorebi](https://github.com/LGUG2Z/komorebi)
tiling window manager directly in the Windows 11 taskbar, together with the
icons of the windows each workspace contains. No separate status bar is needed,
and no screen space is lost to one.

![Screenshot](https://raw.githubusercontent.com/alexandrewavelet/windhawk-komorebi-workspaces/main/screenshots/closeup.png)

## Features

* Workspaces of the taskbar's monitor, with the focused one highlighted.
* Icons of the windows in each workspace, with the window title as a tooltip.
* Click a workspace to focus it.
* Click a window icon to focus that window, switching workspace if needed.
* Middle click a window icon to close the window.
* Scroll over the workspaces to cycle through them.
* Placement on the left, in the center or on the right of the taskbar.
* Customizable sizes, spacing, colors and opacity.

## Requirements

* [komorebi](https://github.com/LGUG2Z/komorebi) must be installed and
  running. The mod uses `komorebic` to subscribe to komorebi events, so set its
  path in the settings if komorebi isn't installed in the default location.
* Windows 11 with the new (XAML) taskbar.

For the default left placement, centered taskbar icons and a disabled Widgets
button leave the left side of the taskbar free for the workspaces.

## How it works

The mod subscribes to komorebi events through a named pipe
(`komorebic subscribe-pipe`), so the workspaces are updated as soon as komorebi
changes state. If komorebi is started later or restarted, the mod reconnects
automatically, and the workspaces are hidden while komorebi isn't running.

## Known limitations

* With multiple monitors, every taskbar shows the workspaces of the monitor of
  the main taskbar.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- position: left
  $name: Position
  $description: Where the workspaces are shown on the taskbar
  $options:
  - left: Left
  - center: Center
  - right: Right
- horizontalOffset: 8
  $name: Horizontal offset
  $description: >-
    Distance in pixels from the taskbar edge (left and right positions), or
    shift from the center (center position, negative values move left)
- showEmptyWorkspaces: false
  $name: Show empty workspaces
  $description: The focused workspace is always shown
- showWorkspaceNames: true
  $name: Show workspace names
  $description: When disabled, workspace numbers are shown instead
- showWindowIcons: true
  $name: Show window icons
- maxIconsPerWorkspace: 0
  $name: Maximum icons per workspace
  $description: >-
    Extra windows are shown as a "+N" counter (0 for no limit)
- scrollToSwitchWorkspaces: true
  $name: Scroll to switch workspaces
  $description: Scroll over the workspaces to cycle through them
- iconSize: 20
  $name: Icon size
- fontSize: 15
  $name: Font size
- fontFamily: ""
  $name: Font family
  $description: Leave empty to use the taskbar font
- workspacePadding: 8
  $name: Workspace padding
  $description: Horizontal padding inside each workspace tile, in pixels
- workspaceSpacing: 6
  $name: Workspace spacing
  $description: Space between workspace tiles, in pixels
- accentColor: "#faa968"
  $name: Accent color
  $description: Focused workspace color, as #RRGGBB or #AARRGGBB
- textColor: "#f6dcac"
  $name: Text color
  $description: Workspace label color, as #RRGGBB or #AARRGGBB
- inactiveOpacity: 90
  $name: Unfocused workspace opacity (%)
- focusedBackgroundOpacity: 30
  $name: Focused workspace background opacity (%)
  $description: Opacity of the accent color behind the focused workspace
- inactiveBackgroundOpacity: 12
  $name: Unfocused workspace background opacity (%)
  $description: Opacity of the text color behind unfocused workspaces
- hoverBackgroundOpacity: 25
  $name: Hovered workspace background opacity (%)
  $description: Opacity of the text color behind a hovered workspace
- komorebicPath: C:\Program Files\komorebi\bin\komorebic-no-console.exe
  $name: komorebic path
  $description: >-
    Full path to komorebic-no-console.exe (or komorebic.exe)
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#undef GetCurrentTime

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.h>

#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wincodec.h>

#include <algorithm>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace winrt::Windows::UI::Xaml;
namespace Json = winrt::Windows::Data::Json;

constexpr WCHAR kPanelName[] = L"KomorebiWorkspacesPanel";

enum class Position {
    left,
    center,
    right,
};

struct {
    Position position;
    int horizontalOffset;
    bool showEmptyWorkspaces;
    bool showWorkspaceNames;
    bool showWindowIcons;
    int maxIconsPerWorkspace;
    bool scrollToSwitchWorkspaces;
    int iconSize;
    int fontSize;
    std::wstring fontFamily;
    int workspacePadding;
    int workspaceSpacing;
    winrt::Windows::UI::Color accentColor;
    winrt::Windows::UI::Color textColor;
    double inactiveOpacity;
    BYTE focusedBackgroundAlpha;
    BYTE inactiveBackgroundAlpha;
    BYTE hoverBackgroundAlpha;
    std::wstring komorebicPath;
} g_settings;

// Guards the string settings, which are read from other threads while the
// settings are reloaded.
std::mutex g_settingsStringsMutex;

std::atomic<bool> g_unloading;
std::atomic<int> g_hookCallCounter;
std::atomic<int> g_pendingCalls;  // UI callbacks and action threads in flight.

HANDLE g_stopEvent;
HANDLE g_komorebiThread;

////////////////////////////////////////////////////////////////////////////////
// Model

struct WindowInfo {
    HWND hwnd;
    std::wstring title;
    std::wstring exe;
    bool focused;
};

struct WorkspaceInfo {
    std::wstring name;
    bool focused;
    std::vector<WindowInfo> windows;
};

struct Model {
    int monitorIndex = -1;
    std::vector<WorkspaceInfo> workspaces;
};

struct IconPixels {
    int size;
    std::vector<BYTE> bgra;  // Premultiplied BGRA, top-down.
};

std::mutex g_modelMutex;
Model g_model;
std::wstring g_modelSignature;
std::unordered_map<HWND, IconPixels> g_icons;

////////////////////////////////////////////////////////////////////////////////
// Injected panels

struct InjectedFrame {
    winrt::weak_ref<FrameworkElement> frame;
    winrt::weak_ref<Controls::StackPanel> panel;
    winrt::Windows::System::DispatcherQueue dispatcherQueue{nullptr};
};

std::mutex g_framesMutex;
std::vector<InjectedFrame> g_frames;

////////////////////////////////////////////////////////////////////////////////
// Helpers

winrt::Windows::UI::Color ParseColor(PCWSTR str,
                                     winrt::Windows::UI::Color fallback) {
    std::wstring s = str ? str : L"";
    if (!s.empty() && s[0] == L'#') {
        s = s.substr(1);
    }
    if (s.size() != 6 && s.size() != 8) {
        return fallback;
    }
    WCHAR* end;
    unsigned long v = wcstoul(s.c_str(), &end, 16);
    if (*end) {
        return fallback;
    }
    if (s.size() == 6) {
        v |= 0xFF000000;
    }
    return {(BYTE)(v >> 24), (BYTE)(v >> 16), (BYTE)(v >> 8), (BYTE)v};
}

HWND FindCurrentProcessTaskbarWnd() {
    HWND hTaskbarWnd = nullptr;

    EnumWindows(
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            DWORD dwProcessId;
            WCHAR className[32];
            if (GetWindowThreadProcessId(hWnd, &dwProcessId) &&
                dwProcessId == GetCurrentProcessId() &&
                GetClassName(hWnd, className, ARRAYSIZE(className)) &&
                _wcsicmp(className, L"Shell_TrayWnd") == 0) {
                *reinterpret_cast<HWND*>(lParam) = hWnd;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&hTaskbarWnd));

    return hTaskbarWnd;
}

FrameworkElement EnumChildElements(
    FrameworkElement element,
    std::function<bool(FrameworkElement)> enumCallback) {
    int childrenCount = Media::VisualTreeHelper::GetChildrenCount(element);

    for (int i = 0; i < childrenCount; i++) {
        auto child = Media::VisualTreeHelper::GetChild(element, i)
                         .try_as<FrameworkElement>();
        if (child && enumCallback(child)) {
            return child;
        }
    }

    return nullptr;
}

FrameworkElement FindChildByName(FrameworkElement element, PCWSTR name) {
    return EnumChildElements(element, [name](FrameworkElement child) {
        return child.Name() == name;
    });
}

// Runs a komorebic command. If output is non-null, stdout is captured.
bool RunKomorebic(const std::wstring& args,
                  std::string* output = nullptr,
                  DWORD timeoutMs = 5000) {
    std::wstring commandLine;
    {
        std::lock_guard lock(g_settingsStringsMutex);
        commandLine = L"\"" + g_settings.komorebicPath + L"\" " + args;
    }

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    STARTUPINFO si{.cb = sizeof(si)};
    if (output) {
        SECURITY_ATTRIBUTES sa{.nLength = sizeof(sa), .bInheritHandle = TRUE};
        if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
            return false;
        }
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = writePipe;
        si.hStdError = writePipe;
        si.hStdInput = nullptr;
    }

    PROCESS_INFORMATION pi{};
    BOOL created = CreateProcess(nullptr, commandLine.data(), nullptr, nullptr,
                                 output ? TRUE : FALSE, CREATE_NO_WINDOW,
                                 nullptr, nullptr, &si, &pi);
    if (writePipe) {
        CloseHandle(writePipe);
    }
    if (!created) {
        Wh_Log(L"CreateProcess failed (%u): %s", GetLastError(),
               commandLine.c_str());
        if (readPipe) {
            CloseHandle(readPipe);
        }
        return false;
    }

    if (output) {
        char buffer[16384];
        DWORD read;
        while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) &&
               read) {
            output->append(buffer, read);
        }
        CloseHandle(readPipe);
    }

    WaitForSingleObject(pi.hProcess, timeoutMs);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        Wh_Log(L"komorebic %s exited with %u", args.c_str(), exitCode);
    }

    return exitCode == 0;
}

// Runs a function on a separate thread, tracked so that unloading waits for it.
void RunDetached(std::function<void()> func) {
    auto* param = new std::function<void()>(std::move(func));
    g_pendingCalls++;
    HANDLE thread = CreateThread(
        nullptr, 0,
        [](LPVOID p) -> DWORD {
            auto* f = static_cast<std::function<void()>*>(p);
            (*f)();
            delete f;
            g_pendingCalls--;
            return 0;
        },
        param, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    } else {
        delete param;
        g_pendingCalls--;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Icons

// Returns the size of the largest dimension of an icon's bitmap, or 0.
int GetIconNativeSize(HICON icon) {
    ICONINFO iconInfo{};
    if (!GetIconInfo(icon, &iconInfo)) {
        return 0;
    }

    int size = 0;
    BITMAP bitmap{};
    if (iconInfo.hbmColor &&
        GetObject(iconInfo.hbmColor, sizeof(bitmap), &bitmap)) {
        size = std::max(bitmap.bmWidth, bitmap.bmHeight);
    } else if (iconInfo.hbmMask &&
               GetObject(iconInfo.hbmMask, sizeof(bitmap), &bitmap)) {
        // A monochrome icon's mask holds the AND and XOR masks stacked.
        size = std::max(bitmap.bmWidth, bitmap.bmHeight / 2);
    }

    if (iconInfo.hbmColor) {
        DeleteObject(iconInfo.hbmColor);
    }
    if (iconInfo.hbmMask) {
        DeleteObject(iconInfo.hbmMask);
    }
    return size;
}

// Resizes premultiplied BGRA pixels with a high quality filter. GDI's own
// stretching (e.g. in DrawIconEx) is nearest-neighbor-like and makes downscaled
// icons look pixelated.
bool ScaleIconPixels(IconPixels* pixels, int size) {
    if (pixels->size == size) {
        return true;
    }

    winrt::com_ptr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(factory.put())))) {
        return false;
    }

    winrt::com_ptr<IWICBitmap> source;
    if (FAILED(factory->CreateBitmapFromMemory(
            pixels->size, pixels->size, GUID_WICPixelFormat32bppPBGRA,
            pixels->size * 4, (UINT)pixels->bgra.size(), pixels->bgra.data(),
            source.put()))) {
        return false;
    }

    winrt::com_ptr<IWICBitmapScaler> scaler;
    if (FAILED(factory->CreateBitmapScaler(scaler.put())) ||
        FAILED(scaler->Initialize(
            source.get(), size, size,
            // Fant is a box filter, which suits downscaling best.
            size < pixels->size ? WICBitmapInterpolationModeFant
                                : WICBitmapInterpolationModeCubic))) {
        return false;
    }

    std::vector<BYTE> scaled((size_t)size * size * 4);
    if (FAILED(scaler->CopyPixels(nullptr, size * 4, (UINT)scaled.size(),
                                  scaled.data()))) {
        return false;
    }

    pixels->size = size;
    pixels->bgra = std::move(scaled);
    return true;
}

// Renders an icon on black and on white backgrounds, and derives the alpha
// channel from the difference. Works for both alpha and legacy mask icons. The
// icon is drawn at its native size, then resized with a high quality filter.
bool RenderIconPixels(HICON icon, int size, IconPixels* result) {
    int renderSize = GetIconNativeSize(icon);
    if (renderSize <= 0 || renderSize > 256) {
        renderSize = size;
    }

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = renderSize;
    bi.bmiHeader.biHeight = -renderSize;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        return false;
    }

    void* bitsBlack = nullptr;
    void* bitsWhite = nullptr;
    HBITMAP bmpBlack =
        CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bitsBlack, nullptr, 0);
    HBITMAP bmpWhite =
        CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bitsWhite, nullptr, 0);

    bool ok = false;
    if (bmpBlack && bmpWhite) {
        size_t bytes = (size_t)renderSize * renderSize * 4;

        HGDIOBJ old = SelectObject(dc, bmpBlack);
        memset(bitsBlack, 0x00, bytes);
        DrawIconEx(dc, 0, 0, icon, renderSize, renderSize, 0, nullptr,
                   DI_NORMAL);
        SelectObject(dc, bmpWhite);
        memset(bitsWhite, 0xFF, bytes);
        DrawIconEx(dc, 0, 0, icon, renderSize, renderSize, 0, nullptr,
                   DI_NORMAL);
        SelectObject(dc, old);
        GdiFlush();

        result->size = renderSize;
        result->bgra.resize(bytes);
        const BYTE* b = (const BYTE*)bitsBlack;
        const BYTE* w = (const BYTE*)bitsWhite;
        BYTE* out = result->bgra.data();
        for (size_t i = 0; i < bytes; i += 4) {
            int diff = ((w[i] - b[i]) + (w[i + 1] - b[i + 1]) +
                        (w[i + 2] - b[i + 2])) /
                       3;
            int alpha = 255 - diff;
            alpha = alpha < 0 ? 0 : (alpha > 255 ? 255 : alpha);
            // The black background rendering is the premultiplied color.
            out[i] = std::min<int>(b[i], alpha);
            out[i + 1] = std::min<int>(b[i + 1], alpha);
            out[i + 2] = std::min<int>(b[i + 2], alpha);
            out[i + 3] = (BYTE)alpha;
        }
        ok = true;
    }

    if (bmpBlack) {
        DeleteObject(bmpBlack);
    }
    if (bmpWhite) {
        DeleteObject(bmpWhite);
    }
    DeleteDC(dc);

    return ok && ScaleIconPixels(result, size);
}

bool BitmapToPixels(HBITMAP bitmap, int size, IconPixels* result) {
    BITMAP bitmapInfo{};
    if (!GetObject(bitmap, sizeof(bitmapInfo), &bitmapInfo) ||
        bitmapInfo.bmWidth <= 0 ||
        bitmapInfo.bmWidth != std::abs(bitmapInfo.bmHeight)) {
        return false;
    }
    int bitmapSize = bitmapInfo.bmWidth;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = bitmapSize;
    bi.bmiHeader.biHeight = -bitmapSize;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    result->size = bitmapSize;
    result->bgra.resize((size_t)bitmapSize * bitmapSize * 4);
    HDC dc = GetDC(nullptr);
    int lines = GetDIBits(dc, bitmap, 0, bitmapSize, result->bgra.data(), &bi,
                          DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (lines != bitmapSize) {
        return false;
    }

    auto& bgra = result->bgra;
    bool hasAlpha = false;
    bool straightAlpha = false;
    for (size_t i = 0; i < bgra.size(); i += 4) {
        BYTE alpha = bgra[i + 3];
        if (alpha) {
            hasAlpha = true;
        }
        if (bgra[i] > alpha || bgra[i + 1] > alpha || bgra[i + 2] > alpha) {
            straightAlpha = true;
        }
    }

    if (!hasAlpha) {
        for (size_t i = 3; i < bgra.size(); i += 4) {
            bgra[i] = 0xFF;
        }
    } else if (straightAlpha) {
        // The shell returns app icons with straight (non-premultiplied)
        // alpha, but XAML expects premultiplied pixels. Without this, the
        // semi-transparent edges show a light halo.
        for (size_t i = 0; i < bgra.size(); i += 4) {
            BYTE alpha = bgra[i + 3];
            bgra[i] = (BYTE)(bgra[i] * alpha / 255);
            bgra[i + 1] = (BYTE)(bgra[i + 1] * alpha / 255);
            bgra[i + 2] = (BYTE)(bgra[i + 2] * alpha / 255);
        }
    }

    return ScaleIconPixels(result, size);
}

std::wstring GetProcessAumid(DWORD pid) {
    using GetApplicationUserModelId_t =
        LONG(WINAPI*)(HANDLE hProcess, UINT32 * length, PWSTR id);
    static auto pGetApplicationUserModelId =
        (GetApplicationUserModelId_t)GetProcAddress(
            GetModuleHandle(L"kernel32.dll"), "GetApplicationUserModelId");
    if (!pGetApplicationUserModelId) {
        return L"";
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return L"";
    }
    WCHAR buffer[256];
    UINT32 length = ARRAYSIZE(buffer);
    LONG error = pGetApplicationUserModelId(process, &length, buffer);
    CloseHandle(process);
    return error == ERROR_SUCCESS ? buffer : L"";
}

std::wstring GetWindowAumid(HWND hwnd) {
    // PKEY_AppUserModel_ID
    static const PROPERTYKEY kAumidKey = {
        {0x9F4C2855,
         0x9F79,
         0x4B39,
         {0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3}},
        5};

    // The explicit AUMID of the window, as used by the taskbar.
    winrt::com_ptr<IPropertyStore> store;
    if (SUCCEEDED(
            SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(store.put())))) {
        std::wstring aumid;
        PROPVARIANT pv;
        PropVariantInit(&pv);
        if (SUCCEEDED(store->GetValue(kAumidKey, &pv)) && pv.vt == VT_LPWSTR &&
            pv.pwszVal) {
            aumid = pv.pwszVal;
        }
        PropVariantClear(&pv);
        if (!aumid.empty()) {
            return aumid;
        }
    }

    // The AUMID of the window's process, if it's a packaged app.
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (std::wstring aumid = GetProcessAumid(pid); !aumid.empty()) {
        return aumid;
    }

    // UWP apps are hosted in an ApplicationFrameWindow owned by
    // ApplicationFrameHost.exe, with the app's own window as a child.
    WCHAR className[64];
    if (GetClassName(hwnd, className, ARRAYSIZE(className)) &&
        wcscmp(className, L"ApplicationFrameWindow") == 0) {
        struct Param {
            DWORD framePid;
            std::wstring aumid;
        } param{pid};
        EnumChildWindows(
            hwnd,
            [](HWND child, LPARAM lParam) -> BOOL {
                auto* param = reinterpret_cast<Param*>(lParam);
                DWORD childPid = 0;
                GetWindowThreadProcessId(child, &childPid);
                if (childPid && childPid != param->framePid) {
                    param->aumid = GetProcessAumid(childPid);
                    return param->aumid.empty();
                }
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&param));
        return param.aumid;
    }

    return L"";
}

// Packaged apps (Store apps, Settings, etc.) have their real icons in their
// package, not in their executable. Get the icon from the app's AUMID through
// the shell, as the Start menu does.
bool GetPackagedAppIconPixels(HWND hwnd, int size, IconPixels* result) {
    // FOLDERID_AppsFolder
    static const GUID kAppsFolder = {
        0x1e87508d,
        0x89c2,
        0x42f0,
        {0x8a, 0x7e, 0x64, 0x5a, 0x0f, 0x50, 0xca, 0x58}};

    // Packaged app AUMIDs have the "PackageFamilyName!AppId" format. Other
    // AUMIDs (e.g. set by browsers) may not be in the Apps folder, and the
    // window icon is better for them anyway.
    std::wstring aumid = GetWindowAumid(hwnd);
    if (aumid.find(L'!') == std::wstring::npos) {
        return false;
    }

    winrt::com_ptr<IShellItemImageFactory> factory;
    if (FAILED(SHCreateItemInKnownFolder(kAppsFolder, 0, aumid.c_str(),
                                         IID_PPV_ARGS(factory.put())))) {
        return false;
    }

    HBITMAP bitmap = nullptr;
    if (FAILED(factory->GetImage({size, size}, SIIGBF_ICONONLY, &bitmap)) ||
        !bitmap) {
        return false;
    }

    bool ok = BitmapToPixels(bitmap, size, result);
    DeleteObject(bitmap);
    return ok;
}

bool GetProcessImagePath(HWND hwnd, std::wstring* path) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return false;
    }
    WCHAR buffer[MAX_PATH];
    DWORD bufferSize = ARRAYSIZE(buffer);
    BOOL ok = QueryFullProcessImageName(process, 0, buffer, &bufferSize);
    CloseHandle(process);
    if (!ok) {
        return false;
    }
    *path = buffer;
    return true;
}

// Extracts the icon of an executable at the given size, picking the best
// matching image of its icon resource.
bool GetExecutableIconPixels(const std::wstring& path,
                             int size,
                             IconPixels* result) {
    HICON icon = nullptr;
    if (FAILED(SHDefExtractIcon(path.c_str(), 0, 0, &icon, nullptr, size)) ||
        !icon) {
        return false;
    }
    bool ok = RenderIconPixels(icon, size, result);
    DestroyIcon(icon);
    return ok;
}

HICON GetWindowIcon(HWND hwnd, bool big) {
    DWORD_PTR icon = 0;
    SendMessageTimeout(hwnd, WM_GETICON, big ? ICON_BIG : ICON_SMALL2, 0,
                       SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &icon);
    if (!icon) {
        icon = GetClassLongPtr(hwnd, big ? GCLP_HICON : GCLP_HICONSM);
    }
    return (HICON)icon;
}

bool GetWindowIconPixels(HWND hwnd,
                         const std::wstring& exe,
                         int size,
                         IconPixels* result) {
    std::wstring path;
    bool hasPath = GetProcessImagePath(hwnd, &path);

    // File Explorer windows show the icon of the current folder, while the
    // taskbar shows the File Explorer icon. Use the latter, which is more
    // recognizable.
    if (hasPath && _wcsicmp(exe.c_str(), L"explorer.exe") == 0 &&
        GetExecutableIconPixels(path, size, result)) {
        return true;
    }

    // The window's own icon matches what the taskbar shows, e.g. for browser
    // profiles or web apps, so prefer it when it's large enough not to be
    // upscaled.
    HICON bigIcon = GetWindowIcon(hwnd, /*big=*/true);
    if (bigIcon && GetIconNativeSize(bigIcon) >= size) {
        return RenderIconPixels(bigIcon, size, result);
    }

    HICON smallIcon = GetWindowIcon(hwnd, /*big=*/false);
    if (smallIcon && GetIconNativeSize(smallIcon) >= size) {
        return RenderIconPixels(smallIcon, size, result);
    }

    // Packaged apps hosted in an ApplicationFrameWindow (e.g. Settings) have
    // no window icon, and their executable icon is generic or missing. Their
    // real icon is in their package. It's only a fallback, since the package
    // icon may be plated (e.g. WhatsApp's green tile instead of its bubble).
    if (GetPackagedAppIconPixels(hwnd, size, result)) {
        return true;
    }

    // Otherwise, the executable usually has the same icon in more sizes.
    if (hasPath && GetExecutableIconPixels(path, size, result)) {
        return true;
    }

    if (HICON icon = bigIcon ? bigIcon : smallIcon) {
        return RenderIconPixels(icon, size, result);
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////
// komorebi state parsing

Json::IJsonValue ValueAt(const Json::JsonObject& obj, PCWSTR key) {
    if (!obj || !obj.HasKey(key)) {
        return nullptr;
    }
    return obj.Lookup(key);
}

Json::JsonObject ObjectAt(const Json::JsonObject& obj, PCWSTR key) {
    auto value = ValueAt(obj, key);
    if (!value || value.ValueType() != Json::JsonValueType::Object) {
        return nullptr;
    }
    return value.GetObject();
}

std::wstring StringAt(const Json::JsonObject& obj, PCWSTR key) {
    auto value = ValueAt(obj, key);
    if (!value || value.ValueType() != Json::JsonValueType::String) {
        return L"";
    }
    return std::wstring(value.GetString());
}

double NumberAt(const Json::JsonObject& obj, PCWSTR key, double fallback) {
    auto value = ValueAt(obj, key);
    if (!value || value.ValueType() != Json::JsonValueType::Number) {
        return fallback;
    }
    return value.GetNumber();
}

// komorebi serializes Ring<T> as {"elements": [...], "focused": n}. Plain
// arrays are accepted too, for older/newer versions.
Json::JsonArray RingAt(const Json::JsonObject& obj,
                       PCWSTR key,
                       int* focused = nullptr) {
    if (focused) {
        *focused = -1;
    }
    auto value = ValueAt(obj, key);
    if (!value) {
        return nullptr;
    }
    if (value.ValueType() == Json::JsonValueType::Array) {
        return value.GetArray();
    }
    if (value.ValueType() != Json::JsonValueType::Object) {
        return nullptr;
    }
    auto ring = value.GetObject();
    if (focused) {
        *focused = (int)NumberAt(ring, L"focused", -1);
    }
    auto elements = ValueAt(ring, L"elements");
    if (!elements || elements.ValueType() != Json::JsonValueType::Array) {
        return nullptr;
    }
    return elements.GetArray();
}

int FindMonitorIndex(const Json::JsonArray& monitors) {
    HWND taskbarWnd = FindCurrentProcessTaskbarWnd();
    HMONITOR target =
        taskbarWnd ? MonitorFromWindow(taskbarWnd, MONITOR_DEFAULTTOPRIMARY)
                   : MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{.cbSize = sizeof(mi)};
    GetMonitorInfo(target, &mi);

    int bySize = -1;
    for (uint32_t i = 0; i < monitors.Size(); i++) {
        auto monitor = monitors.GetObjectAt(i);
        if ((HMONITOR)(INT_PTR)NumberAt(monitor, L"id", 0) == target) {
            return i;
        }
        auto size = ObjectAt(monitor, L"size");
        if (bySize == -1 && size &&
            (LONG)NumberAt(size, L"left", -1) == mi.rcMonitor.left &&
            (LONG)NumberAt(size, L"top", -1) == mi.rcMonitor.top) {
            bySize = i;
        }
    }

    if (bySize != -1) {
        return bySize;
    }
    return monitors.Size() > 0 ? 0 : -1;
}

void AddWindow(WorkspaceInfo& ws,
               std::unordered_set<HWND>& seen,
               const Json::JsonObject& window,
               bool focused) {
    if (!window) {
        return;
    }
    HWND hwnd = (HWND)(INT_PTR)NumberAt(window, L"hwnd", 0);
    if (!hwnd || !seen.insert(hwnd).second) {
        return;
    }
    ws.windows.push_back({
        .hwnd = hwnd,
        .title = StringAt(window, L"title"),
        .exe = StringAt(window, L"exe"),
        .focused = focused,
    });
}

void AddContainerWindows(WorkspaceInfo& ws,
                         std::unordered_set<HWND>& seen,
                         const Json::JsonObject& container,
                         bool focusedContainer) {
    int focused;
    auto windows = RingAt(container, L"windows", &focused);
    if (!windows) {
        return;
    }
    for (uint32_t i = 0; i < windows.Size(); i++) {
        AddWindow(ws, seen, windows.GetObjectAt(i),
                  focusedContainer && (int)i == focused);
    }
}

bool ParseState(const Json::JsonObject& state, Model* model) {
    auto monitors = RingAt(state, L"monitors");
    if (!monitors) {
        return false;
    }

    int monitorIndex = FindMonitorIndex(monitors);
    if (monitorIndex < 0) {
        return false;
    }

    auto monitor = monitors.GetObjectAt(monitorIndex);
    int focusedWorkspace;
    auto workspaces = RingAt(monitor, L"workspaces", &focusedWorkspace);
    if (!workspaces) {
        return false;
    }

    model->monitorIndex = monitorIndex;
    model->workspaces.clear();

    for (uint32_t i = 0; i < workspaces.Size(); i++) {
        auto workspace = workspaces.GetObjectAt(i);
        WorkspaceInfo ws{
            .name = StringAt(workspace, L"name"),
            .focused = (int)i == focusedWorkspace,
        };
        std::unordered_set<HWND> seen;

        if (auto monocle = ObjectAt(workspace, L"monocle_container")) {
            AddContainerWindows(ws, seen, monocle, true);
        }
        AddWindow(ws, seen, ObjectAt(workspace, L"maximized_window"), false);

        int focusedContainer;
        if (auto containers =
                RingAt(workspace, L"containers", &focusedContainer)) {
            for (uint32_t c = 0; c < containers.Size(); c++) {
                AddContainerWindows(ws, seen, containers.GetObjectAt(c),
                                    (int)c == focusedContainer);
            }
        }

        if (auto floating = RingAt(workspace, L"floating_windows")) {
            for (uint32_t f = 0; f < floating.Size(); f++) {
                AddWindow(ws, seen, floating.GetObjectAt(f), false);
            }
        }

        model->workspaces.push_back(std::move(ws));
    }

    return true;
}

std::wstring ModelSignature(const Model& model) {
    std::wstring s = std::to_wstring(model.monitorIndex);
    for (const auto& ws : model.workspaces) {
        s += L"|" + ws.name + (ws.focused ? L"*" : L"");
        for (const auto& w : ws.windows) {
            s += L"," + std::to_wstring((INT_PTR)w.hwnd) + w.title +
                 (w.focused ? L"*" : L"");
        }
    }
    return s;
}

////////////////////////////////////////////////////////////////////////////////
// UI

int GetIconPixelSize() {
    UINT dpi = 96;
    if (HWND taskbarWnd = FindCurrentProcessTaskbarWnd()) {
        dpi = GetDpiForWindow(taskbarWnd);
    }
    return MulDiv(g_settings.iconSize, dpi ? dpi : 96, 96);
}

void FocusWorkspace(int monitorIndex, int workspaceIndex) {
    RunDetached([monitorIndex, workspaceIndex] {
        RunKomorebic(L"focus-monitor-workspace " +
                     std::to_wstring(monitorIndex) + L" " +
                     std::to_wstring(workspaceIndex));
    });
}

void FocusWindow(int monitorIndex,
                 int workspaceIndex,
                 bool workspaceFocused,
                 HWND hwnd) {
    RunDetached([=] {
        if (!workspaceFocused) {
            RunKomorebic(L"focus-monitor-workspace " +
                         std::to_wstring(monitorIndex) + L" " +
                         std::to_wstring(workspaceIndex));
            Sleep(50);
        }
        if (IsIconic(hwnd)) {
            ShowWindowAsync(hwnd, SW_RESTORE);
        }
        SetForegroundWindow(hwnd);
    });
}

void CycleWorkspace(bool next) {
    // A mouse wheel notch can produce several events, and each one runs
    // komorebic, so ignore events that arrive too quickly.
    static std::atomic<ULONGLONG> lastTickCount;
    ULONGLONG tickCount = GetTickCount64();
    if (tickCount - lastTickCount < 150) {
        return;
    }
    lastTickCount = tickCount;

    RunDetached([next] {
        RunKomorebic(next ? L"cycle-workspace next"
                          : L"cycle-workspace previous");
    });
}

void ApplyFont(Controls::TextBlock textBlock) {
    textBlock.FontSize(g_settings.fontSize);

    std::wstring fontFamily;
    {
        std::lock_guard lock(g_settingsStringsMutex);
        fontFamily = g_settings.fontFamily;
    }
    if (!fontFamily.empty()) {
        textBlock.FontFamily(Media::FontFamily(fontFamily));
    }
}

Media::ImageSource CreateIconSource(const IconPixels& pixels) {
    Media::Imaging::WriteableBitmap bitmap(pixels.size, pixels.size);
    auto buffer = bitmap.PixelBuffer();
    if (buffer.Capacity() < pixels.bgra.size()) {
        return nullptr;
    }
    memcpy(buffer.data(), pixels.bgra.data(), pixels.bgra.size());
    bitmap.Invalidate();
    return bitmap;
}

UIElement CreateWindowElement(const Model& model,
                              int workspaceIndex,
                              const WorkspaceInfo& ws,
                              const WindowInfo& window) {
    Controls::Border container;
    container.Background(
        Media::SolidColorBrush(winrt::Windows::UI::Colors::Transparent()));
    container.Padding({1, 0, 1, 0});
    container.VerticalAlignment(VerticalAlignment::Center);

    UIElement content{nullptr};
    {
        std::lock_guard lock(g_modelMutex);
        auto it = g_icons.find(window.hwnd);
        if (it != g_icons.end()) {
            if (auto source = CreateIconSource(it->second)) {
                Controls::Image image;
                image.Source(source);
                image.Width(g_settings.iconSize);
                image.Height(g_settings.iconSize);
                content = image;
            }
        }
    }
    if (!content) {
        Controls::TextBlock fallback;
        fallback.Text(window.exe.empty() ? L"?" : window.exe.substr(0, 1));
        ApplyFont(fallback);
        fallback.Width(g_settings.iconSize);
        fallback.TextAlignment(TextAlignment::Center);
        fallback.Foreground(Media::SolidColorBrush(g_settings.textColor));
        content = fallback;
    }
    container.Child(content);

    std::wstring tooltip = window.title;
    if (!window.exe.empty()) {
        tooltip += L" (" + window.exe + L")";
    }
    Controls::ToolTipService::SetToolTip(container, winrt::box_value(tooltip));

    int monitorIndex = model.monitorIndex;
    bool workspaceFocused = ws.focused;
    HWND hwnd = window.hwnd;

    container.Tapped([=](auto const&, Input::TappedRoutedEventArgs const& e) {
        e.Handled(true);
        FocusWindow(monitorIndex, workspaceIndex, workspaceFocused, hwnd);
    });

    // Use the sender rather than capturing the element, which would create a
    // reference cycle and leak the element.
    container.PointerPressed(
        [](winrt::Windows::Foundation::IInspectable const& sender,
           Input::PointerRoutedEventArgs const& e) {
            auto point = e.GetCurrentPoint(sender.as<UIElement>());
            if (point.Properties().IsMiddleButtonPressed()) {
                e.Handled(true);
            }
        });

    container.PointerReleased(
        [hwnd](winrt::Windows::Foundation::IInspectable const& sender,
               Input::PointerRoutedEventArgs const& e) {
            auto point = e.GetCurrentPoint(sender.as<UIElement>());
            if (point.Properties().PointerUpdateKind() ==
                winrt::Windows::UI::Input::PointerUpdateKind::
                    MiddleButtonReleased) {
                e.Handled(true);
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
        });

    return container;
}

void ApplyPanelLayout(Controls::StackPanel panel) {
    double offset = g_settings.horizontalOffset;
    switch (g_settings.position) {
        case Position::left:
            panel.HorizontalAlignment(HorizontalAlignment::Left);
            panel.Margin({offset, 0, 0, 0});
            break;
        case Position::center:
            panel.HorizontalAlignment(HorizontalAlignment::Center);
            // A centered element moves by half of an asymmetric margin.
            panel.Margin({offset * 2 > 0 ? offset * 2 : 0, 0,
                          offset * 2 < 0 ? -offset * 2 : 0, 0});
            break;
        case Position::right:
            panel.HorizontalAlignment(HorizontalAlignment::Right);
            panel.Margin({0, 0, offset, 0});
            break;
    }
}

void RenderPanel(Controls::StackPanel panel) {
    Model model;
    {
        std::lock_guard lock(g_modelMutex);
        model = g_model;
    }

    ApplyPanelLayout(panel);

    auto children = panel.Children();
    children.Clear();

    for (size_t i = 0; i < model.workspaces.size(); i++) {
        const auto& ws = model.workspaces[i];
        if (!g_settings.showEmptyWorkspaces && ws.windows.empty() &&
            !ws.focused) {
            continue;
        }

        auto accent = g_settings.accentColor;

        double padding = g_settings.workspacePadding;

        Controls::Border pill;
        pill.CornerRadius({6, 6, 6, 6});
        pill.Padding({padding, padding / 2, padding, padding / 2});
        pill.Margin({0, 0, (double)g_settings.workspaceSpacing, 0});
        pill.VerticalAlignment(VerticalAlignment::Center);
        pill.BorderThickness({0, 0, 0, 2});

        winrt::Windows::UI::Color background;
        if (ws.focused) {
            background = accent;
            background.A = g_settings.focusedBackgroundAlpha;
            pill.BorderBrush(Media::SolidColorBrush(accent));
        } else {
            background = g_settings.textColor;
            background.A = g_settings.inactiveBackgroundAlpha;
            pill.BorderBrush(Media::SolidColorBrush(
                winrt::Windows::UI::Colors::Transparent()));
            pill.Opacity(g_settings.inactiveOpacity);
        }
        Media::SolidColorBrush backgroundBrush(background);
        pill.Background(backgroundBrush);

        if (!ws.focused) {
            auto hoverBackground = g_settings.textColor;
            hoverBackground.A = g_settings.hoverBackgroundAlpha;
            pill.PointerEntered(
                [backgroundBrush, hoverBackground](auto const&, auto const&) {
                    backgroundBrush.Color(hoverBackground);
                });
            pill.PointerExited(
                [backgroundBrush, background](auto const&, auto const&) {
                    backgroundBrush.Color(background);
                });
        }

        Controls::StackPanel inner;
        inner.Orientation(Controls::Orientation::Horizontal);
        inner.Spacing(6);

        Controls::TextBlock label;
        label.Text(g_settings.showWorkspaceNames && !ws.name.empty()
                       ? ws.name
                       : std::to_wstring(i + 1));
        ApplyFont(label);
        label.VerticalAlignment(VerticalAlignment::Center);
        label.Foreground(
            Media::SolidColorBrush(ws.focused ? accent : g_settings.textColor));
        label.FontWeight(
            ws.focused ? winrt::Windows::UI::Text::FontWeights::Bold()
                       : winrt::Windows::UI::Text::FontWeights::SemiBold());
        inner.Children().Append(label);

        if (g_settings.showWindowIcons) {
            size_t shown = ws.windows.size();
            if (g_settings.maxIconsPerWorkspace > 0 &&
                shown > (size_t)g_settings.maxIconsPerWorkspace) {
                shown = g_settings.maxIconsPerWorkspace;
            }

            for (size_t w = 0; w < shown; w++) {
                inner.Children().Append(
                    CreateWindowElement(model, (int)i, ws, ws.windows[w]));
            }

            if (shown < ws.windows.size()) {
                Controls::TextBlock more;
                more.Text(L"+" + std::to_wstring(ws.windows.size() - shown));
                ApplyFont(more);
                more.FontSize(g_settings.fontSize * 0.85);
                more.VerticalAlignment(VerticalAlignment::Center);
                more.Foreground(Media::SolidColorBrush(g_settings.textColor));

                std::wstring tooltip;
                for (size_t w = shown; w < ws.windows.size(); w++) {
                    if (!tooltip.empty()) {
                        tooltip += L"\n";
                    }
                    tooltip += ws.windows[w].title;
                }
                Controls::ToolTipService::SetToolTip(more,
                                                     winrt::box_value(tooltip));

                inner.Children().Append(more);
            }
        }

        pill.Child(inner);

        int monitorIndex = model.monitorIndex;
        int workspaceIndex = (int)i;
        pill.Tapped([=](auto const&, Input::TappedRoutedEventArgs const& e) {
            e.Handled(true);
            FocusWorkspace(monitorIndex, workspaceIndex);
        });

        children.Append(pill);
    }
}

// Runs a callback on each injected frame's UI thread, with the frame's panel.
void ForEachPanel(std::function<void(Controls::StackPanel)> callback,
                  bool wait = false) {
    std::vector<InjectedFrame> frames;
    {
        std::lock_guard lock(g_framesMutex);
        frames = g_frames;
    }

    std::vector<HANDLE> events;
    for (auto& frame : frames) {
        HANDLE doneEvent =
            wait ? CreateEvent(nullptr, TRUE, FALSE, nullptr) : nullptr;
        g_pendingCalls++;
        bool queued = frame.dispatcherQueue.TryEnqueue([weakPanel = frame.panel,
                                                        callback, doneEvent] {
            if (auto panel = weakPanel.get()) {
                try {
                    callback(panel);
                } catch (...) {
                    Wh_Log(L"Panel callback failed: %08X", winrt::to_hresult());
                }
            }
            if (doneEvent) {
                SetEvent(doneEvent);
            }
            g_pendingCalls--;
        });
        if (!queued) {
            g_pendingCalls--;
            if (doneEvent) {
                CloseHandle(doneEvent);
            }
        } else if (doneEvent) {
            events.push_back(doneEvent);
        }
    }

    for (HANDLE event : events) {
        WaitForSingleObject(event, 2000);
        CloseHandle(event);
    }
}

void InjectPanel(FrameworkElement frame) {
    auto rootGrid =
        FindChildByName(frame, L"RootGrid").try_as<Controls::Grid>();
    if (!rootGrid) {
        Wh_Log(L"RootGrid not found");
        return;
    }

    // Remove a leftover panel, e.g. from a previous mod instance.
    if (auto existing = FindChildByName(rootGrid, kPanelName)) {
        uint32_t index;
        if (rootGrid.Children().IndexOf(existing, index)) {
            rootGrid.Children().RemoveAt(index);
        }
    }

    Controls::StackPanel panel;
    panel.Name(kPanelName);
    panel.Orientation(Controls::Orientation::Horizontal);
    panel.VerticalAlignment(VerticalAlignment::Center);
    ApplyPanelLayout(panel);
    Controls::Canvas::SetZIndex(panel, 100);

    panel.PointerWheelChanged(
        [](winrt::Windows::Foundation::IInspectable const& sender,
           Input::PointerRoutedEventArgs const& e) {
            if (!g_settings.scrollToSwitchWorkspaces) {
                return;
            }
            int delta = e.GetCurrentPoint(sender.as<UIElement>())
                            .Properties()
                            .MouseWheelDelta();
            if (delta) {
                e.Handled(true);
                CycleWorkspace(/*next=*/delta < 0);
            }
        });
    if (int columns = rootGrid.ColumnDefinitions().Size(); columns > 1) {
        Controls::Grid::SetColumnSpan(panel, columns);
    }
    if (int rows = rootGrid.RowDefinitions().Size(); rows > 1) {
        Controls::Grid::SetRowSpan(panel, rows);
    }
    rootGrid.Children().Append(panel);

    {
        std::lock_guard lock(g_framesMutex);
        for (auto& injected : g_frames) {
            if (injected.frame.get() == frame) {
                injected.panel = winrt::make_weak(panel);
            }
        }
    }

    RenderPanel(panel);
    Wh_Log(L"Injected workspaces panel");
}

// Called on the taskbar UI thread with a TaskbarFrame element.
void EnsureInjected(FrameworkElement frame) {
    if (g_unloading) {
        return;
    }

    {
        std::lock_guard lock(g_framesMutex);
        std::erase_if(g_frames, [](const InjectedFrame& injected) {
            return !injected.frame.get();
        });
        for (const auto& injected : g_frames) {
            if (injected.frame.get() == frame) {
                return;
            }
        }

        auto dispatcherQueue =
            winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
        if (!dispatcherQueue) {
            Wh_Log(L"No dispatcher queue");
            return;
        }

        g_frames.push_back({
            .frame = winrt::make_weak(frame),
            .dispatcherQueue = dispatcherQueue,
        });

        // Don't modify the tree during layout, inject on the next tick.
        g_pendingCalls++;
        bool queued = dispatcherQueue.TryEnqueue([weakFrame =
                                                      winrt::make_weak(frame)] {
            if (auto frame = weakFrame.get(); frame && !g_unloading) {
                try {
                    InjectPanel(frame);
                } catch (...) {
                    Wh_Log(L"InjectPanel failed: %08X", winrt::to_hresult());
                }
            }
            g_pendingCalls--;
        });
        if (!queued) {
            g_pendingCalls--;
        }
    }
}

void RemovePanels() {
    ForEachPanel(
        [](Controls::StackPanel panel) {
            panel.Children().Clear();
            auto parent = Media::VisualTreeHelper::GetParent(panel)
                              .try_as<Controls::Panel>();
            if (!parent) {
                return;
            }
            uint32_t index;
            if (parent.Children().IndexOf(panel, index)) {
                parent.Children().RemoveAt(index);
            }
        },
        /*wait=*/true);

    std::lock_guard lock(g_framesMutex);
    g_frames.clear();
}

////////////////////////////////////////////////////////////////////////////////
// komorebi connection

void UpdateIcons(const Model& model) {
    int size = GetIconPixelSize();

    std::unordered_set<HWND> present;
    std::vector<const WindowInfo*> missing;
    {
        std::lock_guard lock(g_modelMutex);
        for (const auto& ws : model.workspaces) {
            for (const auto& window : ws.windows) {
                present.insert(window.hwnd);
                auto it = g_icons.find(window.hwnd);
                if (it == g_icons.end() || it->second.size != size) {
                    missing.push_back(&window);
                }
            }
        }
        std::erase_if(g_icons, [&present](const auto& item) {
            return !present.contains(item.first);
        });
    }

    for (const WindowInfo* window : missing) {
        IconPixels pixels;
        if (GetWindowIconPixels(window->hwnd, window->exe, size, &pixels)) {
            std::lock_guard lock(g_modelMutex);
            g_icons[window->hwnd] = std::move(pixels);
        }
    }
}

void HandleStateJson(std::string_view jsonUtf8) {
    if (g_unloading) {
        return;
    }

    Json::JsonObject root{nullptr};
    if (!Json::JsonObject::TryParse(winrt::to_hstring(jsonUtf8), root)) {
        Wh_Log(L"Failed to parse JSON (%zu bytes)", jsonUtf8.size());
        return;
    }

    // Notifications are {"event": ..., "state": ...}, `komorebic state` is the
    // bare state.
    Json::JsonObject state = ObjectAt(root, L"state");
    if (!state) {
        state = root;
    }

    Model model;
    if (!ParseState(state, &model)) {
        Wh_Log(L"Failed to parse komorebi state");
        return;
    }

    UpdateIcons(model);

    std::wstring signature = ModelSignature(model);
    {
        std::lock_guard lock(g_modelMutex);
        if (signature == g_modelSignature) {
            return;
        }
        g_modelSignature = std::move(signature);
        g_model = std::move(model);
    }

    ForEachPanel(RenderPanel);
}

// Waits for an overlapped operation, returns false if stopping or timed out.
bool WaitOverlapped(OVERLAPPED* ov, DWORD timeoutMs) {
    HANDLE handles[] = {ov->hEvent, g_stopEvent};
    DWORD result = WaitForMultipleObjects(2, handles, FALSE, timeoutMs);
    return result == WAIT_OBJECT_0;
}

void RunPipeSession(const std::wstring& pipeName) {
    std::wstring pipePath = L"\\\\.\\pipe\\" + pipeName;
    HANDLE pipe = CreateNamedPipe(
        pipePath.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 0, 1 << 20, 0,
        nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        Wh_Log(L"CreateNamedPipe failed: %u", GetLastError());
        return;
    }

    OVERLAPPED ov{.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr)};
    bool connected = false;

    if (ConnectNamedPipe(pipe, &ov)) {
        connected = true;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_PIPE_CONNECTED) {
            connected = true;
        } else if (error != ERROR_IO_PENDING) {
            Wh_Log(L"ConnectNamedPipe failed: %u", error);
        }
    }

    if (!connected) {
        // komorebi connects to the pipe as a client when asked to subscribe.
        RunKomorebic(L"subscribe-pipe " + pipeName);
        if (WaitOverlapped(&ov, 3000)) {
            DWORD transferred;
            connected = GetOverlappedResult(pipe, &ov, &transferred, FALSE);
        } else {
            CancelIoEx(pipe, &ov);
            DWORD transferred;
            GetOverlappedResult(pipe, &ov, &transferred, TRUE);
        }
    }

    if (connected) {
        Wh_Log(L"Connected to komorebi");

        // Notifications only arrive on the next event, so fetch the current
        // state right away.
        std::string initialState;
        if (RunKomorebic(L"state", &initialState)) {
            HandleStateJson(initialState);
        }

        std::string pending;
        std::vector<char> buffer(1 << 16);
        while (WaitForSingleObject(g_stopEvent, 0) != WAIT_OBJECT_0) {
            ResetEvent(ov.hEvent);
            DWORD read = 0;
            if (!ReadFile(pipe, buffer.data(), (DWORD)buffer.size(), &read,
                          &ov)) {
                DWORD error = GetLastError();
                if (error != ERROR_IO_PENDING) {
                    Wh_Log(L"ReadFile failed: %u", error);
                    break;
                }
                if (!WaitOverlapped(&ov, INFINITE)) {
                    CancelIoEx(pipe, &ov);
                    GetOverlappedResult(pipe, &ov, &read, TRUE);
                    break;
                }
                if (!GetOverlappedResult(pipe, &ov, &read, FALSE)) {
                    Wh_Log(L"Pipe closed: %u", GetLastError());
                    break;
                }
            }

            pending.append(buffer.data(), read);

            // Notifications are newline-delimited JSON. Only the latest one
            // matters, since each contains the full state.
            size_t lastNewline = pending.rfind('\n');
            if (lastNewline == std::string::npos) {
                continue;
            }
            size_t prevNewline = lastNewline == 0
                                     ? std::string::npos
                                     : pending.rfind('\n', lastNewline - 1);
            size_t start =
                prevNewline == std::string::npos ? 0 : prevNewline + 1;
            std::string_view line(pending.data() + start, lastNewline - start);
            if (!line.empty()) {
                HandleStateJson(line);
            }
            pending.erase(0, lastNewline + 1);
        }

        Wh_Log(L"Disconnected from komorebi");

        // Hide the workspaces until komorebi is back, rather than showing a
        // stale state.
        {
            std::lock_guard lock(g_modelMutex);
            g_model = {};
            g_modelSignature.clear();
        }
        if (!g_unloading) {
            ForEachPanel(RenderPanel);
        }
    }

    if (WaitForSingleObject(g_stopEvent, 0) == WAIT_OBJECT_0 && connected) {
        RunKomorebic(L"unsubscribe-pipe " + pipeName, nullptr, 1000);
    }

    CloseHandle(ov.hEvent);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

DWORD WINAPI KomorebiThread(LPVOID) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    std::wstring pipeName =
        L"komorebi-workspaces-" + std::to_wstring(GetCurrentProcessId());

    while (WaitForSingleObject(g_stopEvent, 0) != WAIT_OBJECT_0) {
        try {
            RunPipeSession(pipeName);
        } catch (...) {
            Wh_Log(L"Pipe session failed: %08X", winrt::to_hresult());
        }

        // Retry later, e.g. komorebi isn't running yet or was restarted.
        WaitForSingleObject(g_stopEvent, 3000);
    }

    winrt::uninit_apartment();
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Hooks

using TaskbarFrame_MeasureOverride_t =
    int(WINAPI*)(void* pThis,
                 winrt::Windows::Foundation::Size size,
                 winrt::Windows::Foundation::Size* resultSize);
TaskbarFrame_MeasureOverride_t TaskbarFrame_MeasureOverride_Original;
int WINAPI TaskbarFrame_MeasureOverride_Hook(
    void* pThis,
    winrt::Windows::Foundation::Size size,
    winrt::Windows::Foundation::Size* resultSize) {
    g_hookCallCounter++;

    int ret = TaskbarFrame_MeasureOverride_Original(pThis, size, resultSize);

    if (!g_unloading) {
        try {
            FrameworkElement frame = nullptr;
            ((IUnknown*)pThis)
                ->QueryInterface(winrt::guid_of<FrameworkElement>(),
                                 winrt::put_abi(frame));
            if (frame) {
                EnsureInjected(frame);
            }
        } catch (...) {
            Wh_Log(L"MeasureOverride hook failed: %08X", winrt::to_hresult());
        }
    }

    g_hookCallCounter--;
    return ret;
}

// Called on taskbar button hover and state changes. Used to inject into a
// taskbar that was already laid out before the mod was loaded.
using TaskListButton_UpdateVisualStates_t = void(WINAPI*)(void* pThis);
TaskListButton_UpdateVisualStates_t TaskListButton_UpdateVisualStates_Original;
void WINAPI TaskListButton_UpdateVisualStates_Hook(void* pThis) {
    g_hookCallCounter++;

    TaskListButton_UpdateVisualStates_Original(pThis);

    if (!g_unloading) {
        try {
            FrameworkElement element = nullptr;
            ((IUnknown*)pThis + 3)
                ->QueryInterface(winrt::guid_of<FrameworkElement>(),
                                 winrt::put_abi(element));
            while (element) {
                if (winrt::get_class_name(element) == L"Taskbar.TaskbarFrame") {
                    EnsureInjected(element);
                    break;
                }
                element = Media::VisualTreeHelper::GetParent(element)
                              .try_as<FrameworkElement>();
            }
        } catch (...) {
            Wh_Log(L"UpdateVisualStates hook failed: %08X",
                   winrt::to_hresult());
        }
    }

    g_hookCallCounter--;
}

bool HookTaskbarViewDllSymbols(HMODULE module) {
    // Taskbar.View.dll, ExplorerExtensions.dll
    WindhawkUtils::SYMBOL_HOOK symbolHooks[] = {
        {
            {LR"(public: virtual int __cdecl winrt::impl::produce<struct winrt::Taskbar::implementation::TaskbarFrame,struct winrt::Windows::UI::Xaml::IFrameworkElementOverrides>::MeasureOverride(struct winrt::Windows::Foundation::Size,struct winrt::Windows::Foundation::Size *))"},
            &TaskbarFrame_MeasureOverride_Original,
            TaskbarFrame_MeasureOverride_Hook,
        },
        {
            {LR"(private: void __cdecl winrt::Taskbar::implementation::TaskListButton::UpdateVisualStates(void))"},
            &TaskListButton_UpdateVisualStates_Original,
            TaskListButton_UpdateVisualStates_Hook,
            true,
        },
    };

    if (!HookSymbols(module, symbolHooks, ARRAYSIZE(symbolHooks))) {
        Wh_Log(L"HookSymbols failed");
        return false;
    }

    return true;
}

HMODULE GetTaskbarViewModuleHandle() {
    HMODULE module = GetModuleHandle(L"Taskbar.View.dll");
    if (!module) {
        module = GetModuleHandle(L"ExplorerExtensions.dll");
    }
    return module;
}

std::atomic<bool> g_taskbarViewDllLoaded;

using LoadLibraryExW_t = decltype(&LoadLibraryExW);
LoadLibraryExW_t LoadLibraryExW_Original;
HMODULE WINAPI LoadLibraryExW_Hook(LPCWSTR lpLibFileName,
                                   HANDLE hFile,
                                   DWORD dwFlags) {
    HMODULE module = LoadLibraryExW_Original(lpLibFileName, hFile, dwFlags);
    if (module && !g_taskbarViewDllLoaded &&
        GetTaskbarViewModuleHandle() == module &&
        !g_taskbarViewDllLoaded.exchange(true)) {
        Wh_Log(L"Loaded %s", lpLibFileName);
        if (HookTaskbarViewDllSymbols(module)) {
            Wh_ApplyHookOperations();
        }
    }
    return module;
}

////////////////////////////////////////////////////////////////////////////////
// Mod lifecycle

void LoadSettings() {
    {
        std::lock_guard lock(g_settingsStringsMutex);

        PCWSTR path = Wh_GetStringSetting(L"komorebicPath");
        g_settings.komorebicPath = path;
        Wh_FreeStringSetting(path);
        if (g_settings.komorebicPath.empty()) {
            g_settings.komorebicPath =
                L"C:\\Program Files\\komorebi\\bin\\komorebic-no-console.exe";
        }

        PCWSTR fontFamily = Wh_GetStringSetting(L"fontFamily");
        g_settings.fontFamily = fontFamily;
        Wh_FreeStringSetting(fontFamily);
    }

    PCWSTR position = Wh_GetStringSetting(L"position");
    if (wcscmp(position, L"center") == 0) {
        g_settings.position = Position::center;
    } else if (wcscmp(position, L"right") == 0) {
        g_settings.position = Position::right;
    } else {
        g_settings.position = Position::left;
    }
    Wh_FreeStringSetting(position);

    g_settings.horizontalOffset = Wh_GetIntSetting(L"horizontalOffset");
    g_settings.showEmptyWorkspaces = Wh_GetIntSetting(L"showEmptyWorkspaces");
    g_settings.showWorkspaceNames = Wh_GetIntSetting(L"showWorkspaceNames");
    g_settings.showWindowIcons = Wh_GetIntSetting(L"showWindowIcons");
    g_settings.maxIconsPerWorkspace =
        std::max(Wh_GetIntSetting(L"maxIconsPerWorkspace"), 0);
    g_settings.scrollToSwitchWorkspaces =
        Wh_GetIntSetting(L"scrollToSwitchWorkspaces");
    g_settings.iconSize = std::clamp(Wh_GetIntSetting(L"iconSize"), 8, 64);
    g_settings.fontSize = std::clamp(Wh_GetIntSetting(L"fontSize"), 6, 32);
    g_settings.workspacePadding =
        std::clamp(Wh_GetIntSetting(L"workspacePadding"), 0, 32);
    g_settings.workspaceSpacing =
        std::clamp(Wh_GetIntSetting(L"workspaceSpacing"), 0, 32);

    PCWSTR accent = Wh_GetStringSetting(L"accentColor");
    g_settings.accentColor = ParseColor(accent, {0xFF, 0xFA, 0xA9, 0x68});
    Wh_FreeStringSetting(accent);

    PCWSTR text = Wh_GetStringSetting(L"textColor");
    g_settings.textColor = ParseColor(text, {0xFF, 0xF6, 0xDC, 0xAC});
    Wh_FreeStringSetting(text);

    g_settings.inactiveOpacity =
        std::clamp(Wh_GetIntSetting(L"inactiveOpacity"), 0, 100) / 100.0;
    g_settings.focusedBackgroundAlpha = (BYTE)MulDiv(
        std::clamp(Wh_GetIntSetting(L"focusedBackgroundOpacity"), 0, 100), 255,
        100);
    g_settings.inactiveBackgroundAlpha = (BYTE)MulDiv(
        std::clamp(Wh_GetIntSetting(L"inactiveBackgroundOpacity"), 0, 100), 255,
        100);
    g_settings.hoverBackgroundAlpha = (BYTE)MulDiv(
        std::clamp(Wh_GetIntSetting(L"hoverBackgroundOpacity"), 0, 100), 255,
        100);
}

BOOL Wh_ModInit() {
    Wh_Log(L">");

    LoadSettings();

    if (HMODULE taskbarViewModule = GetTaskbarViewModuleHandle()) {
        g_taskbarViewDllLoaded = true;
        if (!HookTaskbarViewDllSymbols(taskbarViewModule)) {
            return FALSE;
        }
    } else {
        Wh_Log(L"Taskbar view module not loaded yet");
        HMODULE kernelBaseModule = GetModuleHandle(L"kernelbase.dll");
        auto pKernelBaseLoadLibraryExW =
            (decltype(&LoadLibraryExW))GetProcAddress(kernelBaseModule,
                                                      "LoadLibraryExW");
        WindhawkUtils::SetFunctionHook(pKernelBaseLoadLibraryExW,
                                       LoadLibraryExW_Hook,
                                       &LoadLibraryExW_Original);
    }

    return TRUE;
}

void Wh_ModAfterInit() {
    Wh_Log(L">");

    if (!g_taskbarViewDllLoaded) {
        if (HMODULE taskbarViewModule = GetTaskbarViewModuleHandle()) {
            if (!g_taskbarViewDllLoaded.exchange(true)) {
                if (HookTaskbarViewDllSymbols(taskbarViewModule)) {
                    Wh_ApplyHookOperations();
                }
            }
        }
    }

    g_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    g_komorebiThread =
        CreateThread(nullptr, 0, KomorebiThread, nullptr, 0, nullptr);

    // Ask the taskbar to re-layout so that the panel gets injected right away.
    // Otherwise it's injected on the next taskbar layout or button hover.
    if (HWND taskbarWnd = FindCurrentProcessTaskbarWnd()) {
        PostMessage(taskbarWnd, WM_SETTINGCHANGE, SPI_SETLOGICALDPIOVERRIDE, 0);
    }
}

void Wh_ModBeforeUninit() {
    Wh_Log(L">");

    g_unloading = true;

    if (g_komorebiThread) {
        SetEvent(g_stopEvent);
        WaitForSingleObject(g_komorebiThread, 5000);
        CloseHandle(g_komorebiThread);
        g_komorebiThread = nullptr;
    }

    RemovePanels();
}

void Wh_ModUninit() {
    Wh_Log(L">");

    for (int i = 0; i < 100 && (g_hookCallCounter > 0 || g_pendingCalls > 0);
         i++) {
        Sleep(100);
    }

    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }
}

void Wh_ModSettingsChanged() {
    Wh_Log(L">");

    LoadSettings();

    {
        std::lock_guard lock(g_modelMutex);
        g_modelSignature.clear();
    }

    // Icons are re-fetched at a new size on the next komorebi event; render
    // the current model with the new settings now.
    ForEachPanel(RenderPanel);
}
