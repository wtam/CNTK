#include "check.hpp"
#include "platform.hpp"

#include <iostream>
#include <stdarg.h>

using namespace std;

const int c_buffer_size = 1024;

void PrintErrorMessage(int err, const char* filename, int line_number, const char* format, va_list& args)
{
  // Log system error message if not success.
  if (err != 0)
  {
    char sys_error_meassage[c_buffer_size];
    Platform::strerror_s(sys_error_meassage, c_buffer_size, err);
    cerr << "SYSTEM ERRNO: " << sys_error_meassage << endl;
  }

  // Make formatted message.
  char error_message[c_buffer_size] = { 0 };
  Platform::vsnprintf_s(error_message, c_buffer_size, c_buffer_size - 1, format, args);
  // Log formatted message.
  cerr << "ERROR: " << error_message << " [file: " << filename << ", line: " << line_number << "]" << endl;
}

void Check(bool b, const char* filename, int line_number, const char* format, ...)
{
  if (!b)
  {
    // Just call shared PrintErrorMessage with success errno.
    va_list args;
    va_start(args, format);
    PrintErrorMessage(0, filename, line_number, format, args);
    va_end(args);
    throw;
  }
}

void CheckErrno(bool b, const char* filename, int line_number, const char* format, ...)
{
  if (!b)
  {
    // Take errno right away not to potentially spoil it with subsequent calls.
    int errno_saved = errno;
    // Delegate rest to the shared PrintErrorMessage call.
    va_list args;
    va_start(args, format);
    PrintErrorMessage(errno_saved, filename, line_number, format, args);
    va_end(args);
    throw;
  }
}