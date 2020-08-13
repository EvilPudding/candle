#include <windows.h>
#include <fileapi.h>

int modified_time(const char *filename, ULONGLONG *mtime)
{
	HANDLE fh;
	FILETIME ft;
	ULARGE_INTEGER ft_int64;

	fh = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
	                NULL, OPEN_EXISTING, 0, NULL);

	if (fh == INVALID_HANDLE_VALUE)
	{
		CloseHandle(fh);
		return 1;
	}

	if (GetFileTime(fh, NULL, NULL, &ft) == 0)
	{
		CloseHandle(fh);
		return 1;
	}

	ft_int64.HighPart = ft.dwHighDateTime;
	ft_int64.LowPart = ft.dwLowDateTime;

	// Convert from hectonanoseconds since 01/01/1601 to seconds since 01/01/1970
	*mtime = ft_int64.QuadPart / 10000000ULL - 11644473600ULL;

	CloseHandle(fh);
	return 0;
}

int main(int argc, char *argv[])
{
	ULONGLONG t0, t1;
	if (modified_time(argv[1], &t0))
		return 1;
	if (modified_time(argv[2], &t1))
		return 1;
	return t0 > t1;
}
