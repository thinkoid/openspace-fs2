// Temporary link stubs for game globals referenced by the foundation but
// owned by subsystems not yet ported.  Every entry here is a debt; delete it
// when the owning subsystem comes in.

#include <stdarg.h>
#include <stdio.h>

#include "pstypes.h"

// freespace2/freespace.cpp
int Fred_running = 0;
int Pofview_running = 0;
int Nebedit_running = 0;

// graphics/2d.cpp
void gr_activate(int /*active*/)
{
}

// debugconsole/console.cpp
int Dc_command;
int Dc_help;
int Dc_status;
char *Dc_arg;
char *Dc_arg_org;
uint Dc_arg_type;
char *Dc_command_line;
int Dc_arg_int;
float Dc_arg_float;

void dc_printf(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

void dc_get_arg(uint /*flags*/)
{
}

debug_command::debug_command(const char *_name, const char *_help, void (*_func)())
{
	name = _name;
	help = _help;
	func = _func;
}

// io/timer.cpp — real values so time-dependent code behaves; replaced by the
// ported timer in the platform phase
#include <time.h>

static longlong nanotime()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (longlong)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

int timer_get_milliseconds()
{
	return (int)(nanotime() / 1000000);
}

int timer_get_microseconds()
{
	return (int)(nanotime() / 1000);
}

fix timer_get_fixed_seconds()
{
	// 16.16 fixed-point seconds, rebased to first call so the *65536 can't
	// overflow on long uptimes
	static longlong base = nanotime();
	return (fix)((nanotime() - base) * 65536 / 1000000000);
}

// nebula/neb.cpp
void neb2_set_detail_level(int /*level*/)
{
}
