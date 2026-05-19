#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <algorithm> // Header required for std::min
#include <cstdlib>   // Header required for std::abs
#include <cstdio>    // Header required for sprintf_s

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

// Global variables
bool isSelecting = false;
POINT ptStart, ptEnd;
POINT ptCursor = {0, 0};   // Current mouse cursor position for real-time color display
HBITMAP hFullScreenshot = NULL;
HWND hOverlayWnd = NULL;

// Capture the entire screen and return the bitmap handle
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

// Copy the specified region to the clipboard
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

// Get the color of a pixel from the captured screenshot bitmap
COLORREF GetScreenshotPixel(int x, int y) {
    HDC hdc = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hdc);
    SelectObject(hMemDC, hFullScreenshot);
    COLORREF color = GetPixel(hMemDC, x, y);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hdc);
    return color;
}

// Copy a string to the clipboard as Unicode text
bool CopyStringToClipboard(const char* str) {
    int len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar_t));
    if (!hGlobal) return false;

    wchar_t* pData = (wchar_t*)GlobalLock(hGlobal);
    MultiByteToWideChar(CP_ACP, 0, str, -1, pData, len);
    GlobalUnlock(hGlobal);

    bool success = false;
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        success = (SetClipboardData(CF_UNICODETEXT, hGlobal) != NULL);
        CloseClipboard();
    }

    if (!success) {
        GlobalFree(hGlobal);
    }
    return success;
}

// Draw the pixel color info (swatch + hex text) near the cursor on the given DC.
// hdcSource must be a DC that already has hFullScreenshot selected into it.
void DrawColorInfo(HDC hdc, HDC hdcSource, int cursorX, int cursorY, int screenW, int screenH) {
    COLORREF pixelColor = GetPixel(hdcSource, cursorX, cursorY);
    
    char hexStr[16];
    sprintf_s(hexStr, sizeof(hexStr), "#%02X%02X%02X", GetRValue(pixelColor), GetGValue(pixelColor), GetBValue(pixelColor));
    
    int swatchSize = 18;
    int padding = 5;
    int textLen = (int)strlen(hexStr);
    int textWidth = textLen * 8;
    int totalWidth = swatchSize + padding + textWidth + padding * 2;
    int totalHeight = swatchSize + padding * 2;
    
    // Position info box near cursor (below and to the right)
    int infoX = cursorX + 18;
    int infoY = cursorY + 18;
    
    // Keep within screen bounds
    if (infoX + totalWidth > screenW) infoX = cursorX - totalWidth - 18;
    if (infoY + totalHeight > screenH) infoY = cursorY - totalHeight - 18;
    if (infoX < 0) infoX = 2;
    if (infoY < 0) infoY = 2;
    
    // Draw semi-transparent-like dark background
    HBRUSH hBgBrush = CreateSolidBrush(RGB(30, 30, 30));
    RECT bgRect = {infoX, infoY, infoX + totalWidth, infoY + totalHeight};
    FillRect(hdc, &bgRect, hBgBrush);
    DeleteObject(hBgBrush);
    
    // Draw white border around the info box
    HBRUSH hBorderBrush = CreateSolidBrush(RGB(200, 200, 200));
    FrameRect(hdc, &bgRect, hBorderBrush);
    DeleteObject(hBorderBrush);
    
    // Draw color swatch
    HBRUSH hColorBrush = CreateSolidBrush(pixelColor);
    RECT swatchRect = {infoX + padding, infoY + padding, infoX + padding + swatchSize, infoY + padding + swatchSize};
    FillRect(hdc, &swatchRect, hColorBrush);
    DeleteObject(hColorBrush);
    
    // Draw thin white border around swatch
    HBRUSH hSwatchBorder = CreateSolidBrush(RGB(255, 255, 255));
    FrameRect(hdc, &swatchRect, hSwatchBorder);
    DeleteObject(hSwatchBorder);
    
    // Draw hex text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOutA(hdc, infoX + padding + swatchSize + padding, infoY + padding + 2, hexStr, textLen);
}

