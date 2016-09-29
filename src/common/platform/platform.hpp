#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string.h>

namespace Platform
{

int fopen_s(FILE** pFile, const char *filename, const char *mode);

int64_t ftell64(FILE* stream);

int fseek64(FILE* stream, int64_t offset, int origin);

int strcpy_s(char *strDestination, size_t numberOfElements, const char *strSource);

// Creates formatted string
// Parameters:
//  buffer - Storage location for output.
//  sizeOfBuffer - The size of the buffer for output, as the character count.
//  count - Maximum number of characters to write (not including the terminating null).
//  format - Format specification.
//  argptr - Pointer to list of arguments.
int vsnprintf_s(char* buffer, size_t sizeOfBuffer, size_t count, const char* format, ::va_list argptr);

}