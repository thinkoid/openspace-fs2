// -*- mode: c++; -*-

#ifndef FREESPACE2_STATS_MEDALS_HH
#define FREESPACE2_STATS_MEDALS_HH

#include <defs.hh>

class scoring_struct;
class player;

#define MAX_BADGES 3
extern int Rank_medal_index;

extern scoring_struct* Player_score;

// NUM_MEDALS stored in scoring.h since needed for player scoring structure

class medal_stuff {
public:
    char name[NAME_LENGTH];
    std::string alt_name;
    char bitmap[MAX_FILENAME_LEN];
    char debrief_bitmap[MAX_FILENAME_LEN];
    int num_versions;
    bool version_starts_at_1;
    bool available_from_start;
    int kills_needed;

    // If this is a badge (kills_needed > 0)
    char voice_base[MAX_FILENAME_LEN];
    std::map< int, std::string > promotion_text;

    medal_stuff ();

    const char* get_display_string ();
};

extern std::vector< medal_stuff > Medals;

extern void parse_medal_tbl ();

// modes for this screen
#define MM_NORMAL 0 // normal - run through the state code
#define MM_POPUP \
    1 // called from within some other tight loop (don't use gameseq_
      // functions)

// main medals screen
void medal_main_init (player* pl, int mode = MM_NORMAL);

// return 0 if the screen should close (used for MM_POPUP mode)
int medal_main_do ();
void medal_main_close ();

int medals_info_lookup (const char* name);

#endif // FREESPACE2_STATS_MEDALS_HH
