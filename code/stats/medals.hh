/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef FREESPACE_MEDAL_HEADER_FILE
#define FREESPACE_MEDAL_HEADER_FILE

#include <playerman/player.hh>
#include <stats/scoring.hh>

#define MAX_BADGES	3
#define MAX_ASSIGNABLE_MEDALS		12				// index into Medals array of the first medal which cannot be assigned

extern scoring_struct *Player_score;

// NUM_MEDALS stored in scoring.h since needed for player scoring structure

typedef struct medal_stuff {
	char	name[NAME_LENGTH+1];
	char	bitmap[NAME_LENGTH];
	int	num_versions;
	int	kills_needed;
} medal_stuff;

typedef struct badge_stuff {
	char voice_base[MAX_FILENAME_LEN + 1];
	char *promotion_text;
} badge_stuff;

extern medal_stuff Medals[NUM_MEDALS];
extern badge_stuff Badge_info[MAX_BADGES];
extern int Badge_index[MAX_BADGES];				// array which contains indices into Medals to indicate which medals are badges

extern void parse_medal_tbl();

// modes for this screen
#define MM_NORMAL							0			// normal - run through the state code
#define MM_POPUP							1			// called from within some other tight loop (don't use gameseq_ functions)

// main medals screen
void medal_main_init(player *pl,int mode = MM_NORMAL);

// return 0 if the screen should close (used for MM_POPUP mode)
int medal_main_do();
void medal_main_close();

//void init_medal_palette();
void init_medal_bitmaps();
void init_snazzy_regions();
void blit_medals();
void blit_label(char *label,int *coords);
void blit_callsign();

// individual medals 

extern int Medal_ID;       // ID of the medal to display in this screen. Should be set by the caller

void blit_text();

void medals_translate_name(char *name, int max_len);

#endif
