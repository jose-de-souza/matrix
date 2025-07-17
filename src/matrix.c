#include "matrix.h"
#include "charset.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Define constants for animation behavior
#define FONT_HEIGHT 20          // Pixel height of each character in the Consolas font
#define MATRIX_SPEED_MS 70      // Update interval in milliseconds (controls animation speed)
#define FADE_RATE 0.04f         // Rate at which trails fade (alpha decrement per frame)
#define MIN_TRAIL_LENGTH 30     // Minimum number of characters in a trail
#define MAX_TRAIL_LENGTH 40     // Maximum number of characters in a trail
#define ACTIVE_COLUMN_PROBABILITY 0.95f // Probability a column is active (95% chance)

// Linear Congruential Generator (LCG) for pseudo-random numbers
static unsigned int rng_next(unsigned int* state) {
    // Update state using LCG formula: state = (state * multiplier + increment) mod 2^31
    *state = (*state * 1664525 + 1013904223) & 0x7FFFFFFF;
    return *state;
}

// Generate random integer in range [at_least, less_than)
static int rng_int_range(unsigned int* state, int at_least, int less_than) {
    // Calculate range of possible values
    int range = less_than - at_least;
    if (range <= 0) return at_least; // Return minimum if range is invalid
    // Get random number and map to range
    unsigned int r = rng_next(state);
    return at_least + (r % range);
}

// Generate random float in range [0, 1)
static float rng_float(unsigned int* state) {
    // Divide random number by 10000 to get value between 0 and 1
    return (float)(rng_next(state) % 10000) / 10000.0f;
}

// Select a random character from the letters array in charset.h
static WCHAR random_char(unsigned int* rng_state) {
    // Calculate number of characters in letters array (excluding null terminator)
    int len = (int)(sizeof(letters) / sizeof(letters[0]) - 1);
    if (len <= 0) {
        // Return '0' if array is empty (error case)
        return L'0';
    }
    // Pick random index and return corresponding Unicode character
    int idx = rng_int_range(rng_state, 0, len);
    return letters[idx];
}

