#include "matrix.h"
#include "charset.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FONT_HEIGHT 20
#define MATRIX_SPEED_MS 100
#define TRAIL_LENGTH 22
#define FADE_RATE 0.04f

// Linear Congruential Generator for random numbers
static unsigned int rng_next(unsigned int* state) {
    *state = *state * 1103515245 + 12345;
    return (*state >> 16) & 0x7FFFFFFF;
}

static int rng_int_range(unsigned int* state, int at_least, int less_than) {
    int range = less_than - at_least;
    if (range <= 0) return at_least;
    unsigned int max = 0x7FFFFFFF;
    unsigned int bucket_size = (max + 1) / (unsigned int)range;
    unsigned int limit = bucket_size * (unsigned int)range;
    unsigned int r;
    do {
        r = rng_next(state);
    } while (r >= limit);
    return at_least + (int)(r / bucket_size);
}

static float rng_float(unsigned int* state) {
    return (float)rng_next(state) / (float)0x7FFFFFFF;
}

static WCHAR random_char(unsigned int* rng_state) {
    return letters[rng_int_range(rng_state, 0, (int)(sizeof(letters) / sizeof(letters[0]) - 1))];
}

Matrix* Matrix_init(HWND hwnd, int width, int height) {
    OutputDebugStringW(L"Matrix_init: Starting initialization\n");

    Matrix* matrix = (Matrix*)malloc(sizeof(Matrix));
    if (!matrix) {
        OutputDebugStringW(L"Matrix_init: Failed to allocate Matrix\n");
        return NULL;
    }

    HDC hdc = GetDC(hwnd);
    if (!hdc) {
        OutputDebugStringW(L"Matrix_init: Failed to get DC\n");
        free(matrix);
        return NULL;
    }

    HFONT font = CreateFontW(
        FONT_HEIGHT, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_MODERN, L"Consolas"
    );
    if (!font) {
        OutputDebugStringW(L"Matrix_init: Failed to create font\n");
        free(matrix);
        ReleaseDC(hwnd, hdc);
        return NULL;
    }

    HFONT old_font = (HFONT)SelectObject(hdc, font);
    TEXTMETRICW tm;
    if (!GetTextMetricsW(hdc, &tm)) {
        OutputDebugStringW(L"Matrix_init: Failed to get text metrics\n");
        SelectObject(hdc, old_font);
        DeleteObject(font);
        free(matrix);
        ReleaseDC(hwnd, hdc);
        return NULL;
    }
    int char_width = tm.tmAveCharWidth;
    size_t columns = char_width > 0 ? (size_t)(width / char_width) : 0;
    SelectObject(hdc, old_font);
    ReleaseDC(hwnd, hdc);

    if (columns == 0) {
        OutputDebugStringW(L"Matrix_init: Invalid columns (char_width=0)\n");
        DeleteObject(font);
        free(matrix);
        return NULL;
    }

    matrix->hwnd = hwnd;
    matrix->hfont = font;
    matrix->width = width;
    matrix->height = height;
    matrix->char_width = char_width;
    matrix->columns = columns;
    matrix->rng_state = (unsigned int)GetTickCount();
    matrix->last_update = GetTickCount64();

    matrix->drops = (int*)calloc(columns, sizeof(int));
    matrix->frozen = (BOOL*)calloc(columns, sizeof(BOOL));
    matrix->trail_chars = (WCHAR**)calloc(columns, sizeof(WCHAR*));
    matrix->frozen_trails = (FrozenTrail**)calloc(columns, sizeof(FrozenTrail*));
    matrix->frozen_trail_counts = (size_t*)calloc(columns, sizeof(size_t));
    matrix->frozen_trail_capacities = (size_t*)calloc(columns, sizeof(size_t));

    if (!matrix->drops || !matrix->frozen || !matrix->trail_chars || 
        !matrix->frozen_trails || !matrix->frozen_trail_counts || 
        !matrix->frozen_trail_capacities) {
        OutputDebugStringW(L"Matrix_init: Memory allocation failed\n");
        Matrix_deinit(matrix);
        return NULL;
    }

    for (size_t i = 0; i < columns; i++) {
        matrix->trail_chars[i] = (WCHAR*)calloc(TRAIL_LENGTH, sizeof(WCHAR));
        if (!matrix->trail_chars[i]) {
            OutputDebugStringW(L"Matrix_init: Failed to allocate trail_chars\n");
            Matrix_deinit(matrix);
            return NULL;
        }
        for (size_t j = 0; j < TRAIL_LENGTH; j++) {
            matrix->trail_chars[i][j] = random_char(&matrix->rng_state);
        }
        matrix->frozen_trails[i] = NULL;
        matrix->frozen_trail_counts[i] = 0;
        matrix->frozen_trail_capacities[i] = 0;
        matrix->drops[i] = -rng_int_range(&matrix->rng_state, 0, height / FONT_HEIGHT);
    }

    OutputDebugStringW(L"Matrix_init: Initialization successful\n");
    return matrix;
}

