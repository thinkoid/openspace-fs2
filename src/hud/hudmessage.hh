// -*- mode: c++; -*-

#ifndef FREESPACE2_HUD_HUDMESSAGE_HH
#define FREESPACE2_HUD_HUDMESSAGE_HH

#include <defs.hh>

#include <anim/packunpack.hh>
#include <graphics/generic.hh>
#include <hud/hud.hh>

#include <queue>

#define MAX_HUD_LINE_LEN 256 // maximum number of characters for a HUD message

#define HUD_SOURCE_COMPUTER 0
#define HUD_SOURCE_TRAINING 1
#define HUD_SOURCE_HIDDEN 2
#define HUD_SOURCE_IMPORTANT 3
#define HUD_SOURCE_FAILED 4
#define HUD_SOURCE_SATISFIED 5
#define HUD_SOURCE_TERRAN_CMD 6
#define HUD_SOURCE_NETPLAYER 7

#define HUD_SOURCE_TEAM_OFFSET 8 // must be higher than any previous hud source

struct HUD_message_data  {
    std::string text;
    int source; // where this message came from so we can color code it
    int x;
};

struct line_node  {
    line_node* next;
    line_node* prev;
    fix time;   // timestamp when message was added
    int source; // who/what the source of the message was (for color coding)
    int x;
    int y;
    int underline_width;
    char* text;
};

extern line_node Msg_scrollback_used_list;

struct Hud_display_info  {
    HUD_message_data msg;
    int y; // y Coordinate to draw message at
    int target_y;
    int total_life; // timestamp id to control how long a HUD message stays
                    // alive
};

void hud_scrollback_init ();
void hud_scrollback_close ();
void hud_scrollback_do_frame (float frametime);
void hud_scrollback_exit ();

void hud_init_msg_window ();
void hud_clear_msg_buffer ();
int HUD_team_get_source (int team);
int HUD_source_get_team (int team);
void HUD_printf (const char* format, ...)
    __attribute__ ((format (printf, 1, 2)));
void hud_sourced_print (int source, const char* msg);
void HUD_sourced_printf (int source, const char* format, ...)
    __attribute__ ((format (printf, 2, 3))); // send hud message from specified source
void HUD_ship_sent_printf (int sh, const char* format, ...)
    __attribute__ ((format (printf, 2, 3))); // send hud message from a specific ship
void HUD_fixed_printf (float duration, color col, const char* format, ...)
    __attribute__ ((format (printf, 3, 4))); // Display a single message for duration seconds.
void HUD_init_fixed_text (); // Clear all pending fixed text.

void HUD_add_to_scrollback (const char* text, int source);
void hud_add_line_to_scrollback (
    const char* text, int source, int t, int x, int y, int w);
void hud_add_msg_to_scrollback (const char* text, int source, int t);
void hud_free_scrollback_list ();

class HudGaugeMessages : public HudGauge // HUD_MESSAGE_LINES
{
protected:
    // User-defined properties
    int Max_lines;
    int Max_width; // 620 for GR_640 and 1004 for GR_1024
    int Scroll_time;
    int Step_size;
    int Total_life;
    int Line_h;
    bool Hidden_by_comms_menu;

    int Window_width;
    int Window_height;

    std::vector< Hud_display_info > active_messages;
    std::queue< HUD_message_data > pending_messages;

    bool Scroll_needed;
    bool Scroll_in_progress;
    int Scroll_time_id;

public:
    HudGaugeMessages ();

    void initLineHeight (int h);
    void initMaxLines (int lines);
    void initMaxWidth (int width);
    void initScrollTime (int ms);
    void initStepSize (int h);
    void initTotalLife (int ms);
    void initHiddenByCommsMenu (bool hide);

    void clearMessages ();
    void processMessageBuffer ();
    void addPending (const char* text, int source, int x = 0);
    void scrollMessages ();
    void preprocess () override;
    void render (float frametime) override;
    void initialize () override;
    void pageIn () override;
};

class HudGaugeTalkingHead : public HudGauge // HUD_TALKING_HEAD
{
    hud_frames Head_frame;

    int Header_offsets[2];
    int Anim_offsets[2];
    int Anim_size[2];

    generic_anim* head_anim;

    int msg_id;

public:
    HudGaugeTalkingHead ();
    void initBitmaps (const char* fname);
    void initHeaderOffsets (int x, int y);
    void initAnimOffsets (int x, int y);
    void initAnimSizes (int w, int h);
    void pageIn () override;
    void render (float frametime) override;
    void initialize () override;
    bool canRender () override;
    anim_instance* createAnim (int anim_start_frame, anim* anim_data);
};

class HudGaugeFixedMessages : public HudGauge {
    bool center_text;

public:
    HudGaugeFixedMessages ();
    void initCenterText (bool center);
    void render (float frametime) override;
    void pageIn () override;
};

#endif // FREESPACE2_HUD_HUDMESSAGE_HH
