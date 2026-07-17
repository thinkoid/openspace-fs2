/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/ 

#include <string.h>
#include <stdlib.h>
#include "cmdline.h"
#include "linklist.h"
#include "systemvars.h"
#include "multi.h"
#include "cfile.h"

// variables
class cmdline_parm {
public:
	cmdline_parm *next, *prev;
	char *name;						// name of parameter, must start with '-' char
	char *help;						// help text for this parameter
	char *args;						// string value for parameter arguements (NULL if no arguements)
	int name_found;				// true if parameter on command line, otherwise false

	cmdline_parm(char *name, char *help);
	~cmdline_parm();
	int found();
	int get_int();
	float get_float();
	char *str();
};

// here are the command line parameters that we will be using for FreeSpace
cmdline_parm standalone_arg("-standalone", NULL);
cmdline_parm nosound_arg("-nosound", NULL);
cmdline_parm nomusic_arg("-nomusic", NULL);
cmdline_parm startgame_arg("-startgame", NULL);
cmdline_parm gamename_arg("-gamename", NULL);
cmdline_parm gamepassword_arg("-password", NULL);
cmdline_parm gameclosed_arg("-closed", NULL);
cmdline_parm gamerestricted_arg("-restricted", NULL);
cmdline_parm allowabove_arg("-allowabove", NULL);
cmdline_parm allowbelow_arg("-allowbelow", NULL);
cmdline_parm port_arg("-port", NULL);
cmdline_parm connect_arg("-connect", NULL);
cmdline_parm multilog_arg("-multilog", NULL);
cmdline_parm server_firing_arg("-oldfire", NULL);
cmdline_parm client_dodamage("-clientdamage", NULL);
cmdline_parm pof_spew("-pofspew", NULL);
cmdline_parm d3d_32bit("-32bit", NULL);
cmdline_parm mouse_coords("-coords", NULL);
cmdline_parm timeout("-timeout", NULL);
cmdline_parm d3d_window("-window", NULL);

int Cmdline_multi_stream_chat_to_file = 0;
int Cmdline_freespace_no_sound = 0;
int Cmdline_freespace_no_music = 0;
int Cmdline_gimme_all_medals = 0;
int Cmdline_use_last_pilot = 0;
int Cmdline_multi_protocol = -1;
int Cmdline_cd_check = 1;
int Cmdline_start_netgame = 0;
int Cmdline_closed_game = 0;
int Cmdline_restricted_game = 0;
int Cmdline_network_port = -1;
char *Cmdline_game_name = NULL;
char *Cmdline_game_password = NULL;
char *Cmdline_rank_above= NULL;
char *Cmdline_rank_below = NULL;
char *Cmdline_connect_addr = NULL;
int Cmdline_multi_log = 0;
int Cmdline_server_firing = 0;
int Cmdline_client_dodamage = 0;
int Cmdline_spew_pof_info = 0;
int Cmdline_force_32bit = 0;
int Cmdline_mouse_coords = 0;
int Cmdline_timeout = -1;

int Cmdline_window = 0;

static cmdline_parm Parm_list(NULL, NULL);
static int Parm_list_inited = 0;


//	Return true if this character is an extra char (white space and quotes)
int is_extra_space(char ch)
{
	return ((ch == ' ') || (ch == '\t') || (ch == 0x0a) || (ch == '\'') || (ch == '\"'));
}


// eliminates all leading and trailing extra chars from a string.  Returns pointer passed in.
char *drop_extra_chars(char *str)
{
	int s, e;

	s = 0;
	while (str[s] && is_extra_space(str[s]))
		s++;

	e = strlen(str) - 1;
	while (e > s) {
		if (!is_extra_space(str[e])){
			break;
		}

		e--;
	}

	if (e > s){
		memmove(str, str + s, e - s + 1);
	}

	str[e - s + 1] = 0;
	return str;
}


// internal function - copy the value for a parameter agruement into the cmdline_parm arg field
void parm_stuff_args(cmdline_parm *parm, char *cmdline)
{
	char buffer[1024];
	memset(buffer, 0, 1024);
	char *dest = buffer;

	cmdline += strlen(parm->name);

	while ((*cmdline != 0) && (*cmdline != '-')) {
		*dest++ = *cmdline++;
	}

	drop_extra_chars(buffer);

	// mwa 9/14/98 -- made it so that newer command line arguments found will overwrite
	// the old arguments
//	Assert(parm->args == NULL);
	if ( parm->args != NULL ) {
		delete( parm->args );
		parm->args = NULL;
	}

	int size = strlen(buffer) + 1;
	if (size > 0) {
		parm->args = new char[size];
		memset(parm->args, 0, size);
		strcpy(parm->args, buffer);
	}
}


// internal function - parse the command line, extracting parameter arguements if they exist
// cmdline - command line string passed to the application
void os_parse_parms(char *cmdline)
{
	// locate command line parameters
	cmdline_parm *parmp;
	char *cmdline_offset;

	for (parmp = GET_FIRST(&Parm_list); parmp !=END_OF_LIST(&Parm_list); parmp = GET_NEXT(parmp) ) {
		cmdline_offset = strstr(cmdline, parmp->name);
		if (cmdline_offset) {
			parmp->name_found = 1;
			parm_stuff_args(parmp, cmdline_offset);
		}
	}
}