void Matrix_deinit(Matrix* matrix) {
    if (!matrix) return;
    OutputDebugStringW(L"Matrix_deinit: Cleaning up Matrix\n");
    for (size_t i = 0; i < matrix->columns; i++) {
        if (matrix->trail_chars[i]) free(matrix->trail_chars[i]);
        if (matrix->frozen_trails[i]) free(matrix->frozen_trails[i]);
    }
    if (matrix->drops) free(matrix->drops);
    if (matrix->frozen) free(matrix->frozen);
    if (matrix->trail_chars) free(matrix->trail_chars);
    if (matrix->frozen_trails) free(matrix->frozen_trails);
    if (matrix->frozen_trail_counts) free(matrix->frozen_trail_counts);
    if (matrix->frozen_trail_capacities) free(matrix->frozen_trail_capacities);
    if (matrix->hfont) DeleteObject(matrix->hfont);
    free(matrix);
}

void Matrix_update(Matrix* matrix) {
    long long now = GetTickCount64();
    if (now - matrix->last_update < MATRIX_SPEED_MS) return;
    matrix->last_update = now;

    int screen_height_in_chars = matrix->height / FONT_HEIGHT;

    for (size_t i = 0; i < matrix->columns; i++) {
        if (matrix->frozen[i]) {
            if (matrix->frozen_trail_counts[i] == 0) {
                matrix->frozen[i] = FALSE;
                matrix->drops[i] = 0;
            }
        } else {
            for (size_t j = TRAIL_LENGTH - 1; j > 0; j--) {
                matrix->trail_chars[i][j] = matrix->trail_chars[i][j - 1];
            }
            matrix->trail_chars[i][0] = random_char(&matrix->rng_state);

            matrix->drops[i]++;

            if (matrix->drops[i] > screen_height_in_chars && rng_float(&matrix->rng_state) > 0.975f) {
                matrix->frozen[i] = TRUE;
                if (matrix->frozen_trail_counts[i] >= matrix->frozen_trail_capacities[i]) {
                    size_t new_capacity = matrix->frozen_trail_capacities[i] == 0 ? 4 : matrix->frozen_trail_capacities[i] * 2;
                    FrozenTrail* new_trails = (FrozenTrail*)realloc(matrix->frozen_trails[i], new_capacity * sizeof(FrozenTrail));
                    if (!new_trails) {
                        OutputDebugStringW(L"Matrix_update: Failed to realloc frozen_trails\n");
                        continue;
                    }
                    matrix->frozen_trails[i] = new_trails;
                    matrix->frozen_trail_capacities[i] = new_capacity;
                }
                FrozenTrail* trail = &matrix->frozen_trails[i][matrix->frozen_trail_counts[i]++];
                memcpy(trail->characters, matrix->trail_chars[i], TRAIL_LENGTH * sizeof(WCHAR));
                trail->y_pos = matrix->drops[i] * FONT_HEIGHT;
                trail->alpha = 1.0f;
                matrix->drops[i] = -rng_int_range(&matrix->rng_state, 0, screen_height_in_chars);
            }
        }

        if (rng_float(&matrix->rng_state) < 0.1f) {
            size_t idx = rng_int_range(&matrix->rng_state, 1, TRAIL_LENGTH);
            matrix->trail_chars[i][idx] = random_char(&matrix->rng_state);
        }

        for (size_t j = 0; j < matrix->frozen_trail_counts[i];) {
            matrix->frozen_trails[i][j].alpha -= FADE_RATE;
            if (matrix->frozen_trails[i][j].alpha <= 0) {
                matrix->frozen_trails[i][j] = matrix->frozen_trails[i][--matrix->frozen_trail_counts[i]];
            } else {
                j++;
            }
        }
    }
}

