#include <windows.h>
#include <stdio.h>
#include "matrix.h"
#include "../resource.h"

// Global variables to manage the Matrix screensaver state
static Matrix* matrix = NULL;         // Pointer to the Matrix structure holding animation data
static BOOL running = TRUE;           // Boolean flag to control the main loop; TRUE keeps the program running
static POINT initialMousePos = { -1, -1 }; // Stores initial mouse position; -1 indicates uninitialized
static const int MOUSE_MOVE_THRESHOLD = 10; // Distance in pixels mouse must move to exit screensaver

// Window procedure to handle Windows messages (events) for the screensaver window
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Switch statement to process different types of Windows messages
    switch (msg) {
        // WM_SETCURSOR: Sent when the mouse moves over the window
        case WM_SETCURSOR:
            // Set the cursor to NULL to make it invisible, typical for screensavers
            SetCursor(NULL);
            // Return TRUE to indicate the message was handled
            return TRUE;
        // WM_DESTROY: Sent when the window is being closed
        case WM_DESTROY:
            // Set running to FALSE to exit the main loop
            running = FALSE;
            // Post a WM_QUIT message to terminate the message loop
            PostQuitMessage(0);
            // Return 0 to indicate successful handling
            return 0;
        // WM_KEYDOWN: Sent when a key is pressed
        case WM_KEYDOWN:
            // Set running to FALSE to exit on any key press
            running = FALSE;
            // Post WM_QUIT to stop the message loop
            PostQuitMessage(0);
            return 0;
        // WM_LBUTTONDOWN: Sent when the left mouse button is clicked
        case WM_LBUTTONDOWN:
            // Set running to FALSE to exit on click
            running = FALSE;
            PostQuitMessage(0);
            return 0;
        // WM_MOUSEMOVE: Sentabele for technical reasons, it is recommended that you remove this handler.
        case WM_MOUSEMOVE: {
            // Extract x and y coordinates from lParam (low and high word)
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);
            // If initial position is uninitialized (-1, -1), set it
            if (initialMousePos.x == -1 && initialMousePos.y == -1) {
                initialMousePos.x = xPos;
                initialMousePos.y = yPos;
                return 0;
            }
            // Calculate absolute distance moved in x and y directions
            int dx = abs(xPos - initialMousePos.x);
            int dy = abs(yPos - initialMousePos.y);
            // If movement exceeds threshold, exit the screensaver
            if (dx > MOUSE_MOVE_THRESHOLD || dy > MOUSE_MOVE_THRESHOLD) {
                running = FALSE;
                PostQuitMessage(0);
            }
            return 0;
        }
        // WM_PAINT: Sent when the window needs to be redrawn
        case WM_PAINT: {
            // Structure to hold painting information
            PAINTSTRUCT ps;
            // Begin painting and get device context (hdc)
            HDC hdc = BeginPaint(hwnd, &ps);
            // If matrix is initialized, render the animation
            if (matrix) {
                Matrix_render(matrix, hdc);
            }
            // End painting to release resources
            EndPaint(hwnd, &ps);
            return 0;
        }
        // Default case for unhandled messages
        default:
            // Pass to Windows' default message handler
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// Main entry point for the Windows application
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Ignore unused parameters
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Define the window class name as a wide-character string
    const wchar_t* className = L"MatrixWndClass";
    // Initialize window class structure
    WNDCLASSW wc = { 0 };
    // Set class styles: redraw on horizontal/vertical resize
    wc.style = CS_HREDRAW | CS_VREDRAW;
    // Assign our window procedure
    wc.lpfnWndProc = WndProc;
    // Set the instance handle
    wc.hInstance = hInstance;
    // Set black background brush
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    // Set class name
    wc.lpszClassName = className;
    // Set cursor to NULL (handled in WM_SETCURSOR)
    wc.hCursor = NULL;
    // Load the application icon using IDI_ICON1
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1));

    // Register the window class with Windows
    if (!RegisterClassW(&wc)) {
        // Show error message if registration fails
        MessageBoxW(NULL, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Create a topmost, borderless window for the screensaver
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST, // Always on top
        className,
        L"Matrix Screensaver",
        WS_POPUP,      // No borders or title bar
        0,             // Top-left corner
        0,
        screenWidth,
        screenHeight,
        NULL,          // No parent window
        NULL,          // No menu
        hInstance,
        NULL           // No creation data
    );
    if (!hwnd) {
        // Show error if window creation fails
        MessageBoxW(NULL, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Display and update the window
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Initialize the Matrix animation with window handle and screen size
    matrix = Matrix_init(hwnd, screenWidth, screenHeight);
    if (!matrix) {
        // Show error and clean up if initialization fails
        MessageBoxW(NULL, L"Failed to initialize Matrix", L"Error", MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd);
        return 1;
    }

    // Get initial mouse position for movement detection
    GetCursorPos(&initialMousePos);

    // Main message loop to keep the program running
    MSG msg;
    while (running) {
        // Process all pending messages
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            // If WM_QUIT is received, exit the loop
            if (msg.message == WM_QUIT) {
                running = FALSE;
            }
            // Translate and dispatch messages
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        // Update the Matrix animation state
        Matrix_update(matrix);
        // Request window redraw
        InvalidateRect(hwnd, NULL, FALSE);
        // Pause to control animation speed (50ms)
        Sleep(50);
    }

    // Clean up resources
    Matrix_deinit(matrix);
    DestroyWindow(hwnd);
    return 0;
}