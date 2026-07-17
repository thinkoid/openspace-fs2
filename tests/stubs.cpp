// Temporary link stubs for game globals referenced by the foundation but
// owned by subsystems not yet ported.  Every entry here is a debt; delete it
// when the owning subsystem comes in.

#include <stdarg.h>
#include <stdio.h>

#include "pstypes.h"


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


// network/multiutil.cpp:980 — real body; single-player mission load
// checksums mission data with it
ushort netmisc_calc_checksum( void * vptr, int len )
{
	ubyte * ptr = (ubyte *)vptr;
	unsigned int sum1,sum2;

	sum1 = sum2 = 0;

	while(len--)	{
		sum1 += *ptr++;
		if (sum1 >= 255 ) sum1 -= 255;
		sum2 += sum1;
	}
	sum2 %= 255;

	return (unsigned short)((sum1<<8)+ sum2);
}
