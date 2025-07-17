#include "matrix.h"
#include "charset.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Define constants for animation behavior, controlling visual and performance
// aspects of the Matrix effect. These are preprocessor macros for compile-time
// evaluation, ensuring consistent behavior and easy tuning.
#define FONT_HEIGHT 20          // Pixel height of each character in the Consolas font
#define MATRIX_SPEED_MS 70      // Update interval in milliseconds (controls animation speed)
#define FADE_RATE 0.04f         // Rate at which trails fade (alpha decrement per frame)
#define MIN_TRAIL_LENGTH 30     // Minimum number of characters in a trail
#define MAX_TRAIL_LENGTH 40     // Maximum number of characters in a trail
#define ACTIVE_COLUMN_PROBABILITY 0.95f // Probability a column is active (95% chance)

// Linear Congruential Generator (LCG) for pseudo-random numbers.
// Generates a sequence of 31-bit integers using a deterministic formula.
// Static limits scope to this file, preventing external access and reducing
// namespace pollution, a best practice for modular C code.
static unsigned int rng_next(unsigned int* state) {
    // Update state using LCG formula: state = (state * multiplier + increment) mod 2^31.
    // Constants 1664525 (multiplier) and 1013904223 (increment) are chosen for
    // good randomness properties. The & 0x7FFFFFFF ensures a 31-bit modulus,
    // avoiding sign issues and ensuring portability across platforms.
    *state = (*state * 1664525 + 1013904223) & 0x7FFFFFFF;
    return *state;
}

// Generate random integer in range [at_least, less_than).
// Provides a utility to map LCG output to a specific range, used for trail
// lengths, positions, and character selection.
static int rng_int_range(unsigned int* state, int at_least, int less_than) {
    // Calculate range of possible values.
    int range = less_than - at_least;
    // Return minimum if range is invalid (e.g., less_than <= at_least) to prevent
    // division by zero or negative modulo, ensuring robust error handling.
    if (range <= 0) return at_least;
    // Get random number and map to range using modulo. This ensures the result
    // falls within [at_least, less_than).
    unsigned int r = rng_next(state);
    return at_least + (r % range);
}

// Generate random float in range [0, 1).
// Used for probabilistic decisions (e.g., starting a trail, fading, or freezing).
static float rng_float(unsigned int* state) {
    // Divide random number by 10000 to get a value between 0 and 1.
    // The modulo 10000 limits precision but is sufficient for animation purposes,
    // balancing randomness quality with performance.
    return (float)(rng_next(state) % 10000) / 10000.0f;
}

// Select a random character from the letters array in charset.h.
// Returns a Unicode (WCHAR) character for use in trails, ensuring visual variety.
static WCHAR random_char(unsigned int* rng_state) {
    // Calculate number of characters in letters array (excluding null terminator).
    // sizeof(letters) / sizeof(letters[0]) computes the array length at compile time,
    // subtracting 1 to exclude the trailing 0x0000.
    int len = (int)(sizeof(letters) / sizeof(letters[0]) - 1);
    // Return '0' if array is empty (error case) to prevent invalid access.
    // This defensive check ensures robustness if charset.h is misconfigured.
    if (len <= 0) {
        return L'0';
    }
    // Pick random index and return corresponding Unicode character.
    int idx = rng_int_range(rng_state, 0, len);
    return letters[idx];
}

