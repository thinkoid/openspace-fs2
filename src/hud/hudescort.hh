// -*- mode: c++; -*-

#ifndef FREESPACE2_HUD_HUDESCORT_HH
#define FREESPACE2_HUD_HUDESCORT_HH

#include <defs.hh>

#include <hud/hud.hh>

// Odd def for escort frames
#define NUM_ESCORT_FRAMES 3

extern int Max_escort_ships;

class object;

void hud_escort_update_list ();
void hud_escort_init ();
void hud_setup_escort_list (int level = 1);
void hud_escort_view_toggle ();
void hud_add_remove_ship_escort (int objnum, int supress_feedback = 0);
void hud_escort_clear_all (bool clear_flags = false);
void hud_escort_ship_hit (object* objp, int quadrant);
void hud_escort_target_next ();
void hud_escort_cull_list ();
void hud_add_ship_to_escort (int objnum, int supress_feedback);
void hud_remove_ship_from_escort (int objnum);
int hud_escort_num_ships_on_list ();
int hud_escort_return_objnum (int index);

class HudGaugeEscort : public HudGauge {
protected:
    hud_frames Escort_gauges[NUM_ESCORT_FRAMES];

    int header_text_offsets[2];    // coordinates of the header text
    char header_text[NAME_LENGTH]; // Header text for the Escort Gauge. Default
                                   // is "monitoring"
    int list_start_offsets[2];     // Offset Start of the Ship List
    int entry_h;                   // the height of each entry
    int entry_stagger_w;           // width of the staircase effect
    int bottom_bg_offset;
    int ship_name_offsets[2];      // Offset of the Ship Name column
    int ship_integrity_offsets[2]; // Offset of the Ship Hull column
    int ship_status_offsets[2];    // Offset of the Ship Status column
    int ship_name_max_width;       // max width of ship name entries
    bool right_align_names;        // whether or not to right-align ship names
public:
    HudGaugeEscort ();
    void initBitmaps (char* fname_top, char* fname_middle, char* fname_bottom);
    void initHeaderText (char* text);
    void initHeaderTextOffsets (int x, int y);
    void initListStartOffsets (int x, int y);
    void initEntryHeight (int h);
    void initEntryStaggerWidth (int w);
    void initBottomBgOffset (int offset);
    void initShipNameOffsets (int x, int y);
    void initShipIntegrityOffsets (int x, int y);
    void initShipStatusOffsets (int x, int y);
    void initShipNameMaxWidth (int w);
    void initRightAlignNames (bool align);
    int setGaugeColorEscort (int index, int team);
    void render (float frametime) override;
    void pageIn () override;
    void renderIcon (int x, int y, int index);
};

#endif // FREESPACE2_HUD_HUDESCORT_HH
