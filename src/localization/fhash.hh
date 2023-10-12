// -*- mode: c++; -*-

#ifndef FREESPACE2_LOCALIZATION_FHASH_HH
#define FREESPACE2_LOCALIZATION_FHASH_HH

#include <defs.hh>

// -----------------------------------------------------------------------------------------------
// HASH DEFINES/VARS
//

// -----------------------------------------------------------------------------------------------
// HASH FUNCTIONS
//

// initialize the hash table
void fhash_init ();

// set the hash table to be active for parsing
void fhash_activate ();

// set the hash table to be inactive for parsing
void fhash_deactivate ();

// if the hash table is active
int fhash_active ();

// flush out the hash table, freeing up everything
void fhash_flush ();

// add a string with the given id# to the hash table
void fhash_add_str (const char* str, int id);

// determine if the passed string exists in the table
// returns : -2 if the string doesn't exit, or >= -1 as the string id #
// otherwise
int fhash_string_exists (const char* str);

#endif // FREESPACE2_LOCALIZATION_FHASH_HH