// validate the command line parameters.  Display an error if an unrecognized parameter is located.
void os_validate_parms(char *cmdline)
{
	cmdline_parm *parmp;
	char seps[] = " ,\t\n";
	char *token;
	int parm_found;

   token = strtok(cmdline, seps);
   while(token != NULL) {
	
		if (token[0] == '-') {
			parm_found = 0;
			for (parmp = GET_FIRST(&Parm_list); parmp !=END_OF_LIST(&Parm_list); parmp = GET_NEXT(parmp) ) {
				if (!stricmp(parmp->name, token)) {
					parm_found = 1;
					break;
				}
			}

			if (parm_found == 0) {
				Error(LOCATION,"Unrecogzined command line parameter %s", token);
			}
		}

		token = strtok(NULL, seps);
	}
}


// Call once to initialize the command line system
//
// cmdline - command line string passed to the application
void os_init_cmdline(char *cmdline)
{
	FILE *fp;

	// read the cmdline.cfg file from the data folder, and pass the command line arguments to
	// the the parse_parms and validate_parms line.  Read these first so anything actually on
	// the command line will take precedence
	fp = fopen("data\\cmdline.cfg", "rt");

	// if the file exists, get a single line, and deal with it
	if ( fp ) {
		char buf[1024], *p;

		fgets(buf, 1024, fp);

		// replace the newline character with a NUL:
		if ( (p = strrchr(buf, '\n')) != NULL ) {
			*p = '\0';
		}

		os_parse_parms(buf);
		os_validate_parms(buf);
		fclose(fp);
	}



	os_parse_parms(cmdline);
	os_validate_parms(cmdline);

}


// arg constructor
// name_ - name of the parameter, must start with '-' character
// help_ - help text for this parameter
cmdline_parm::cmdline_parm(char *name_, char *help_)
{
	name = name_;
	help = help_;
	args = NULL;
	name_found = 0;

	if (Parm_list_inited == 0) {
		list_init(&Parm_list);
		Parm_list_inited = 1;
	}

	if (name != NULL) {
		list_append(&Parm_list, this);
	}
}


// destructor - frees any allocated memory
cmdline_parm::~cmdline_parm()
{
	if (args) {
		delete [] args;
		args = NULL;
	}
}


// returns - true if the parameter exists on the command line, otherwise false
int cmdline_parm::found()
{
	return name_found;
}


// returns - the interger representation for the parameter arguement
int cmdline_parm::get_int()
{
	Assert(args);
	return atoi(args);
}


// returns - the float representation for the parameter arguement
float cmdline_parm::get_float()
{
	Assert(args);
	return (float)atof(args);
}


// returns - the string value for the parameter arguement
char *cmdline_parm::str()
{
	Assert(args);
	return args;
}

// external entry point into this modules
int parse_cmdline(char *cmdline)
{
	os_init_cmdline(cmdline);

	// is this a standalone server??
	if (standalone_arg.found()) {
		Is_standalone = 1;
	}

	// run with no sound
	if ( nosound_arg.found() ) {
		Cmdline_freespace_no_sound = 1;
	}

	// run with no music
	if ( nomusic_arg.found() ) {
		Cmdline_freespace_no_music = 1;
	}

	// should we start a network game
	if ( startgame_arg.found() ) {
		Cmdline_use_last_pilot = 1;
		Cmdline_start_netgame = 1;
	}

	// closed network game
	if ( gameclosed_arg.found() ) {
		Cmdline_closed_game = 1;
	}

	// restircted network game
	if ( gamerestricted_arg.found() ) {
		Cmdline_restricted_game = 1;
	}

	// get the name of the network game
	if ( gamename_arg.found() ) {
		Cmdline_game_name = gamename_arg.str();

		// be sure that this string fits in our limits
		if ( strlen(Cmdline_game_name) > MAX_GAMENAME_LEN ) {
			Cmdline_game_name[MAX_GAMENAME_LEN-1] = '\0';
		}
	}

	// get the password for a pssword game
	if ( gamepassword_arg.found() ) {
		Cmdline_game_password = gamepassword_arg.str();

		// be sure that this string fits in our limits
		if ( strlen(Cmdline_game_name) > MAX_PASSWD_LEN ) {
			Cmdline_game_name[MAX_PASSWD_LEN-1] = '\0';
		}
	}

	// set the rank above/below arguments
	if ( allowabove_arg.found() ) {
		Cmdline_rank_above = allowabove_arg.str();
	}
	if ( allowbelow_arg.found() ) {
		Cmdline_rank_below = allowbelow_arg.str();
	}

	// get the port number for games
	if ( port_arg.found() ) {
		Cmdline_network_port = port_arg.get_int();
	}

	// the connect argument specifies to join a game at this particular address
	if ( connect_arg.found() ) {
		Cmdline_use_last_pilot = 1;
		Cmdline_connect_addr = connect_arg.str();
	}

	// see if the multilog flag was set
	if ( multilog_arg.found() ){
		Cmdline_multi_log = 1;
	}	

	// maybe use old-school server-side firing
	if (server_firing_arg.found() ){
		Cmdline_server_firing = 1;
	}

	// maybe use old-school client damage
	if(client_dodamage.found()){
		Cmdline_client_dodamage = 1;
	}	

	// spew pof info
	if(pof_spew.found()){
		Cmdline_spew_pof_info = 1;
	}

	// 32 bit
	if(d3d_32bit.found()){
		Cmdline_force_32bit = 1;
	}

	// mouse coords
	if(mouse_coords.found()){
		Cmdline_mouse_coords = 1;
	}

	// net timeout
	if(timeout.found()){
		Cmdline_timeout = timeout.get_int();
	}

	// d3d windowed
	if(d3d_window.found()){
		Cmdline_window = 1;
	}

	return 1;
}