// Initialize the Matrix animation structure
Matrix* Matrix_init(HWND hwnd, int width, int height) {
    // Allocate memory for Matrix structure
    Matrix* matrix = (Matrix*)malloc(sizeof(Matrix));
    if (!matrix) {
        // Return NULL if memory allocation fails
        return NULL;
    }

    // Get device context (DC) for the window to measure font metrics
    HDC hdc = GetDC(hwnd);
    if (!hdc) {
        // Free matrix and return NULL if DC acquisition fails
        free(matrix);
        return NULL;
    }

    // Create a bold Consolas font with specified height
    HFONT font = CreateFontW(
        FONT_HEIGHT, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_MODERN, L"Consolas"
    );
    if (!font) {
        // Clean up and return NULL if font creation fails
        free(matrix);
        ReleaseDC(hwnd, hdc);
        return NULL;
    }

    // Select font into DC to get text metrics
    HFONT old_font = (HFONT)SelectObject(hdc, font);
    TEXTMETRICW tm;
    if (!GetTextMetricsW(hdc, &tm)) {
        // Clean up and return NULL if text metrics retrieval fails
        SelectObject(hdc, old_font);
        DeleteObject(font);
        free(matrix);
        ReleaseDC(hwnd, hdc);
        return NULL;
    }
    // Calculate character width (average width + padding)
    int char_width = tm.tmAveCharWidth + 8;
    // Calculate number of columns based on window width
    size_t columns = char_width > 0 ? (size_t)(width / char_width) : 0;
    // Cap columns at 500 to prevent performance issues
    if (columns > 500) columns = 500;
    // Restore original font and release DC
    SelectObject(hdc, old_font);
    ReleaseDC(hwnd, hdc);

    // Check if columns are valid
    if (columns == 0) {
        // Clean up if char_width is invalid
        DeleteObject(font);
        free(matrix);
        return NULL;
    }

    // Initialize Matrix structure fields
    matrix->hwnd = hwnd;                    // Store window handle
    matrix->hfont = font;                   // Store font handle
    matrix->width = width;                  // Store window width
    matrix->height = height;                // Store window height
    matrix->char_width = char_width;        // Store character width
    matrix->columns = columns;              // Store number of columns
    // Seed RNG with system tick count XORed with a constant
    matrix->rng_state = (unsigned int)(GetTickCount() ^ 0xDEADBEEF);
    // Store current time for update timing
    matrix->last_update = GetTickCount64();

    // Allocate arrays for trail data
    matrix->drops = (int*)calloc(columns, sizeof(int));                     // Y positions
    matrix->frozen = (BOOL*)calloc(columns, sizeof(BOOL));                  // Frozen state
    matrix->trail_chars = (WCHAR**)calloc(columns, sizeof(WCHAR*));         // Character arrays
    matrix->trail_lengths = (size_t*)calloc(columns, sizeof(size_t));       // Trail lengths
    matrix->trail_alphas = (float*)calloc(columns, sizeof(float));          // Alpha values
    matrix->frozen_trails = (FrozenTrail**)calloc(columns, sizeof(FrozenTrail*)); // Frozen trails
    matrix->frozen_trail_counts = (size_t*)calloc(columns, sizeof(size_t)); // Frozen trail counts
    matrix->frozen_trail_capacities = (size_t*)calloc(columns, sizeof(size_t)); // Capacities

    // Check if any allocation failed
    if (!matrix->drops || !matrix->frozen || !matrix->trail_chars || 
        !matrix->trail_lengths || !matrix->trail_alphas || !matrix->frozen_trails || 
        !matrix->frozen_trail_counts || !matrix->frozen_trail_capacities) {
        // Clean up and return NULL on failure
        Matrix_deinit(matrix);
        return NULL;
    }

    // Initialize each column
    for (size_t i = 0; i < columns; i++) {
        // Allocate character array for the trail
        matrix->trail_chars[i] = (WCHAR*)calloc(MAX_TRAIL_LENGTH, sizeof(WCHAR));
        if (!matrix->trail_chars[i]) {
            // Clean up and return NULL if allocation fails
            Matrix_deinit(matrix);
            return NULL;
        }
        // Set random trail length
        matrix->trail_lengths[i] = rng_int_range(&matrix->rng_state, MIN_TRAIL_LENGTH, MAX_TRAIL_LENGTH + 1);
        // Set initial alpha to fully opaque
        matrix->trail_alphas[i] = 1.0f;
        // Fill trail with random characters
        for (size_t j = 0; j < matrix->trail_lengths[i]; j++) {
            matrix->trail_chars[i][j] = random_char(&matrix->rng_state);
        }
        // Initialize frozen trail array
        matrix->frozen_trails[i] = NULL;
        matrix->frozen_trail_counts[i] = 0;
        matrix->frozen_trail_capacities[i] = 0;
        // Randomly decide if column is active
        if (rng_float(&matrix->rng_state) < ACTIVE_COLUMN_PROBABILITY) {
            // Start trail above screen for smooth entry
            matrix->drops[i] = -rng_int_range(&matrix->rng_state, 0, height / FONT_HEIGHT * 2);
        } else {
            // Mark as inactive with large negative position
            matrix->drops[i] = -10000;
        }
    }

    return matrix;
}

// Clean up Matrix structure and free all allocated memory
void Matrix_deinit(Matrix* matrix) {
    if (!matrix) return;
    // Free per-column arrays
    for (size_t i = 0; i < matrix->columns; i++) {
        if (matrix->trail_chars[i]) free(matrix->trail_chars[i]);
        if (matrix->frozen_trails[i]) free(matrix->frozen_trails[i]);
    }
    // Free top-level arrays
    if (matrix->drops) free(matrix->drops);
    if (matrix->frozen) free(matrix->frozen);
    if (matrix->trail_chars) free(matrix->trail_chars);
    if (matrix->trail_lengths) free(matrix->trail_lengths);
    if (matrix->trail_alphas) free(matrix->trail_alphas);
    if (matrix->frozen_trails) free(matrix->frozen_trails);
    if (matrix->frozen_trail_counts) free(matrix->frozen_trail_counts);
    if (matrix->frozen_trail_capacities) free(matrix->frozen_trail_capacities);
    // Delete font object
    if (matrix->hfont) DeleteObject(matrix->hfont);
    // Free Matrix structure
    free(matrix);
}

