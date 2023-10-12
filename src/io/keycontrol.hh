// -*- mode: c++; -*-

#ifndef FREESPACE2_IO_KEYCONTROL_HH
#define FREESPACE2_IO_KEYCONTROL_HH

#include <defs.hh>

#include <controlconfig/controlsconfig.hh>

// Holds the bit arrays that indicate which action is to be executed.
#define NUM_BUTTON_FIELDS ((CCFG_MAX + 31) / 32)

extern int Dead_key_set[];
extern int Dead_key_set_size;
extern bool Perspective_locked;

extern int Ignored_keys[];

struct button_info  {
    int status[NUM_BUTTON_FIELDS];
};

void button_info_set (button_info* bi, int n);
void button_info_unset (button_info* bi, int n);
int button_info_query (button_info* bi, int n);
void button_info_do (button_info* bi);
void button_info_clear (button_info* bi);
void process_set_of_keys (int key, int count, int* list);
void game_process_pause_key ();
void button_strip_noncritical_keys (button_info* bi);

#endif // FREESPACE2_IO_KEYCONTROL_HH
