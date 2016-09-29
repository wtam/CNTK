#include "check.hpp"
#include "platform.hpp"

#include <iostream>
#include <stdarg.h>

using namespace std;

void CHECK(bool b, const char* format, ...)
{
  if (!b)
  {
    // Make formatted message.
    const int BUFFER_SIZE = 1024;
    char error_message[BUFFER_SIZE] = { 0 };

    va_list args;
    va_start(args, format);
    Platform::vsnprintf_s(error_message, BUFFER_SIZE, BUFFER_SIZE - 1, format, args);

    // Log message to err and throw.
    cerr << "ERROR: " << error_message << endl;
    throw;
  }
}