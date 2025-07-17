#ifndef MATRIX_H
#define MATRIX_H

// Include the Windows API header for types and functions used in this project.
// windows.h provides definitions for HWND, HFONT, BOOL, and other Windows-specific
// types, as well as functions like CreateFontW and TextOutW. Including it here
// ensures all necessary declarations are available for the Matrix structure and
// function prototypes.
#include <windows.h>

// Defines a structure to store data for a trail that has stopped moving (frozen).
// Frozen trails are used to create a lingering effect where trails pause at the
// bottom of the screen and fade out. This structure encapsulates the state needed
// to render these trails independently of active, moving trails.
typedef struct {
    // Array to store up to 64 Unicode characters for the frozen trail.
    // The size 64 is chosen to accommodate the maximum trail length (defined as
    // MAX_TRAIL_LENGTH=40 in matrix.c) with extra space for safety. WCHAR is used
    // for Unicode support, allowing characters like Katakana and Cyrillic.
    WCHAR characters[64];
    
    // Y-coordinate (in pixels) of the trail's base position on the screen.
    // Stored as an int to match Windows API coordinate systems. This position
    // determines where the trail is drawn during rendering.
    int y_pos;
    
    // Transparency value (0.0 to 1.0) for fading the trail during rendering.
    // A float is used for smooth fading, as it allows fine-grained control over
    // opacity decrements (e.g., FADE_RATE=0.04 in matrix.c). This value is
    // multiplied with color brightness to create the fade-out effect.
    float alpha;
} FrozenTrail;

// Defines the main structure to hold all data for the Matrix animation.
// This structure centralizes the state of the screensaver, including window
// properties, font, and trail data. Using a single structure simplifies memory
// management and function interfaces, as all data can be passed via a single
// pointer.
typedef struct {
    // Handle to the window where the animation is drawn.
    // HWND is a Windows API opaque pointer type, used to reference the window
    // created in main.c. It's needed for operations like getting the device
    // context (GetDC) for font metrics and rendering.
    HWND hwnd;
    
    // Handle to the font used for drawing characters.
    // HFONT is a Windows API handle for GDI font objects, created in Matrix_init
    // using CreateFontW. It's selected into the device context during rendering
    // to draw text in the Consolas font.
    HFONT hfont;
    
    // Window width and height in pixels, obtained from GetSystemMetrics in main.c.
    // These dimensions determine the animation's canvas size and are used to
    // calculate the number of columns and screen boundaries.
    int width;
    int height;
    
    // Width of a single character in pixels, calculated from font metrics.
    // This value, derived from TEXTMETRICW.tmAveCharWidth plus padding, determines
    // the spacing of columns in the animation grid.
    int char_width;
    
    // Number of columns in the animation, calculated as width / char_width.
    // Stored as size_t for unsigned, large values, as the number of columns
    // depends on screen resolution and can be significant (capped at 500).
    size_t columns;
    
    // Array of y-positions for active trails, one per column.
    // Each int represents the bottom y-coordinate (in character units) of a trail.
    // Negative values indicate trails starting above the screen or inactive
    // columns (drops[i] <= -10000). Dynamically allocated in Matrix_init.
    int* drops;
    
    // Array indicating whether each column's trail is frozen (TRUE) or active (FALSE).
    // BOOL is a Windows API type (int, typically 0 or 1). This tracks whether a
    // column is in a fading, stationary state or moving downward.
    BOOL* frozen;
    
    // Array of pointers to character arrays for each column's trail.
    // Each WCHAR* points to a dynamically allocated array of Unicode characters,
    // allowing variable-length trails (30-40 characters). Double pointer (WCHAR**)
    // enables per-column character storage.
    WCHAR** trail_chars;
    
    // Array of trail lengths (number of characters per trail).
    // Stored as size_t for unsigned counts. Each trail's length is randomly set
    // between MIN_TRAIL_LENGTH and MAX_TRAIL_LENGTH in Matrix_init.
    size_t* trail_lengths;
    
    // Array of transparency values for active trails.
    // Each float (0.0 to 1.0) controls the opacity of a trail during rendering,
    // enabling smooth fading effects when trails reach the screen bottom.
    float* trail_alphas;
    
    // Array of pointers to FrozenTrail arrays for each column.
    // Each FrozenTrail* points to a dynamically resized array of FrozenTrail
    // structures, storing trails that have frozen for fading. Double pointer
    // (FrozenTrail**) allows per-column management of multiple frozen trails.
    FrozenTrail** frozen_trails;
    
    // Array tracking the number of frozen trails per column.
    // Stored as size_t for unsigned counts. Used to iterate over frozen_trails[i]
    // during rendering and updating.
    size_t* frozen_trail_counts;
    
    // Array of capacities for frozen trail arrays.
    // Tracks the allocated size of each frozen_trails[i] array, enabling dynamic
    // resizing with realloc when new frozen trails are added.
    size_t* frozen_trail_capacities;
    
    // State for the Linear Congruential Generator (LCG) random number generator.
    // Stored as unsigned int for 31-bit arithmetic in rng_next. Seeded with
    // GetTickCount XORed with a constant for randomness across runs.
    unsigned int rng_state;
    
    // Timestamp (in milliseconds) of the last animation update.
    // Stored as long long to handle high-resolution timing from GetTickCount64.
    // Used to enforce a 70ms update interval (MATRIX_SPEED_MS).
    long long last_update;
} Matrix;

// Function prototypes for Matrix animation operations.
// Declaring these in the header allows main.c to call them without needing
// their implementations, promoting modularity. Each function takes a Matrix*
// pointer to operate on the shared animation state.

// Initializes the Matrix structure with the given window handle and dimensions.
// Allocates memory for all arrays and sets up font and column data. Returns
// NULL on failure (e.g., memory allocation or font creation errors), allowing
// error handling in main.c.
Matrix* Matrix_init(HWND hwnd, int width, int height);

// Frees all memory and resources associated with the Matrix structure.
// Ensures no memory leaks by deallocating arrays and GDI objects (e.g., hfont).
// Safe to call with NULL input, following defensive programming practices.
void Matrix_deinit(Matrix* matrix);

// Updates the animation state for all trails.
// Advances trail positions, handles fading and freezing, and randomly changes
// characters. Uses timing checks to maintain a consistent update rate (70ms).
void Matrix_update(Matrix* matrix);

// Renders the animation to the provided device context (hdc).
// Draws active and frozen trails with gradient and fading effects using GDI
// functions. Employs double buffering to prevent flicker, ensuring smooth visuals.
void Matrix_render(Matrix* matrix, HDC hdc);

// Include guard prevents multiple inclusions, avoiding redefinition errors.
// This is critical in C to ensure the compiler processes the header only once
// per translation unit, maintaining clean compilation.
#endif