// Initialize the Matrix animation structure with window handle and dimensions.
// Allocates memory, sets up font, and initializes trail data for all columns.
// Returns NULL on failure, allowing main.c to handle errors gracefully.
Matrix* Matrix_init(HWND hwnd, int width, int height) {
    // Allocate memory for Matrix structure using malloc.
    // sizeof(Matrix) computes the structure size, including all fields. Checking
    // for NULL ensures robust error handling for memory allocation failures.
    Matrix* matrix = (Matrix*)malloc(sizeof(Matrix));
    if (!matrix) {
        return NULL;
    }

    // Get device context (DC) for the window to measure font metrics.
    // GetDC retrieves a DC for querying display properties. It's released after
    // use to prevent resource leaks, a critical GDI practice.
    HDC hdc = GetDC(hwnd);
    if (!hdc) {
        free(matrix);
        return NULL;
    }

    // Create a bold Consolas font with specified height (FONT_HEIGHT=20).
    // CreateFontW is used for Unicode support, specifying parameters like height,
    // weight (FW_BOLD), and quality (ANTIALIASED_QUALITY) for smooth rendering.
    // Consolas is a monospaced font, ideal for aligned character columns.
    HFONT font = CreateFontW(
        FONT_HEIGHT, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_MODERN, L"Consolas"
    );
    if (!font) {
        free(matrix);
        ReleaseDC(hwnd, hdc);
        return NULL;
    }

    // Select font into DC to get text metrics, saving the old font.
    // SelectObject associates the font with the DC, and TEXTMETRICW provides
    // font properties like average character width. Saving old_font allows
    // restoring the DC's state, preventing resource leaks.
    HFONT old_font = (HFONT)SelectObject(hdc, font);
    TEXTMETRICW tm;
    if (!GetTextMetricsW(hdc, &tm)) {
        SelectObject(hdc, old_font);
        DeleteObject(font);
        free(matrix);
        ReleaseDC(hwnd, hdc);
        return NULL;
    }
    // Calculate character width by adding padding (8 pixels) to tmAveCharWidth.
    // Padding ensures characters don't overlap, improving readability.
    int char_width = tm.tmAveCharWidth + 8;
    // Calculate number of columns based on window width.
    // Cast to size_t for unsigned storage, handling zero char_width safely.
    size_t columns = char_width > 0 ? (size_t)(width / char_width) : 0;
    // Cap columns at 500 to prevent excessive memory and CPU usage on
    // high-resolution displays, balancing performance and visual density.
    if (columns > 500) columns = 500;
    // Restore original font and release DC to free GDI resources.
    SelectObject(hdc, old_font);
    ReleaseDC(hwnd, hdc);

    // Check if columns are valid (non-zero char_width).
    // If invalid, clean up and return NULL to prevent division-by-zero or
    // rendering issues in later steps.
    if (columns == 0) {
        DeleteObject(font);
        free(matrix);
        return NULL;
    }

    // Initialize Matrix structure fields with window and font data.
    matrix->hwnd = hwnd;
    matrix->hfont = font;
    matrix->width = width;
    matrix->height = height;
    matrix->char_width = char_width;
    matrix->columns = columns;
    // Seed RNG with system tick count XORed with 0xDEADBEEF for uniqueness.
    // GetTickCount provides a millisecond timestamp, and XOR ensures different
    // seeds across runs, improving randomness variety.
    matrix->rng_state = (unsigned int)(GetTickCount() ^ 0xDEADBEEF);
    // Store current time for update timing, using GetTickCount64 for
    // high-resolution timing (64-bit to avoid wraparound issues).
    matrix->last_update = GetTickCount64();

    // Allocate arrays for trail data using calloc for zero-initialization.
    // Zero-initialization prevents undefined behavior in uninitialized fields.
    // Each array is sized for the number of columns, storing per-column data.
    matrix->drops = (int*)calloc(columns, sizeof(int));                     // Y positions
    matrix->frozen = (BOOL*)calloc(columns, sizeof(BOOL));                  // Frozen state
    matrix->trail_chars = (WCHAR**)calloc(columns, sizeof(WCHAR*));         // Character arrays
    matrix->trail_lengths = (size_t*)calloc(columns, sizeof(size_t));       // Trail lengths
    matrix->trail_alphas = (float*)calloc(columns, sizeof(float));          // Alpha values
    matrix->frozen_trails = (FrozenTrail**)calloc(columns, sizeof(FrozenTrail*)); // Frozen trails
    matrix->frozen_trail_counts = (size_t*)calloc(columns, sizeof(size_t)); // Frozen trail counts
    matrix->frozen_trail_capacities = (size_t*)calloc(columns, sizeof(size_t)); // Capacities

    // Check if any allocation failed, cleaning up and returning NULL if so.
    // This ensures partial allocations don't cause memory leaks or crashes.
    if (!matrix->drops || !matrix->frozen || !matrix->trail_chars || 
        !matrix->trail_lengths || !matrix->trail_alphas || !matrix->frozen_trails || 
        !matrix->frozen_trail_counts || !matrix->frozen_trail_capacities) {
        Matrix_deinit(matrix);
        return NULL;
    }

    // Initialize each column's trail data.
    for (size_t i = 0; i < columns; i++) {
        // Allocate character array for the trail, sized for MAX_TRAIL_LENGTH.
        // Using calloc ensures characters are initialized to 0, avoiding garbage data.
        matrix->trail_chars[i] = (WCHAR*)calloc(MAX_TRAIL_LENGTH, sizeof(WCHAR));
        if (!matrix->trail_chars[i]) {
            Matrix_deinit(matrix);
            return NULL;
        }
        // Set random trail length between MIN_TRAIL_LENGTH and MAX_TRAIL_LENGTH.
        matrix->trail_lengths[i] = rng_int_range(&matrix->rng_state, MIN_TRAIL_LENGTH, MAX_TRAIL_LENGTH + 1);
        // Set initial alpha to 1.0 (fully opaque) for new trails.
        matrix->trail_alphas[i] = 1.0f;
        // Fill trail with random Unicode characters from charset.h.
        for (size_t j = 0; j < matrix->trail_lengths[i]; j++) {
            matrix->trail_chars[i][j] = random_char(&matrix->rng_state);
        }
        // Initialize frozen trail array as NULL with zero count and capacity.
        // Dynamic allocation occurs later in Matrix_update when trails freeze.
        matrix->frozen_trails[i] = NULL;
        matrix->frozen_trail_counts[i] = 0;
        matrix->frozen_trail_capacities[i] = 0;
        // Randomly decide if column is active (95% chance, ACTIVE_COLUMN_PROBABILITY).
        // Active columns start above the screen for smooth entry, with negative
        // y-positions. Inactive columns use a large negative value (-10000) to
        // indicate they are off.
        if (rng_float(&matrix->rng_state) < ACTIVE_COLUMN_PROBABILITY) {
            matrix->drops[i] = -rng_int_range(&matrix->rng_state, 0, height / FONT_HEIGHT * 2);
        } else {
            matrix->drops[i] = -10000;
        }
    }

    return matrix;
}

