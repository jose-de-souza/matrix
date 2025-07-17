#include <windows.h>
#include <stdio.h>
#include "matrix.h"
#include "../resource.h"

// Global variables to manage the Matrix screensaver state, accessible across
// functions. Static limits their scope to this file, reducing namespace pollution
// and preventing external modification, a best practice for encapsulation.
static Matrix* matrix = NULL;         // Pointer to the Matrix structure holding animation data
static BOOL running = TRUE;           // Boolean flag to control the main loop; TRUE keeps the program running
static POINT initialMousePos = { -1, -1 }; // Stores initial mouse position; -1 indicates uninitialized
static const int MOUSE_MOVE_THRESHOLD = 10; // Distance in pixels mouse must move to exit screensaver

// Window procedure to handle Windows messages (events) for the screensaver window.
// LRESULT is a Windows API type for return values, typically 0 for success or
// specific values for certain messages. HWND, UINT, WPARAM, and LPARAM are
// Windows types for window handle, message ID, and message parameters,
// respectively. This function is the core of event-driven programming in Windows,
// processing user input and system events.
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Switch statement to process different types of Windows messages, dispatching
    // based on msg. Each case handles a specific event, ensuring the screensaver
    // responds appropriately to user input and system requests.
    switch (msg) {
        // WM_SETCURSOR: Sent when the mouse moves over the window.
        // Used to hide the cursor, a standard feature for screensavers to enhance
        // immersion by removing visual distractions.
        case WM_SETCURSOR:
            // Set the cursor to NULL, effectively hiding it. This is done for every
            // cursor movement to ensure the cursor remains invisible.
            SetCursor(NULL);
            // Return TRUE to indicate the message was handled, preventing default
            // cursor behavior (e.g., showing an arrow).
            return TRUE;
        
        // WM_DESTROY: Sent when the window is being closed (e.g., by the system).
        // Triggers program exit, ensuring clean shutdown.
        case WM_DESTROY:
            // Set running to FALSE to exit the main loop in WinMain.
            running = FALSE;
            // Post a WM_QUIT message to the message queue, signaling the application
            // to terminate. This ensures the message loop exits gracefully.
            PostQuitMessage(0);
            // Return 0 to indicate successful handling, per Windows API convention.
            return 0;
        
        // WM_KEYDOWN: Sent when a key is pressed.
        // Screensavers typically exit on any user input, including keyboard presses.
        case WM_KEYDOWN:
            // Set running to FALSE to stop the main loop.
            running = FALSE;
            // Post WM_QUIT to terminate the message loop, ensuring immediate exit.
            PostQuitMessage(0);
            return 0;
        
        // WM_LBUTTONDOWN: Sent when the left mouse button is clicked.
        // Provides another exit condition, aligning with screensaver standards.
        case WM_LBUTTONDOWN:
            // Set running to FALSE and post WM_QUIT for immediate exit.
            running = FALSE;
            PostQuitMessage(0);
            return 0;
        
        // WM_MOUSEMOVE: Sent when the mouse moves over the window.
        // Exits the screensaver if the mouse moves significantly, a standard
        // screensaver feature to detect user activity.
        case WM_MOUSEMOVE: {
            // Extract x and y coordinates from lParam using LOWORD and HIWORD macros.
            // lParam is a 32-bit value where the low 16 bits are x and high 16 bits
            // are y, a common Windows API convention for mouse coordinates.
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);
            // If initialMousePos is uninitialized (-1, -1), set it to the current
            // position. This captures the starting point for movement detection.
            if (initialMousePos.x == -1 && initialMousePos.y == -1) {
                initialMousePos.x = xPos;
                initialMousePos.y = yPos;
                return 0;
            }
            // Calculate absolute distance moved in x and y directions using abs.
            // This ensures movement in any direction (positive or negative) is detected.
            int dx = abs(xPos - initialMousePos.x);
            int dy = abs(yPos - initialMousePos.y);
            // If movement exceeds MOUSE_MOVE_THRESHOLD (10 pixels), exit the screensaver.
            // The threshold prevents accidental exits from minor mouse jitter.
            if (dx > MOUSE_MOVE_THRESHOLD || dy > MOUSE_MOVE_THRESHOLD) {
                running = FALSE;
                PostQuitMessage(0);
            }
            return 0;
        }
        
        // WM_PAINT: Sent when the window needs to be redrawn (e.g., after resizing
        // or uncovering). Handles rendering the Matrix animation.
        case WM_PAINT: {
            // PAINTSTRUCT holds painting information, such as the region to redraw.
            PAINTSTRUCT ps;
            // BeginPaint retrieves the device context (HDC) for drawing and prepares
            // the window for painting. It must be paired with EndPaint to release
            // resources, preventing GDI leaks.
            HDC hdc = BeginPaint(hwnd, &ps);
            // If matrix is initialized, call Matrix_render to draw the animation.
            // Checking for NULL prevents crashes if initialization failed.
            if (matrix) {
                Matrix_render(matrix, hdc);
            }
            // EndPaint releases the HDC and completes the painting process.
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        // Default case for unhandled messages.
        // Passes unprocessed messages to DefWindowProcW for default handling,
        // ensuring the application behaves correctly for system events (e.g., focus).
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// Main entry point for the Windows application, replacing standard C main.
// WinMain is called by the Windows runtime, passing parameters for the instance
// handle, previous instance (unused), command line, and show state. It sets up
// the window, initializes the animation, and runs the message loop.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Ignore unused parameters to suppress compiler warnings.
    // hPrevInstance is deprecated (always NULL in modern Windows).
    // lpCmdLine and nCmdShow are unused as the screensaver runs full-screen.
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Define the window class name as a wide-character string for Unicode support.
    // Using L prefix ensures wchar_t literals, compatible with _UNICODE macro.
    const wchar_t* className = L"MatrixWndClass";
    
    // Initialize window class structure, zeroed for safety.
    // WNDCLASSW is used for Unicode window registration, aligning with _UNICODE.
    WNDCLASSW wc = { 0 };
    // Set class styles: CS_HREDRAW and CS_VREDRAW ensure the window redraws
    // when resized horizontally or vertically, maintaining visual consistency.
    wc.style = CS_HREDRAW | CS_VREDRAW;
    // Assign the WndProc function to handle messages for this window class.
    wc.lpfnWndProc = WndProc;
    // Set the instance handle, identifying the application instance.
    wc.hInstance = hInstance;
    // Set a black background brush using GetStockObject for efficiency.
    // BLACK_BRUSH avoids creating a custom brush, reducing resource usage.
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    // Set the class name for registration.
    wc.lpszClassName = className;
    // Set cursor to NULL (hidden, handled in WM_SETCURSOR).
    wc.hCursor = NULL;
    // Load the application icon using IDI_ICON1 from resource.h.
    // MAKEINTRESOURCEW converts the integer ID to a resource pointer, and
    // LoadIconW loads the icon for the window's title bar and taskbar.
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1));

    // Register the window class with Windows. This step is required before
    // creating a window. If registration fails, display an error and exit.
    if (!RegisterClassW(&wc)) {
        // MessageBoxW displays a Unicode error message with an OK button and
        // error icon. NULL parent window makes it a standalone dialog.
        MessageBoxW(NULL, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
        return 1; // Non-zero return indicates failure.
    }

    // Get screen dimensions using GetSystemMetrics for full-screen display.
    // SM_CXSCREEN and SM_CYSCREEN provide the primary monitor's resolution,
    // ensuring the screensaver covers the entire screen.
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Create a topmost, borderless window for the screensaver.
    // CreateWindowExW is used for Unicode support and extended styles.
    // WS_EX_TOPMOST keeps the window above others, and WS_POPUP removes borders
    // and title bar, creating a full-screen effect.
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST, // Always on top
        className,
        L"Matrix Screensaver", // Window title (not visible due to WS_POPUP)
        WS_POPUP,      // No borders or title bar
        0,             // Top-left corner (x=0, y=0)
        0,
        screenWidth,
        screenHeight,
        NULL,          // No parent window
        NULL,          // No menu
        hInstance,
        NULL           // No creation data
    );
    if (!hwnd) {
        // Handle window creation failure with an error message and exit.
        MessageBoxW(NULL, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Display and update the window to make it visible and ensure initial rendering.
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Initialize the Matrix animation with the window handle and screen size.
    // Matrix_init allocates memory and sets up the animation state. If it fails,
    // display an error, clean up the window, and exit.
    matrix = Matrix_init(hwnd, screenWidth, screenHeight);
    if (!matrix) {
        MessageBoxW(NULL, L"Failed to initialize Matrix", L"Error", MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd);
        return 1;
    }

    // Get initial mouse position for movement detection.
    // GetCursorPos stores the current screen coordinates in initialMousePos.
    // This is used in WM_MOUSEMOVE to detect significant movement.
    GetCursorPos(&initialMousePos);

    // Main message loop to keep the program running, processing events and
    // updating the animation. Uses PeekMessageW for non-blocking message
    // processing, allowing continuous animation updates.
    MSG msg;
    while (running) {
        // Process all pending messages in the queue.
        // PeekMessageW with PM_REMOVE retrieves and removes messages, enabling
        // the loop to handle input while performing animation updates.
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            // If WM_QUIT is received, exit the loop to terminate the application.
            if (msg.message == WM_QUIT) {
                running = FALSE;
            }
            // TranslateMessage converts virtual-key messages to character messages
            // (e.g., for keyboard input). DispatchMessageW sends messages to WndProc.
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        // Update the Matrix animation state (move trails, handle fading/freezing).
        Matrix_update(matrix);
        // Request window redraw by invalidating the entire window.
        // FALSE for bErase prevents clearing the background, as Matrix_render
        // handles it to avoid flicker.
        InvalidateRect(hwnd, NULL, FALSE);
        // Pause for 50ms to control animation speed and reduce CPU usage.
        // This balances responsiveness with performance, ensuring smooth animation.
        Sleep(50);
    }

    // Clean up resources before exiting.
    // Matrix_deinit frees all allocated memory and GDI objects.
    // DestroyWindow releases the window resources.
    Matrix_deinit(matrix);
    DestroyWindow(hwnd);
    return 0; // Return 0 to indicate successful execution.
}