// Update the state of all trails in the Matrix
void Matrix_update(Matrix* matrix) {
    // Get current time in milliseconds
    long long now = GetTickCount64();
    // Skip update if not enough time has passed
    if (now - matrix->last_update < MATRIX_SPEED_MS) return;
    matrix->last_update = now;

    // Calculate screen height in character units
    int screen_height_in_chars = matrix->height / FONT_HEIGHT;

    // Process each column
    for (size_t i = 0; i < matrix->columns; i++) {
        // Handle inactive columns (large negative position)
        if (matrix->drops[i] <= -10000) {
            // Small chance (1%) to reactivate
            if (rng_float(&matrix->rng_state) < 0.01f) {
                matrix->drops[i] = -rng_int_range(&matrix->rng_state, 0, screen_height_in_chars);
                matrix->trail_lengths[i] = rng_int_range(&matrix->rng_state, MIN_TRAIL_LENGTH, MAX_TRAIL_LENGTH + 1);
                matrix->trail_alphas[i] = 1.0f;
            }
            continue;
        }

        // Handle frozen columns
        if (matrix->frozen[i]) {
            // Unfreeze if no frozen trails remain
            if (matrix->frozen_trail_counts[i] == 0) {
                matrix->frozen[i] = FALSE;
                matrix->drops[i] = -rng_int_range(&matrix->rng_state, 0, screen_height_in_chars);
                matrix->trail_lengths[i] = rng_int_range(&matrix->rng_state, MIN_TRAIL_LENGTH, MAX_TRAIL_LENGTH + 1);
                matrix->trail_alphas[i] = 1.0f;
            }
        } else {
            // Randomly start fading active trails (1% chance)
            if (matrix->drops[i] > 0 && matrix->trail_alphas[i] >= 1.0f && rng_float(&matrix->rng_state) < 0.01f) {
                matrix->trail_alphas[i] -= FADE_RATE;
            }

            // Continue fading active trails
            if (matrix->trail_alphas[i] < 1.0f) {
                matrix->trail_alphas[i] -= FADE_RATE;
                // Reset trail if fully faded
                if (matrix->trail_alphas[i] <= 0.0f) {
                    matrix->drops[i] = -rng_int_range(&matrix->rng_state, 0, screen_height_in_chars);
                    matrix->trail_lengths[i] = rng_int_range(&matrix->rng_state, MIN_TRAIL_LENGTH, MAX_TRAIL_LENGTH + 1);
                    matrix->trail_alphas[i] = 1.0f;
                }
            }

            // Update trail if not fully faded
            if (matrix->trail_alphas[i] > 0.0f) {
                // Shift characters down
                for (size_t j = matrix->trail_lengths[i] - 1; j > 0; j--) {
                    matrix->trail_chars[i][j] = matrix->trail_chars[i][j - 1];
                }
                // Add new random character at top
                matrix->trail_chars[i][0] = random_char(&matrix->rng_state);
                // Move trail down
                matrix->drops[i]++;
            }

            // Randomly freeze trails (10% chance when off-screen)
            if (matrix->drops[i] > screen_height_in_chars && matrix->trail_alphas[i] >= 1.0f && rng_float(&matrix->rng_state) > 0.9f) {
                matrix->frozen[i] = TRUE;
                // Resize frozen trail array if needed
                if (matrix->frozen_trail_counts[i] >= matrix->frozen_trail_capacities[i]) {
                    size_t new_capacity = matrix->frozen_trail_capacities[i] == 0 ? 4 : matrix->frozen_trail_capacities[i] * 2;
                    FrozenTrail* new_trails = (FrozenTrail*)realloc(matrix->frozen_trails[i], new_capacity * sizeof(FrozenTrail));
                    if (!new_trails) {
                        continue;
                    }
                    matrix->frozen_trails[i] = new_trails;
                    matrix->frozen_trail_capacities[i] = new_capacity;
                }
                // Store current trail as frozen
                FrozenTrail* trail = &matrix->frozen_trails[i][matrix->frozen_trail_counts[i]++];
                memcpy(trail->characters, matrix->trail_chars[i], matrix->trail_lengths[i] * sizeof(WCHAR));
                trail->y_pos = matrix->drops[i] * FONT_HEIGHT;
                trail->alpha = 1.0f;
                // Reset active trail
                matrix->drops[i] = -rng_int_range(&matrix->rng_state, 0, screen_height_in_chars);
                matrix->trail_alphas[i] = 1.0f;
            }
        }

        // Randomly change a character in the trail (10% chance)
        if (rng_float(&matrix->rng_state) < 0.1f) {
            size_t idx = rng_int_range(&matrix->rng_state, 1, matrix->trail_lengths[i]);
            matrix->trail_chars[i][idx] = random_char(&matrix->rng_state);
        }

        // Fade out frozen trails
        for (size_t j = 0; j < matrix->frozen_trail_counts[i];) {
            matrix->frozen_trails[i][j].alpha -= FADE_RATE;
            // Remove fully faded frozen trails
            if (matrix->frozen_trails[i][j].alpha <= 0) {
                matrix->frozen_trails[i][j] = matrix->frozen_trails[i][--matrix->frozen_trail_counts[i]];
            } else {
                j++;
            }
        }
    }
}

