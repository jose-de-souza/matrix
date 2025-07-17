#ifndef CHARSET_H
#define CHARSET_H

// Array of Unicode characters used in the Matrix animation.
// WCHAR (wchar_t) is used for 16-bit Unicode characters, supporting a wide range
// of symbols (e.g., Katakana, Cyrillic). The array includes digits (0-1),
// Cyrillic letters (0x0410-0x042F), and Katakana characters (0x30A2-0x30F3),
// terminated by 0x0000 to mark the end. This array is used by random_char in
// matrix.c to select characters for trails, ensuring visual variety and
// authenticity to the Matrix aesthetic. Static limits scope to this file,
// preventing external access and reducing namespace pollution. The const
// qualifier ensures the array is read-only, preventing accidental modification.
static const WCHAR letters[] = {
    0x0030, 0x0031, // Digits 0-1
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 0x0419,
    0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F, 0x0420, 0x0421, 0x0422, 0x0423,
    0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D,
    0x042E, 0x042F, // Cyrillic letters А-Я
    0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3,
    0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF, 0x30C1, 0x30C3, 0x30C6, 0x30C8,
    0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB,
    0x30DE, 0x30DF, 0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8,
    0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F2, 0x30F3, // Katakana characters
    0x0000 // Null terminator to mark array end
};

// Include guard prevents multiple inclusions, ensuring the letters array is
// defined only once. This is critical for avoiding redefinition errors during
// compilation, a standard C practice for header files.
#endif