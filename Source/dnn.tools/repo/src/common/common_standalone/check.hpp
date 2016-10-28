#pragma once

// If given variable is false prints the formatted message and throws.
void Check(bool b, const char* filename, int line_number, const char* format, ...);

// If given variable is false prints the string equivalent of system error number then prints given formatted message
// and throws.
void CheckErrno(bool b, const char* filename, int line_number, const char* format, ...);

// Macros below should be used instead of functions to automatically log file and line number.
#define CHECK(b, format, ...) \
Check(b, __FILE__, __LINE__, format, ##__VA_ARGS__);

#define CHECK_ERRNO(b, format, ...) \
CheckErrno(b, __FILE__, __LINE__, format, ##__VA_ARGS__);