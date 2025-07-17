#include <windows.h>
#include <stdio.h>
#include "matrix.h"
#include "../resource.h" // Include the new resource header

static Matrix* matrix = NULL;
static BOOL running = TRUE;
static POINT initialMousePos = { -1, -1 }; // Store initial mouse position
static const int MOUSE_MOVE_THRESHOLD = 10; // Pixels to move before exiting

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        // Handle the message to set the cursor
        case WM_SETCURSOR:
            // Set the cursor to NULL (invisible) and indicate we've handled the message.
            SetCursor(NULL);
            return TRUE;
        case WM_DESTROY:
            OutputDebugStringW(L"WndProc: WM_DESTROY received\n");
            running = FALSE;
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            OutputDebugStringW(L"WndProc: WM_KEYDOWN received\n");
            running = FALSE;
            PostQuitMessage(0);
            return 0;
        case WM_LBUTTONDOWN:
            OutputDebugStringW(L"WndProc: WM_LBUTTONDOWN received\n");
            running = FALSE;
            PostQuitMessage(0);
            return 0;
        case WM_MOUSEMOVE: {
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);
            WCHAR debugMsg[256];
            wsprintfW(debugMsg, L"WndProc: WM_MOUSEMOVE received at (%d, %d)\n", xPos, yPos);
            OutputDebugStringW(debugMsg);

            // Initialize mouse position on first movement
            if (initialMousePos.x == -1 && initialMousePos.y == -1) {
                initialMousePos.x = xPos;
                initialMousePos.y = yPos;
                OutputDebugStringW(L"WndProc: Initial mouse position set\n");
                return 0;
            }

            // Calculate distance moved
            int dx = abs(xPos - initialMousePos.x);
            int dy = abs(yPos - initialMousePos.y);
            if (dx > MOUSE_MOVE_THRESHOLD || dy > MOUSE_MOVE_THRESHOLD) {
                OutputDebugStringW(L"WndProc: Mouse moved beyond threshold, exiting\n");
                running = FALSE;
                PostQuitMessage(0);
            }
            return 0;
        }
        case WM_PAINT: {
            OutputDebugStringW(L"WndProc: WM_PAINT received\n");
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (matrix) {
                Matrix_render(matrix, hdc);
            } else {
                OutputDebugStringW(L"WndProc: matrix is NULL in WM_PAINT\n");
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    OutputDebugStringW(L"WinMain: Starting Matrix screensaver\n");

    const wchar_t* className = L"MatrixWndClass";
    WNDCLASSW wc = { 0 };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = className;
    // We no longer need to set a class cursor since we handle WM_SETCURSOR
    wc.hCursor = NULL; 
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1));

    if (!RegisterClassW(&wc)) {
        OutputDebugStringW(L"WinMain: Failed to register window class\n");
        MessageBoxW(NULL, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    WCHAR debugMsg[256];
    wsprintfW(debugMsg, L"WinMain: Screen dimensions: %d x %d\n", screenWidth, screenHeight);
    OutputDebugStringW(debugMsg);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST, // Use WS_EX_TOPMOST for true full-screen
        className,
        L"Matrix Screensaver",
        WS_POPUP,
        0,
        0,
        screenWidth,
        screenHeight,
        NULL,
        NULL,
        hInstance,
        NULL
    );
    if (!hwnd) {
        OutputDebugStringW(L"WinMain: Failed to create window\n");
        MessageBoxW(NULL, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    OutputDebugStringW(L"WinMain: Window created successfully\n");

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    // The ShowCursor calls are no longer needed here.

    matrix = Matrix_init(hwnd, screenWidth, screenHeight);
    if (!matrix) {
        OutputDebugStringW(L"WinMain: Failed to initialize Matrix\n");
        MessageBoxW(NULL, L"Failed to initialize Matrix", L"Error", MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd);
        return 1;
    }

    OutputDebugStringW(L"WinMain: Matrix initialized successfully\n");

    GetCursorPos(&initialMousePos);
    WCHAR initMsg[256];
    wsprintfW(initMsg, L"WinMain: Initial mouse position set to (%ld, %ld)\n", initialMousePos.x, initialMousePos.y);
    OutputDebugStringW(initMsg);

    MSG msg;
    while (running) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                OutputDebugStringW(L"WinMain: WM_QUIT received\n");
                running = FALSE;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        Matrix_update(matrix);
        InvalidateRect(hwnd, NULL, FALSE);
        Sleep(50);
    }

    OutputDebugStringW(L"WinMain: Cleaning up\n");
    // The cursor will be restored automatically by Windows when the app exits.
    Matrix_deinit(matrix);
    DestroyWindow(hwnd);
    OutputDebugStringW(L"WinMain: Exiting\n");
    return 0;
}