// Clean up Matrix structure and free all allocated memory and resources.
// Ensures no memory leaks or dangling GDI objects, following best practices
// for resource management in C and Windows programming.
void Matrix_deinit(Matrix* matrix) {
    // Check for NULL to prevent crashes if called with uninitialized matrix.
    if (!matrix) return;
    // Free per-column arrays (trail_chars and frozen_trails).
    for (size_t i = 0; i < matrix->columns; i++) {
        if (matrix->trail_chars[i]) free(matrix->trail_chars[i]);
        if (matrix->frozen_trails[i]) free(matrix->frozen_trails[i]);
    }
    // Free top-level arrays, checking for NULL to ensure safety.
    if (matrix->drops) free(matrix->drops);
    if (matrix->frozen) free(matrix->frozen);
    if (matrix->trail_chars) free(matrix->trail_chars);
    if (matrix->trail_lengths) free(matrix->trail_lengths);
    if (matrix->trail_alphas) free(matrix->trail_alphas);
    if (matrix->frozen_trails) free(matrix->frozen_trails);
    if (matrix->frozen_trail_counts) free(matrix->frozen_trail_counts);
    if (matrix->frozen_trail_capacities) free(matrix->frozen_trail_capacities);
    // Delete font object to release GDI resources.
    // DeleteObject is required for all GDI objects created with CreateFontW.
    if (matrix->hfont) DeleteObject(matrix->hfont);
    // Free the Matrix structure itself.
    free(matrix);
}

