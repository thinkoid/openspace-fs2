/*
 * Portable replacement for windebug.cpp (Win32 assert dialogs, debugger
 * output, tracking heap).  Errors and debug output go to stderr; asserts
 * abort so a debugger or core dump catches them.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "pstypes.h"

void WinAssert(const char *text, const char *filename, int line)
{
	fprintf(stderr, "Assertion failed: %s, file %s, line %d\n", text, filename, line);
	abort();
}

void Error(const char *filename, int line, const char *format, ...)
{
	va_list args;

	fprintf(stderr, "Error: %s, line %d: ", filename, line);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	abort();
}

void Warning(const char *filename, int line, const char *format, ...)
{
	va_list args;

	fprintf(stderr, "Warning: %s, line %d: ", filename, line);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
}

void debug_int3()
{
	abort();
}

// ----------------------------------------------------------------------
// debug output (the old Win32 output window); filter ids are ignored
// ----------------------------------------------------------------------

#ifndef NDEBUG
int Log_debug_output_to_file = 0;
#endif

void load_filter_info(void)
{
}

void outwnd_init(int /*display_under_freespace_window*/)
{
}

void outwnd_close()
{
}

void outwnd_printf(const char * /*id*/, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

void outwnd_printf2(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

// ----------------------------------------------------------------------
// heap wrappers (the tracking heap is gone)
// ----------------------------------------------------------------------

int vm_init(int /*min_heap_size*/)
{
	return 1;
}

long filelength(int fd)
{
	struct stat st;
	if (fstat(fd, &st) != 0)
		return -1;
	return (long)st.st_size;
}

void *vm_malloc(int size)
{
	return malloc(size);
}

char *vm_strdup(const char *ptr)
{
	return strdup(ptr);
}

void vm_free(void *ptr)
{
	free(ptr);
}

void vm_free_all()
{
}

