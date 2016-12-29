#pragma once

enum SeverityLevel
{
  normal,  // Logged to cout
  warning, // Logged to cerr
};

// Prints the formatted log message
void Log(const char* filename, int line_number, SeverityLevel sv_level, const char* format, ...);

// Macros below should be used instead of functions to automatically log file and line number.
#define LOG(format, ...) \
  Log(__FILE__, __LINE__, SeverityLevel::normal, format, ##__VA_ARGS__);

#define WARNING(format, ...) \
  Log(__FILE__, __LINE__, SeverityLevel::warning, format, ##__VA_ARGS__);