// Overlay window callback procedure
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
            // Always update cursor position for real-time color display
            ptCursor.x = LOWORD(lParam);
            ptCursor.y = HIWORD(lParam);
            if (isSelecting) {
                ptEnd = ptCursor;
            }
            // Force synchronous redraw so the color info follows the cursor in real-time
            RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
            return 0;

        case WM_LBUTTONUP:
            if (isSelecting) {
                isSelecting = false;
                ReleaseCapture();

                // Calculate region using std::min and std::abs
                int x = std::min(ptStart.x, ptEnd.x);
                int y = std::min(ptStart.y, ptEnd.y);
                int w = std::abs(ptStart.x - ptEnd.x);
                int h = std::abs(ptStart.y - ptEnd.y);

                if (w == 0 && h == 0) {
                    // Simple click without dragging: pick pixel color instead of screenshot
                    COLORREF color = GetScreenshotPixel(ptStart.x, ptStart.y);
                    char hexStr[16];
                    sprintf_s(hexStr, sizeof(hexStr), "#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));

                    if (CopyStringToClipboard(hexStr)) {
                        wchar_t msgBuf[128];
                        swprintf_s(msgBuf, L"Color hex value \"%S\" has been copied to the clipboard.", hexStr);
                        MessageBoxW(NULL, msgBuf, L"Color Picked", MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL);
                    } else {
                        MessageBoxW(NULL, L"Failed to copy color to clipboard!", L"Error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
                    }
                    
                    DestroyWindow(hwnd);
                    return 0;
                }

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
                    MessageBoxW(NULL, L"Screenshot region has been copied to the clipboard!", L"Screenshot Success", MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL);
                } else if (w > 0 && h > 0) {
                    MessageBoxW(NULL, L"Failed to write to clipboard!", L"Error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
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
                
                // Calculate region using std::min and std::abs
                int x = std::min(ptStart.x, ptEnd.x);
                int y = std::min(ptStart.y, ptEnd.y);
                int w = std::abs(ptStart.x - ptEnd.x);
                int h = std::abs(ptStart.y - ptEnd.y);
                
                RECT r = {x, y, x + w, y + h};
                FrameRect(hMemDC, &r, hBrush);
                DeleteObject(hBrush);
            }

            // Draw real-time pixel color info at cursor position.
            // hScreenDC already has hFullScreenshot selected — pass it as the pixel source
            // so GetPixel reads from the screenshot without attempting a second SelectObject.
            DrawColorInfo(hMemDC, hScreenDC, ptCursor.x, ptCursor.y, sw, sh);

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
    // Fix screenshot scaling issues on high-DPI / system-scaled displays
    SetProcessDPIAware();

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"LightweightSnippingOverlay";
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    RegisterClassW(&wc);

    if (!RegisterHotKey(NULL, 1, MOD_CONTROL | MOD_ALT, 'P')) {
        MessageBoxW(NULL, L"Hotkey Ctrl+Alt+P registration failed, may be occupied by another program!", L"Startup Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    RegisterHotKey(NULL, 2, MOD_CONTROL | MOD_ALT, 'Q');

    MessageBoxW(NULL, L"Snipping tool is running silently in the background.\n\n- Press [Ctrl + Alt + P] to start region snip (or color pick)\n- Press [Ctrl + Alt + Q] to exit the program completely", L"Startup Success", MB_OK | MB_ICONINFORMATION);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            if (msg.wParam == 1 && hOverlayWnd == NULL) { 
                hFullScreenshot = CaptureScreen();
                int w = GetSystemMetrics(SM_CXSCREEN);
                int h = GetSystemMetrics(SM_CYSCREEN);
                
                // Initialize cursor position for immediate color display
                GetCursorPos(&ptCursor);
                
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