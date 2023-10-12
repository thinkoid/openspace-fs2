// -*- mode: c++; -*-

#ifndef FREESPACE2_STARFIELD_NEBULA_HH
#define FREESPACE2_STARFIELD_NEBULA_HH

#include <defs.hh>

// mainly only needed by Fred
extern int Nebula_pitch;
extern int Nebula_bank;
extern int Nebula_heading;

struct angles;

// You shouldn't pass the extension for filename.
// PBH = Pitch, Bank, Heading.   Pass NULL for default orientation.
void nebula_init (const char* filename, int pitch, int bank, int heading);
void nebula_init (const char* filename, angles_t* pbh = /*NULL*/ 0);
void nebula_close ();
void nebula_render ();

#define NEBULA_INDEXED_COLORS 20

#endif // FREESPACE2_STARFIELD_NEBULA_HH
