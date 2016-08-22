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

}