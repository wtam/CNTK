#include "check.hpp"
#include "log.hpp"
#include "platform.hpp"

#include <iostream>
#include <stdarg.h>

using namespace std;

const int c_buffer_size = 1024;

void PrintMessage(const char* filename, int line_number, SeverityLevel sv_level, const char* format, va_list& args)
{
  // Make formatted message.
  char message[c_buffer_size] = { 0 };
  Platform::vsnprintf_s(message, c_buffer_size, c_buffer_size - 1, format, args);
  // Log formatted message.
  switch (sv_level)
  {
  case SeverityLevel::normal:
    cout << "LOG: " << message << " [file: " << filename << ", line: " << line_number << "]" << endl;
    break;
  case SeverityLevel::warning:
    cerr << "WARNING: " << message << " [file: " << filename << ", line: " << line_number << "]" << endl;
    break;
  default:
    CHECK(false, "Unsupported severity level used while logging");
    break;
  }
}

void Log(const char* filename, int line_number, SeverityLevel sv_level, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  PrintMessage(filename, line_number, sv_level, format, args);
  va_end(args);
}
