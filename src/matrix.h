#ifndef MATRIX_H
#define MATRIX_H

#include <windows.h>

typedef struct {
    WCHAR characters[22]; // TRAIL_LENGTH
    int y_pos;
    float alpha;
} FrozenTrail;

typedef struct {
    HWND hwnd;
    HFONT hfont;
    int width;
    int height;
    int char_width;
    size_t columns;
    int* drops;
    WCHAR** trail_chars;
    BOOL* frozen;
    FrozenTrail** frozen_trails;
    size_t* frozen_trail_counts;
    size_t* frozen_trail_capacities;
    unsigned int rng_state;
    long long last_update;
} Matrix;

Matrix* Matrix_init(HWND hwnd, int width, int height);
void Matrix_deinit(Matrix* matrix);
void Matrix_update(Matrix* matrix);
void Matrix_render(Matrix* matrix, HDC hdc);

#endif