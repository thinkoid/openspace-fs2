// Loose-file cfile backend for point-1 oracles: just enough of the cfile API
// for parselo's read_file_text (open/length/seek/read/close), backed by stdio,
// no VP archives.  Replaced wholesale by the real cfile port in point 2.

#include <stdio.h>

#include "pstypes.h"
#include "cfile.h"

// The real CFILE is an index into cfile.cpp's table; here it is just a FILE*
// in disguise.  Nothing outside this file looks inside.

CFILE *cfopen(char *filename, char *mode, int /*type*/, int /*dir_type*/, bool /*localize*/)
{
	return (CFILE *)fopen(filename, mode);
}

int cfclose(CFILE *cfile)
{
	return fclose((FILE *)cfile);
}

int cfilelength(CFILE *fp)
{
	FILE *f = (FILE *)fp;
	long pos = ftell(f);
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, pos, SEEK_SET);
	return (int)len;
}

int cfread(void *buf, int elsize, int nelem, CFILE *fp)
{
	return (int)fread(buf, elsize, nelem, (FILE *)fp);
}

int cfseek(CFILE *fp, int offset, int where)
{
	return fseek((FILE *)fp, offset, where);
}
