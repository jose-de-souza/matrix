#ifndef RESOURCE_H
#define RESOURCE_H

// Defines a unique identifier (101) for the application's icon resource.
// This ID is used in matrix.rc to link the icon file (matrix_icon.ico) to the
// executable and in main.c to load the icon via LoadIconW. Resource IDs are
// essential in Windows programming for referencing assets like icons, menus, or
// dialogs. The value 101 is arbitrary but must match the ID in matrix.rc to
// ensure the linker embeds the correct icon. Using a header file for resource
// definitions promotes consistency across the project and avoids hardcoding IDs
// in multiple places, reducing the risk of mismatches.
#define IDI_ICON1 101

// The include guard prevents multiple inclusions of this file, which could cause
// redefinition errors during compilation. This is a standard C practice to ensure
// header files are processed only once per translation unit, maintaining clean
// and efficient compilation.
#endif // RESOURCE_H