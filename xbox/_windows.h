// fileapi

#include <windef.h>

#define INVALID_FILE_ATTRIBUTES -1

DWORD GetFileAttributesA(
  LPCSTR lpFileName
);

#define GetFileAttributes(...) GetFileAttributesA(__VA_ARGS__)
