#include <windows.h>
#include <stdio.h>
#include "matrix.h"

static Matrix* matrix = NULL;
static BOOL running = TRUE;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
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
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);

    if (!RegisterClassW(&wc)) {
        OutputDebugStringW(L"WinMain: Failed to register window class\n");
        MessageBoxW(NULL, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    OutputDebugStringW(L"WinMain: Screen dimensions: %dx%d\n");

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
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

    matrix = Matrix_init(hwnd, screenWidth, screenHeight);
    if (!matrix) {
        OutputDebugStringW(L"WinMain: Failed to initialize Matrix\n");
        MessageBoxW(NULL, L"Failed to initialize Matrix", L"Error", MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd);
        return 1;
    }

    OutputDebugStringW(L"WinMain: Matrix initialized successfully\n");

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
        Sleep(10);
    }

    OutputDebugStringW(L"WinMain: Cleaning up\n");
    Matrix_deinit(matrix);
    DestroyWindow(hwnd);
    OutputDebugStringW(L"WinMain: Exiting\n");
    return 0;
}