void Matrix_render(Matrix* matrix, HDC hdc) {
    OutputDebugStringW(L"Matrix_render: Starting render\n");

    HDC mem_dc = CreateCompatibleDC(hdc);
    if (!mem_dc) {
        OutputDebugStringW(L"Matrix_render: Failed to create compatible DC\n");
        return;
    }
    HBITMAP mem_bitmap = CreateCompatibleBitmap(hdc, matrix->width, matrix->height);
    if (!mem_bitmap) {
        OutputDebugStringW(L"Matrix_render: Failed to create compatible bitmap\n");
        DeleteDC(mem_dc);
        return;
    }
    HBITMAP old_bitmap = (HBITMAP)SelectObject(mem_dc, mem_bitmap);

    if (!SelectObject(mem_dc, matrix->hfont)) {
        OutputDebugStringW(L"Matrix_render: Failed to select font\n");
    }
    if (!SetBkMode(mem_dc, TRANSPARENT)) {
        OutputDebugStringW(L"Matrix_render: Failed to set background mode\n");
    }

    HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
    RECT rect = { 0, 0, matrix->width, matrix->height };
    FillRect(mem_dc, &rect, brush);
    DeleteObject(brush);

    for (size_t i = 0; i < matrix->columns; i++) {
        int x = (int)i * matrix->char_width;
        for (size_t j = 0; j < matrix->frozen_trail_counts[i]; j++) {
            FrozenTrail* trail = &matrix->frozen_trails[i][j];
            for (size_t k = 0; k < TRAIL_LENGTH; k++) {
                int y = trail->y_pos - (int)(k + 1) * FONT_HEIGHT;
                if (y < 0) continue;

                int gradient_brightness = 255 - (k * (255 / TRAIL_LENGTH));
                if (gradient_brightness < 50) gradient_brightness = 50;
                if (gradient_brightness > 255) gradient_brightness = 255;
                int final_brightness = (int)(gradient_brightness * trail->alpha);
                if (final_brightness < 5) continue;

                SetTextColor(mem_dc, RGB(0, final_brightness, 0));
                WCHAR ch_buf[2] = { trail->characters[k], 0 };
                TextOutW(mem_dc, x, y, ch_buf, 1);
            }
        }
    }

    for (size_t i = 0; i < matrix->columns; i++) {
        if (matrix->frozen[i]) continue;

        int x = (int)i * matrix->char_width;
        int y = matrix->drops[i] * FONT_HEIGHT;

        for (size_t j = 0; j < TRAIL_LENGTH; j++) {
            int ty = y - (int)(j + 1) * FONT_HEIGHT;
            if (ty < 0) continue;
            int brightness = 255 - (j * (255 / TRAIL_LENGTH));
            if (brightness < 50) brightness = 50;
            if (brightness > 255) brightness = 255;
            SetTextColor(mem_dc, RGB(0, (BYTE)brightness, 0));
            WCHAR ch_buf[2] = { matrix->trail_chars[i][j], 0 };
            TextOutW(mem_dc, x, ty, ch_buf, 1);
        }

        SetTextColor(mem_dc, RGB(200, 244, 248));
        WCHAR lead[2] = { matrix->trail_chars[i][0], 0 };
        TextOutW(mem_dc, x, y, lead, 1);
    }

    BitBlt(hdc, 0, 0, matrix->width, matrix->height, mem_dc, 0, 0, SRCCOPY);

    SelectObject(mem_dc, old_bitmap);
    DeleteObject(mem_bitmap);
    DeleteDC(mem_dc);
    OutputDebugStringW(L"Matrix_render: Render complete\n");
}