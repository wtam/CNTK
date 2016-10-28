#include "platform.hpp"

#include <algorithm>

using namespace std;

namespace Platform
{

#ifdef _MSC_VER

int fopen_s(FILE** pFile, const char *filename, const char *mode)
{
    return ::fopen_s(pFile, filename, mode);
}

int64_t ftell64(FILE* stream)
{
    return _ftelli64(stream);
}

int fseek64(FILE* stream, int64_t offset, int origin)
{
    return _fseeki64(stream, offset, origin);
}

int strcpy_s(char *strDestination, size_t numberOfElements, const char *strSource)
{
    return ::strcpy_s(strDestination, numberOfElements, strSource);
}

int vsnprintf_s(char* buffer, size_t sizeOfBuffer, size_t count, const char* format, va_list argptr)
{
  return ::vsnprintf_s(buffer, sizeOfBuffer, count, format, argptr);
}

int strerror_s(char* buffer, size_t numberOfElements, int errnum)
{
  return ::strerror_s(buffer, numberOfElements, errnum);
}

#else

int fopen_s(FILE** pFile, const char *filename, const char *mode)
{
    *pFile = fopen(filename, mode);
    return (*pFile != nullptr) ? 0 : 1;
}

int64_t ftell64(FILE* stream)
{
    return ftello64(stream);
}

int fseek64(FILE* stream, int64_t offset, int origin)
{
    return fseeko64(stream, offset, origin);
}

int strcpy_s(char *strDestination, size_t numberOfElements, const char *strSource)
{
    strcpy(strDestination, strSource);
    return 0;
}

int vsnprintf_s(char* buffer, size_t sizeOfBuffer, size_t /*count*/, const char* format, va_list argptr)
{
  return vsnprintf(buffer, sizeOfBuffer, format, argptr);
}

int strerror_s(char* buffer, size_t numberOfElements, int errnum)
{
  char* mess = strerror(errnum);
  size_t len = min(numberOfElements, strlen(mess));
  strncpy(buffer, mess, len);
  return 0;
}

#endif

}
