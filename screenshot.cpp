#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <algorithm> // 引入 std::min 需要的头文件
#include <cstdlib>   // 引入 std::abs 需要的头文件

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

// 全局变量
bool isSelecting = false;
POINT ptStart, ptEnd;
HBITMAP hFullScreenshot = NULL;
HWND hOverlayWnd = NULL;

// 截取当前整个屏幕并返回位图句柄
HBITMAP CaptureScreen() {
    HDC hScreen = GetDC(NULL);
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    HDC hMemDC = CreateCompatibleDC(hScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, w, h);
    SelectObject(hMemDC, hBmp);
    BitBlt(hMemDC, 0, 0, w, h, hScreen, 0, 0, SRCCOPY);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreen);
    return hBmp;
}

// 拷贝指定区域到剪贴板
bool CopyRegionToClipboard(HDC hdcFrozen, int x, int y, int w, int h) {
    bool success = false;
    HDC hScreen = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, w, h);
    SelectObject(hMemDC, hBmp);
    
    BitBlt(hMemDC, 0, 0, w, h, hdcFrozen, x, y, SRCCOPY);

    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        if (SetClipboardData(CF_BITMAP, hBmp) != NULL) {
            success = true;
        }
        CloseClipboard();
    }
    
    if (!success) {
        DeleteObject(hBmp);
    }
    
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreen);
    return success;
}

// 遮罩窗口的回调函数
LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_LBUTTONDOWN:
            isSelecting = true;
            ptStart.x = LOWORD(lParam);
            ptStart.y = HIWORD(lParam);
            ptEnd = ptStart;
            SetCapture(hwnd);
            return 0;

        case WM_MOUSEMOVE:
            if (isSelecting) {
                ptEnd.x = LOWORD(lParam);
                ptEnd.y = HIWORD(lParam);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;

        case WM_LBUTTONUP:
            if (isSelecting) {
                isSelecting = false;
                ReleaseCapture();

                // 修复点：使用 std::min 和 std::abs
                int x = std::min(ptStart.x, ptEnd.x);
                int y = std::min(ptStart.y, ptEnd.y);
                int w = std::abs(ptStart.x - ptEnd.x);
                int h = std::abs(ptStart.y - ptEnd.y);

                bool copied = false;
                if (w > 0 && h > 0) {
                    HDC hdc = GetDC(hwnd);
                    HDC hdcMem = CreateCompatibleDC(hdc);
                    SelectObject(hdcMem, hFullScreenshot);
                    copied = CopyRegionToClipboard(hdcMem, x, y, w, h);
                    DeleteDC(hdcMem);
                    ReleaseDC(hwnd, hdc);
                }

                DestroyWindow(hwnd);

                if (copied) {
                    MessageBoxW(NULL, L"区域截图已复制到剪贴板！", L"截图成功", MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL);
                } else if (w > 0 && h > 0) {
                    MessageBoxW(NULL, L"写入剪贴板失败！", L"错误", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
                }
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);

            HDC hMemDC = CreateCompatibleDC(hdc);
            HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, sw, sh);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hMemBmp);

            HDC hScreenDC = CreateCompatibleDC(hdc);
            HBITMAP hOldScreen = (HBITMAP)SelectObject(hScreenDC, hFullScreenshot);

            BitBlt(hMemDC, 0, 0, sw, sh, hScreenDC, 0, 0, SRCCOPY);

            if (isSelecting) {
                HBRUSH hBrush = CreateSolidBrush(RGB(255, 0, 0));
                
                // 修复点：使用 std::min 和 std::abs
                int x = std::min(ptStart.x, ptEnd.x);
                int y = std::min(ptStart.y, ptEnd.y);
                int w = std::abs(ptStart.x - ptEnd.x);
                int h = std::abs(ptStart.y - ptEnd.y);
                
                RECT r = {x, y, x + w, y + h};
                FrameRect(hMemDC, &r, hBrush);
                DeleteObject(hBrush);
            }

            BitBlt(hdc, 0, 0, sw, sh, hMemDC, 0, 0, SRCCOPY);

            SelectObject(hScreenDC, hOldScreen);
            DeleteDC(hScreenDC);
            SelectObject(hMemDC, hOldBmp);
            DeleteObject(hMemBmp);
            DeleteDC(hMemDC);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_DESTROY:
            if (hFullScreenshot) {
                DeleteObject(hFullScreenshot);
                hFullScreenshot = NULL;
            }
            hOverlayWnd = NULL;
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main() {
    // 解决高分辨率/系统缩放导致的截图放大和不全问题
    SetProcessDPIAware();

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"LightweightSnippingOverlay";
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    RegisterClassW(&wc);

    if (!RegisterHotKey(NULL, 1, MOD_CONTROL | MOD_ALT, 'P')) {
        MessageBoxW(NULL, L"快捷键 Ctrl+Alt+P 注册失败，可能被其他软件占用！", L"启动错误", MB_OK | MB_ICONERROR);
        return 1;
    }
    RegisterHotKey(NULL, 2, MOD_CONTROL | MOD_ALT, 'Q');

    MessageBoxW(NULL, L"截图工具已在后台静默运行。\n\n- 按 [Ctrl + Alt + P] 唤醒区域截图\n- 按 [Ctrl + Alt + Q] 彻底退出程序", L"启动成功", MB_OK | MB_ICONINFORMATION);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            if (msg.wParam == 1 && hOverlayWnd == NULL) { 
                hFullScreenshot = CaptureScreen();
                int w = GetSystemMetrics(SM_CXSCREEN);
                int h = GetSystemMetrics(SM_CYSCREEN);
                
                hOverlayWnd = CreateWindowExW(
                    WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                    L"LightweightSnippingOverlay", NULL,
                    WS_POPUP | WS_VISIBLE,
                    0, 0, w, h,
                    NULL, NULL, GetModuleHandle(NULL), NULL
                );
                SetForegroundWindow(hOverlayWnd);
            } 
            else if (msg.wParam == 2) {
                break;
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterHotKey(NULL, 1);
    UnregisterHotKey(NULL, 2);

    return 0;
}