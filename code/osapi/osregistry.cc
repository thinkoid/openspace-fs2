/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <map>
#include <string>

#include <globalincs/pstypes.hh>
#include <osapi/osregistry.hh>

// The Win32 registry is replaced with a plain "key=value" text file at
// ~/.fs2/config.  Keys are "Section/Name" (the section defaults to "Default"
// when the caller passes NULL, which is what the Win32 code used the plain
// app key for).  The file is loaded lazily into an in-memory map and written
// through on every write.

// ------------------------------------------------------------------------------------------------------------
// REGISTRY DEFINES/VARS
//

static char			szCompanyName[128];
static char			szAppName[128];
static char			szAppVersion[128];

char *Osreg_company_name = "Volition";
char *Osreg_class_name = "Freespace2Class";
#if defined(FS2_DEMO)
char *Osreg_app_name = "FreeSpace2Demo";
char *Osreg_title = "Freespace 2 Demo";
#elif defined(OEM_BUILD)
char *Osreg_app_name = "FreeSpace2OEM";
char *Osreg_title = "Freespace 2 OEM";
#else
char *Osreg_app_name = "FreeSpace2";
char *Osreg_title = "Freespace 2";
#endif

int Os_reg_inited = 0;

static std::map<std::string, std::string> Config_map;
static int Config_loaded = 0;

// full path of the config file; creates ~/.fs2 on first use
static const char *config_file_name()
{
	static char path[1024] = "";

	if ( !path[0] )	{
		const char *home = getenv("HOME");
		if ( !home )	{
			home = ".";
		}
		sprintf( path, "%s/.fs2", home );
		mkdir( path, 0755 );					// make sure ~/.fs2 exists
		strcat( path, "/config" );
	}

	return path;
}

// build the "Section/Name" key of an entry
static std::string config_key( char *section, char *name )
{
	std::string key = section ? section : "Default";

	key += "/";
	key += name;

	return key;
}

static void config_load()
{
	char line[1024];

	if ( Config_loaded )	{
		return;
	}
	Config_loaded = 1;

	FILE *f = fopen( config_file_name(), "r" );
	if ( !f )	{
		return;			// no config yet - all reads fall back on defaults
	}

	while ( fgets(line, sizeof(line), f) )	{
		char *p = strchr( line, '\n' );
		if ( p )	{
			*p = 0;
		}
		p = strchr( line, '=' );
		if ( !p )	{
			continue;
		}
		*p = 0;
		Config_map[line] = p + 1;
	}

	fclose(f);
}

static void config_save()
{
	FILE *f = fopen( config_file_name(), "w" );
	if ( !f )	{
		return;
	}

	for ( const auto &entry : Config_map )	{
		fprintf( f, "%s=%s\n", entry.first.c_str(), entry.second.c_str() );
	}

	fclose(f);
}


// ------------------------------------------------------------------------------------------------------------
// REGISTRY FUNCTIONS
//

// os registry functions -------------------------------------------------------------

// initialize the registry. setup default keys to use
void os_init_registry_stuff(char *company, char *app, char *version)
{
	if(company){
		strcpy( szCompanyName, company );
	} else {
		strcpy( szCompanyName, Osreg_company_name);
	}

	if(app){
		strcpy( szAppName, app );
	} else {
		strcpy( szAppName, Osreg_app_name);
	}

	if(version){
		strcpy( szAppVersion, version);
	} else {
		strcpy( szAppVersion, "1.0");
	}

	Os_reg_inited = 1;
}

// Removes a value from to the INI file.  Passing
// name=NULL will delete the section.
void os_config_remove( char *section, char *name )
{
	if(!Os_reg_inited){
		return;
	}

	config_load();

	if ( name )	{
		Config_map.erase( config_key(section, name) );
	} else	{
		// delete the whole section
		std::string prefix = std::string(section ? section : "Default") + "/";

		for ( auto it = Config_map.begin(); it != Config_map.end(); )	{
			if ( it->first.compare(0, prefix.size(), prefix) == 0 )	{
				it = Config_map.erase(it);
			} else	{
				++it;
			}
		}
	}

	config_save();
}

// Writes a string to the INI file.  If value is NULL,
// removes the string. Writing a NULL value to a NULL name will delete
// the section.
void os_config_write_string( char *section, char *name, char *value )
{
	if(!Os_reg_inited){
		return;
	}

	if ( !value )	{
		os_config_remove( section, name );
		return;
	}

	if ( !name )	{
		return;
	}

	config_load();
	Config_map[ config_key(section, name) ] = value;
	config_save();
}

// same as previous function except we don't use the application name to build up the keyname
// (with a single per-user config file the distinction is gone)
void os_config_write_string2( char *section, char *name, char *value )
{
	os_config_write_string( section, name, value );
}

// Writes an unsigned int to the INI file.
void os_config_write_uint( char *section, char *name, uint value )
{
	char tmp[32];

	if(!Os_reg_inited){
		return;
	}

	if ( !name )	{
		return;
	}

	sprintf( tmp, "%u", value );

	config_load();
	Config_map[ config_key(section, name) ] = tmp;
	config_save();
}


// Reads a string from the INI file.  If default is passed,
// and the string isn't found, returns ptr to default otherwise
// returns NULL;    Copy the return value somewhere before
// calling os_read_string again, because it might reuse the
// same buffer.
static char tmp_string_data[1024];
char * os_config_read_string( char *section, char *name, char *default_value )
{
	if(!Os_reg_inited){
		return NULL;
	}

	if ( !name )	{
		return default_value;
	}

	config_load();

	auto it = Config_map.find( config_key(section, name) );
	if ( it == Config_map.end() )	{
		return default_value;
	}

	strncpy( tmp_string_data, it->second.c_str(), sizeof(tmp_string_data) - 1 );
	tmp_string_data[sizeof(tmp_string_data) - 1] = 0;

	return tmp_string_data;
}

// same as previous function except we don't use the application name to build up the keyname
char * os_config_read_string2( char *section, char *name, char *default_value )
{
	return os_config_read_string( section, name, default_value );
}

// Reads a string from the INI file.  Default_value must
// be passed, and if 'name' isn't found, then returns default_value
uint  os_config_read_uint( char *section, char *name, uint default_value )
{
	if(!Os_reg_inited){
		return 0;
	}

	if ( !name )	{
		return default_value;
	}

	config_load();

	auto it = Config_map.find( config_key(section, name) );
	if ( it == Config_map.end() )	{
		return default_value;
	}

	return (uint)strtoul( it->second.c_str(), NULL, 10 );
}

// uses Ex versions of Windows registry functions - here the explicit keyname
// is simply used as the section
char * os_config_read_string_ex( char *keyname, char *name, char *default_value )
{
	if ( !name )	{
		return default_value;
	}

	config_load();

	auto it = Config_map.find( config_key(keyname, name) );
	if ( it == Config_map.end() )	{
		return default_value;
	}

	strncpy( tmp_string_data, it->second.c_str(), sizeof(tmp_string_data) - 1 );
	tmp_string_data[sizeof(tmp_string_data) - 1] = 0;

	return tmp_string_data;
}
