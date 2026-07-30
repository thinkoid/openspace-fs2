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

#include <globalincs/pstypes.hh>

void
WinAssert(const char *text, const char *filename, int line)
{
    fprintf(stderr, "Assertion failed: %s, file %s, line %d\n", text, filename,
            line);
    abort();
}

void
Error(const char *filename, int line, const char *format, ...)
{
    va_list args;

    fprintf(stderr, "Error: %s, line %d: ", filename, line);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    abort();
}

void
Warning(const char *filename, int line, const char *format, ...)
{
    va_list args;

    fprintf(stderr, "Warning: %s, line %d: ", filename, line);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void
debug_int3()
{
    // Retail's Int3() was a continuable debugger breakpoint; some call
    // sites break and carry on after it (ship_make_create_time_unique
    // trips it legitimately when 50+ ships materialize in one millisecond,
    // which FRED-style bulk creation does). The game keeps fail-fast;
    // tools that replicate FRED's bulk paths opt into retail's continuable
    // semantics via the environment.
    if (getenv("FS2_INT3_CONTINUE")) {
        fprintf(stderr, "Int3 (continuing: FS2_INT3_CONTINUE is set)\n");
        return;
    }
    abort();
}

// ----------------------------------------------------------------------
// debug output (the old Win32 output window); filter ids are ignored
// ----------------------------------------------------------------------

void
outwnd_init(int /*display_under_freespace_window*/)
{ }

void
outwnd_close()
{ }

void
outwnd_printf(const char * /*id*/, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

void
outwnd_printf2(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

// ----------------------------------------------------------------------
// heap wrappers (the tracking heap is gone)
// ----------------------------------------------------------------------

int
vm_init(int /*min_heap_size*/)
{
    return 1;
}

long
filelength(int fd)
{
    struct stat st;
    if (fstat(fd, &st) != 0)
        return -1;
    return (long)st.st_size;
}

void
_splitpath(const char *path, char *drive, char *dir, char *fname, char *ext)
{
    if (drive)
        drive[0] = 0; // no drives here

    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    if (dir) {
        strncpy(dir, path, base - path);
        dir[base - path] = 0;
    }

    const char *dot = strrchr(base, '.');
    if (!dot)
        dot = base + strlen(base);

    if (fname) {
        strncpy(fname, base, dot - base);
        fname[dot - base] = 0;
    }
    if (ext)
        strcpy(ext, dot);
}

void *
vm_malloc(int size)
{
    return malloc(size);
}

void
vm_free(void *ptr)
{
    free(ptr);
}
