// leanflutter/window_manager#547 (#76): with the title bar hidden, the window keeps a
// border on all four sides. Windows 10 draws the frame only where the window still has
// non-client area, so one pixel of the top sizing frame stays with the system there;
// Windows 11 draws the border over the client area and leaves the client at the top edge.
// A maximized or full screen window has no top strip, and neither has an explicitly
// frameless one, where no compositor frame is left to draw a border. The window is
// shown without activation at the bottom of the Z order; no input.
#include <windows.h>
#include <iostream>
#include "nativeapi.h"

using namespace nativeapi;

namespace {
int failures = 0;
void Check(bool ok, const char* label) {
  std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl;
  failures += !ok;
}
void Pump() {
  MSG message;
  for (int i = 0; i < 20; ++i) {
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    Sleep(10);
  }
}

// GetVersionExW lies to manifest-less processes; RtlGetVersion reports the real build.
DWORD WindowsBuildNumber() {
  using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  auto rtl_get_version =
      ntdll ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;
  RTL_OSVERSIONINFOW info = {};
  info.dwOSVersionInfoSize = sizeof(info);
  if (!rtl_get_version || rtl_get_version(&info) != 0)
    return 0;
  return info.dwBuildNumber;
}

// The client area's top-left in screen coordinates.
POINT ClientOrigin(HWND hwnd) {
  POINT origin = {0, 0};
  ClientToScreen(hwnd, &origin);
  return origin;
}
}  // namespace

int main() {
  SetProcessDPIAware();
  Application::GetInstance();

  Window window;
  window.SetBounds({200, 200, 640, 400});
  window.SetTitleBarStyle(TitleBarStyle::Hidden);
  HWND root = static_cast<HWND>(window.GetNativeObject());
  Check(root != nullptr, "the window is created");
  if (!root) {
    std::cout << "FAILED" << std::endl;
    return 1;
  }

  // A content window covering the client area, as a Flutter view does.
  WNDCLASSW cls = {};
  cls.lpfnWndProc = DefWindowProcW;
  cls.hInstance = GetModuleHandleW(nullptr);
  cls.hbrBackground = static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH));
  cls.lpszClassName = L"NativeApiHiddenFrameContent";
  Check(RegisterClassW(&cls) != 0, "the content class is registered");
  RECT client;
  GetClientRect(root, &client);
  HWND content =
      CreateWindowExW(0, cls.lpszClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, client.right,
                      client.bottom, root, nullptr, cls.hInstance, nullptr);
  Check(content != nullptr, "the content window is created");
  if (!content) {
    DestroyWindow(root);
    std::cout << "FAILED" << std::endl;
    return 1;
  }

  ShowWindow(root, SW_SHOWNOACTIVATE);  // Child hit testing skips hidden windows.
  SetWindowPos(root, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  Pump();

  RECT rect;
  GetWindowRect(root, &rect);
  const POINT origin = ClientOrigin(root);
  // Windows 11 draws the border over the client area; earlier versions need the strip.
  const int expected_top = WindowsBuildNumber() >= 22000 ? 0 : 1;
  Check(origin.y - rect.top == expected_top,
        expected_top ? "the top sizing frame stays with the system" : "the client reaches the top");
  Check(origin.x - rect.left > 0, "the left sizing frame stays with the system");
  GetClientRect(root, &client);
  Check(rect.bottom > origin.y + client.bottom, "the bottom sizing frame stays with the system");

  // The content starts below the strip, so it cannot paint over the border, and it steps
  // aside so the strip still resizes the window.
  Check(ClientOrigin(content).y == origin.y, "the content stops at the client's top edge");
  const int x = rect.left + (rect.right - rect.left) / 2;
  const LPARAM top = MAKELPARAM(x, rect.top + 1);
  Check(SendMessageW(root, WM_NCHITTEST, 0, top) == HTTOP, "the top edge answers HTTOP");
  Check(SendMessageW(content, WM_NCHITTEST, 0, top) == HTTRANSPARENT,
        "the content lets the top edge hit test through");

  window.Maximize();
  Pump();
  MONITORINFO monitor = {sizeof(monitor)};
  GetMonitorInfoW(MonitorFromWindow(root, MONITOR_DEFAULTTONEAREST), &monitor);
  const POINT maximized = ClientOrigin(root);
  Check(maximized.y == monitor.rcWork.top,
        "a maximized window has no top strip: the client starts at the work area");
  window.Unmaximize();
  Pump();
  GetWindowRect(root, &rect);
  const POINT restored = ClientOrigin(root);
  Check(restored.y - rect.top == expected_top, "restoring brings the top strip back");

  // Full screen has no frame to draw a border on, so no strip either.
  window.SetFullScreen(true);
  Pump();
  Check(window.IsFullScreen(), "it goes full screen");
  monitor = {sizeof(monitor)};
  GetMonitorInfoW(MonitorFromWindow(root, MONITOR_DEFAULTTONEAREST), &monitor);
  const POINT full = ClientOrigin(root);
  Check(full.y == monitor.rcMonitor.top, "full screen starts at the top of the monitor");
  window.SetFullScreen(false);
  Pump();

  // The frameless path never gets the strip, at any build: with no compositor frame
  // there is no border for it to keep, and an inset would leave a 1px band that
  // nothing paints. Only a pre-Windows 11 runner can catch an inset leaking here.
  window.SetFrameless(true);
  Pump();
  Check(window.IsFrameless(), "it goes explicitly frameless");
  GetWindowRect(root, &rect);
  const POINT frameless = ClientOrigin(root);
  Check(frameless.x == rect.left && frameless.y == rect.top,
        "a frameless window has no strip: the client covers the whole window");
  window.SetFrameless(false);
  Pump();
  Check(!window.IsFrameless(), "it leaves the frameless state");
  GetWindowRect(root, &rect);
  const POINT reframed = ClientOrigin(root);
  Check(reframed.y - rect.top >= GetSystemMetrics(SM_CYCAPTION),
        "leaving frameless restores the native caption");

  DestroyWindow(root);
  std::cout << (failures ? "FAILED" : "OK") << std::endl;
  return failures ? 1 : 0;
}