// Render the Matrix animation to the screen
void Matrix_render(Matrix* matrix, HDC hdc) {
    // Create a memory DC for double buffering to avoid flicker
    HDC mem_dc = CreateCompatibleDC(hdc);
    if (!mem_dc) {
        return;
    }
    // Create a compatible bitmap for the memory DC
    HBITMAP mem_bitmap = CreateCompatibleBitmap(hdc, matrix->width, matrix->height);
    if (!mem_bitmap) {
        DeleteDC(mem_dc);
        return;
    }
    // Select bitmap into memory DC
    HBITMAP old_bitmap = (HBITMAP)SelectObject(mem_dc, mem_bitmap);

    // Select font and set transparent background
    SelectObject(mem_dc, matrix->hfont);
    SetBkMode(mem_dc, TRANSPARENT);

    // Clear background to black
    HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
    RECT rect = { 0, 0, matrix->width, matrix->height };
    FillRect(mem_dc, &rect, brush);
    DeleteObject(brush);

    // Render frozen trails
    for (size_t i = 0; i < matrix->columns; i++) {
        if (matrix->drops[i] <= -10000) continue;
        int x = (int)(i * matrix->char_width);
        for (size_t j = 0; j < matrix->frozen_trail_counts[i]; j++) {
            FrozenTrail* trail = &matrix->frozen_trails[i][j];
            for (size_t k = 0; k < matrix->trail_lengths[i]; k++) {
                // Calculate y position for each character
                int y = trail->y_pos - (int)(k + 1) * FONT_HEIGHT;
                // Skip if off-screen
                if (y < 0 || y > matrix->height) continue;
                // Calculate brightness with gradient effect
                int gradient_brightness = 255 - (k * (255 / matrix->trail_lengths[i]));
                if (gradient_brightness < 50) gradient_brightness = 50;
                // Apply alpha for fading
                int final_brightness = (int)(gradient_brightness * trail->alpha);
                if (final_brightness < 5) continue;
                // Set green text color
                SetTextColor(mem_dc, RGB(0, final_brightness, 0));
                // Draw single character
                WCHAR ch_buf[2] = { trail->characters[k], 0 };
                TextOutW(mem_dc, x, y, ch_buf, 1);
            }
        }
    }

    // Render active trails
    for (size_t i = 0; i < matrix->columns; i++) {
        if (matrix->drops[i] <= -10000 || matrix->frozen[i]) continue;
        int x = (int)(i * matrix->char_width);
        int y = matrix->drops[i] * FONT_HEIGHT;
        for (size_t j = 0; j < matrix->trail_lengths[i]; j++) {
            // Calculate y position for each character
            int ty = y - (int)(j + 1) * FONT_HEIGHT;
            if (ty < 0 || ty > matrix->height) continue;
            // Calculate gradient brightness
            int brightness = 255 - (j * (255 / matrix->trail_lengths[i]));
            if (brightness < 50) brightness = 50;
            // Apply alpha for fading
            int final_brightness = (int)(brightness * matrix->trail_alphas[i]);
            if (final_brightness < 5) continue;
            // Set green text color
            SetTextColor(mem_dc, RGB(0, (BYTE)final_brightness, 0));
            // Draw character
            WCHAR ch_buf[2] = { matrix->trail_chars[i][j], 0 };
            TextOutW(mem_dc, x, ty, ch_buf, 1);
        }
        // Draw leading character in bright white
        int lead_brightness = (int)(255 * matrix->trail_alphas[i]);
        if (lead_brightness >= 5) {
            // Set white-ish color for leading character
            SetTextColor(mem_dc, RGB(200 * matrix->trail_alphas[i], 244 * matrix->trail_alphas[i], 248 * matrix->trail_alphas[i]));
            WCHAR lead[2] = { matrix->trail_chars[i][0], 0 };
            TextOutW(mem_dc, x, y, lead, 1);
        }
    }

    // Copy memory DC to screen DC
    BitBlt(hdc, 0, 0, matrix->width, matrix->height, mem_dc, 0, 0, SRCCOPY);

    // Clean up resources
    SelectObject(mem_dc, old_bitmap);
    DeleteObject(mem_bitmap);
    DeleteDC(mem_dc);
}