// Update the state of all trails in the Matrix animation.
// Manages trail movement, fading, freezing, and character changes, ensuring
// the digital rain effect is dynamic and visually appealing.
void Matrix_update(Matrix* matrix) {
    // Get current time in milliseconds using GetTickCount64 for high resolution.
    // This 64-bit function avoids wraparound issues present in 32-bit GetTickCount.
    long long now = GetTickCount64();
    // Skip update if less than MATRIX_SPEED_MS (70ms) has passed since last update.
    // This enforces a consistent animation speed, preventing excessive CPU usage.
    if (now - matrix->last_update < MATRIX_SPEED_MS) return;
    matrix->last_update = now;

    // Calculate screen height in character units (pixels / FONT_HEIGHT).
    // This converts pixel coordinates to character grid coordinates for trail
    // positioning and boundary checks.
    int screen_height_in_chars = matrix->height / FONT_HEIGHT;

    // Process each column independently to update its trail state.
    for (size_t i = 0; i < matrix->columns; i++) {
        // Handle inactive columns (drops[i] <= -10000).
        // These are trails that are off-screen or not yet started.
        if (matrix->drops[i] <= -10000) {
            // 1% chance to reactivate the column, adding variety to the animation.
            if (rng_float(&matrix->rng_state) < 0.01f) {
                // Start trail above screen for smooth entry.
                matrix->drops[i] = -rng_int_range(&matrix->rng_state, 0, screen_height_in_chars);
                matrix->trail_lengths[i] = rng_int_range(&matrix->rng_state, MIN_TRAIL_LENGTH, MAX_TRAIL_LENGTH + 1);
                matrix->trail_alphas[i] = 1.0f; // Reset to fully opaque.
            }
            continue;
        }

        // Handle frozen columns (trails that have stopped moving and are fading).
        if (matrix->frozen[i]) {
            // Unfreeze if no frozen trails remain, allowing the column to resume
            // with a new active trail.
            if (matrix->frozen_trail_counts[i] == 0) {
                matrix->frozen[i] = FALSE;
                matrix->drops[i] = -rng_int_range(&matrix->rng_state, 0, screen_height_in_chars);
                matrix->trail_lengths[i] = rng_int_range(&matrix->rng_state, MIN_TRAIL_LENGTH, MAX_TRAIL_LENGTH + 1);
                matrix->trail_alphas[i] = 1.0f;
            }
        } else {
            // Randomly start fading active trails (1% chance when on-screen).
            // Only start fading when trail is visible (drops[i] > 0) and fully opaque.
            if (matrix->drops[i] > 0 && matrix->trail_alphas[i] >= 1.0f && rng_float(&matrix->rng_state) < 0.01f) {
                matrix->trail_alphas[i] -= FADE_RATE;
            }

            // Continue fading active trails that have started fading.
            if (matrix->trail_alphas[i] < 1.0f) {
                matrix->trail_alphas[i] -= FADE_RATE;
                // Reset trail if fully faded (alpha <= 0), restarting it above screen.
                if (matrix->trail_alphas[i] <= 0.0f) {
                    matrix->drops[i] = -rng_int_range(&matrix->rng_state, 0, screen_height_in_chars);
                    matrix->trail_lengths[i] = rng_int_range(&matrix->rng_state, MIN_TRAIL_LENGTH, MAX_TRAIL_LENGTH + 1);
                    matrix->trail_alphas[i] = 1.0f;
                }
            }

            // Update trail if not fully faded (still visible).
            if (matrix->trail_alphas[i] > 0.0f) {
                // Shift characters down by copying each character to the next position.
                // This simulates the falling effect, with the top character replaced.
                for (size_t j = matrix->trail_lengths[i] - 1; j > 0; j--) {
                    matrix->trail_chars[i][j] = matrix->trail_chars[i][j - 1];
                }
                // Add new random character at the top (index 0).
                matrix->trail_chars[i][0] = random_char(&matrix->rng_state);
                // Move trail down by incrementing y-position.
                matrix->drops[i]++;
            }

            // Randomly freeze trails (10% chance when off-screen).
            // Only freeze fully opaque trails that have reached the bottom
            // (drops[i] > screen_height_in_chars) to create a lingering effect.
            if (matrix->drops[i] > screen_height_in_chars && matrix->trail_alphas[i] >= 1.0f && rng_float(&matrix->rng_state) > 0.9f) {
                matrix->frozen[i] = TRUE;
                // Resize frozen trail array if capacity is reached.
                // Uses doubling strategy (initially 4, then 2x) for amortized O(1) growth.
                if (matrix->frozen_trail_counts[i] >= matrix->frozen_trail_capacities[i]) {
                    size_t new_capacity = matrix->frozen_trail_capacities[i] == 0 ? 4 : matrix->frozen_trail_capacities[i] * 2;
                    FrozenTrail* new_trails = (FrozenTrail*)realloc(matrix->frozen_trails[i], new_capacity * sizeof(FrozenTrail));
                    if (!new_trails) {
                        continue; // Skip if realloc fails, preserving existing state.
                    }
                    matrix->frozen_trails[i] = new_trails;
                    matrix->frozen_trail_capacities[i] = new_capacity;
                }
                // Store current trail as frozen, copying characters and setting position.
                FrozenTrail* trail = &matrix->frozen_trails[i][matrix->frozen_trail_counts[i]++];
                memcpy(trail->characters, matrix->trail_chars[i], matrix->trail_lengths[i] * sizeof(WCHAR));
                trail->y_pos = matrix->drops[i] * FONT_HEIGHT;
                trail->alpha = 1.0f;
                // Reset active trail to start above screen.
                matrix->drops[i] = -rng_int_range(&matrix->rng_state, 0, screen_height_in_chars);
                matrix->trail_alphas[i] = 1.0f;
            }
        }

        // Randomly change a character in the trail (10% chance) to add visual variety.
        // Changes a non-top character (index >= 1) to avoid affecting the leading character.
        if (rng_float(&matrix->rng_state) < 0.1f) {
            size_t idx = rng_int_range(&matrix->rng_state, 1, matrix->trail_lengths[i]);
            matrix->trail_chars[i][idx] = random_char(&matrix->rng_state);
        }

        // Fade out frozen trails and remove fully faded ones.
        // Uses a swap-and-decrement technique to remove elements without shifting.
        for (size_t j = 0; j < matrix->frozen_trail_counts[i];) {
            matrix->frozen_trails[i][j].alpha -= FADE_RATE;
            if (matrix->frozen_trails[i][j].alpha <= 0) {
                // Replace current trail with the last one and decrease count.
                matrix->frozen_trails[i][j] = matrix->frozen_trails[i][--matrix->frozen_trail_counts[i]];
            } else {
                j++; // Move to next trail only if not removed.
            }
        }
    }
}

