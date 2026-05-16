# MiniCap - Lightweight Screen Capture Tool

A lightweight, minimalist screen region capture tool for Windows, written in C++ using the Win32 API.

## Features

- **Region Selection Screenshot**: Press `Ctrl + Alt + P` to activate the crosshair overlay and drag to select a screen region.
- **Clipboard Integration**: The captured region is automatically copied to the clipboard as a bitmap image.
- **High-DPI Support**: Automatically adapts to high-DPI / system-scaled displays to ensure accurate capture.
- **Minimal Footprint**: Runs silently in the background with no system tray icon or visible window.
- **Escape to Cancel**: Press `Esc` during selection to abort.

## Hotkeys

| Hotkey | Action |
|--------|--------|
| `Ctrl + Alt + P` | Activate region selection overlay |
| `Ctrl + Alt + Q` | Exit the program completely |

## Usage

1. Launch `screenshot.exe` — the program runs silently in the background.
2. Press `Ctrl + Alt + P` to bring up the full-screen overlay.
3. Click and drag to select the region you want to capture.
4. Release the mouse button — the selected region is automatically copied to the clipboard.
5. A success message will confirm the operation.
6. Press `Ctrl + Alt + Q` to exit the program.

## Build

Compile with any C++ compiler that supports the Win32 API (e.g., MSVC, MinGW-w64):

```bash
# Using MSVC (Visual Studio Command Prompt)
cl screenshot.cpp /Fe:screenshot.exe /EHsc

# Using MinGW-w64
g++ screenshot.cpp -o screenshot.exe -lgdi32 -mwindows
```

## Requirements

- Windows Vista or later
- No external dependencies (pure Win32 API)

## License

MIT