// Render the Matrix animation to the provided device context (hdc).
// Uses double buffering to prevent flicker and GDI functions for text rendering.
void Matrix_render(Matrix* matrix, HDC hdc) {
    // Create a memory DC for double buffering, compatible with the screen DC.
    // CreateCompatibleDC creates an off-screen DC for smooth rendering.
    HDC mem_dc = CreateCompatibleDC(hdc);
    if (!mem_dc) {
        return; // Exit if DC creation fails to avoid crashes.
    }
    // Create a compatible bitmap for the memory DC, sized to the window.
    HBITMAP mem_bitmap = CreateCompatibleBitmap(hdc, matrix->width, matrix->height);
    if (!mem_bitmap) {
        DeleteDC(mem_dc);
        return;
    }
    // Select bitmap into memory DC, saving the old bitmap for cleanup.
    HBITMAP old_bitmap = (HBITMAP)SelectObject(mem_dc, mem_bitmap);

    // Select font and set transparent background for text rendering.
    // TRANSPARENT mode ensures text background doesn't overwrite the black canvas.
    SelectObject(mem_dc, matrix->hfont);
    SetBkMode(mem_dc, TRANSPARENT);

    // Clear background to black using a solid brush and FillRect.
    // This ensures a clean canvas for each frame, preventing artifacts.
    HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
    RECT rect = { 0, 0, matrix->width, matrix->height };
    FillRect(mem_dc, &rect, brush);
    DeleteObject(brush);

    // Render frozen trails first to ensure correct layering (behind active trails).
    for (size_t i = 0; i < matrix->columns; i++) {
        // Skip inactive columns (drops[i] <= -10000).
        if (matrix->drops[i] <= -10000) continue;
        // Calculate x-position for the column based on char_width.
        int x = (int)(i * matrix->char_width);
        // Render each frozen trail in the column.
        for (size_t j = 0; j < matrix->frozen_trail_counts[i]; j++) {
            FrozenTrail* trail = &matrix->frozen_trails[i][j];
            // Render each character in the frozen trail.
            for (size_t k = 0; k < matrix->trail_lengths[i]; k++) {
                // Calculate y-position, moving upward for each character.
                // Characters are drawn from bottom (y_pos) to top (y_pos - k*FONT_HEIGHT).
                int y = trail->y_pos - (int)(k + 1) * FONT_HEIGHT;
                // Skip if character is off-screen to optimize rendering.
                if (y < 0 || y > matrix->height) continue;
                // Calculate brightness with gradient effect: brighter at top, dimmer at bottom.
                // Minimum brightness (50) ensures visibility even at the trail's end.
                int gradient_brightness = 255 - (k * (255 / matrix->trail_lengths[i]));
                if (gradient_brightness < 50) gradient_brightness = 50;
                // Apply alpha for fading, scaling brightness.
                int final_brightness = (int)(gradient_brightness * trail->alpha);
                if (final_brightness < 5) continue; // Skip nearly invisible characters.
                // Set green text color for Matrix aesthetic.
                SetTextColor(mem_dc, RGB(0, final_brightness, 0));
                // Draw single character using TextOutW for Unicode support.
                // ch_buf is a null-terminated string for one character.
                WCHAR ch_buf[2] = { trail->characters[k], 0 };
                TextOutW(mem_dc, x, y, ch_buf, 1);
            }
        }
    }

    // Render active trails, which appear over frozen trails.
    for (size_t i = 0; i < matrix->columns; i++) {
        // Skip inactive or frozen columns.
        if (matrix->drops[i] <= -10000 || matrix->frozen[i]) continue;
        int x = (int)(i * matrix->char_width);
        int y = matrix->drops[i] * FONT_HEIGHT;
        // Render each character in the active trail.
        for (size_t j = 0; j < matrix->trail_lengths[i]; j++) {
            // Calculate y-position, moving upward from the trail's base.
            int ty = y - (int)(j + 1) * FONT_HEIGHT;
            if (ty < 0 || ty > matrix->height) continue;
            // Apply gradient brightness, similar to frozen trails.
            int brightness = 255 - (j * (255 / matrix->trail_lengths[i]));
            if (brightness < 50) brightness = 50;
            // Apply alpha for fading.
            int final_brightness = (int)(brightness * matrix->trail_alphas[i]);
            if (final_brightness < 5) continue;
            SetTextColor(mem_dc, RGB(0, (BYTE)final_brightness, 0));
            WCHAR ch_buf[2] = { matrix->trail_chars[i][j], 0 };
            TextOutW(mem_dc, x, ty, ch_buf, 1);
        }
        // Draw leading character in bright white for emphasis.
        int lead_brightness = (int)(255 * matrix->trail_alphas[i]);
        if (lead_brightness >= 5) {
            // Use a white-ish color (RGB scaled by alpha) to highlight the trail head.
            SetTextColor(mem_dc, RGB(200 * matrix->trail_alphas[i], 244 * matrix->trail_alphas[i], 248 * matrix->trail_alphas[i]));
            WCHAR lead[2] = { matrix->trail_chars[i][0], 0 };
            TextOutW(mem_dc, x, y, lead, 1);
        }
    }

    // Copy memory DC to screen DC using BitBlt for efficient transfer.
    // SRCCOPY ensures a direct copy of the rendered bitmap to the screen.
    BitBlt(hdc, 0, 0, matrix->width, matrix->height, mem_dc, 0, 0, SRCCOPY);

    // Clean up GDI resources to prevent leaks.
    // Restore original bitmap, delete the new bitmap, and delete the memory DC.
    SelectObject(mem_dc, old_bitmap);
    DeleteObject(mem_bitmap);
    DeleteDC(mem_dc);
}