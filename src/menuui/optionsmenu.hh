// -*- mode: c++; -*-

#ifndef FREESPACE2_MENUUI_OPTIONSMENU_HH
#define FREESPACE2_MENUUI_OPTIONSMENU_HH

#include <defs.hh>

#include <ui/ui.hh>

struct op_sliders {
    // base slider
    const char* filename;
    int x, y, xt, yt;
    int hotspot;
    int dot_w;
    int dots;

    // left and right buttons
    const char* left_filename;
    int left_mask, left_x, left_y;
    const char* right_filename;
    int right_mask, right_x, right_y;

    // slider control
    UI_DOT_SLIDER_NEW slider; // because we have a class inside this struct, we
                              // need the constructor below..

    op_sliders (
        const char* name, int x1, int y1, int xt1, int yt1, int h, int _dot_w,
        int _dots, const char* _left_filename, int _left_mask, int _left_x,
        int _left_y, const char* _right_filename, int _right_mask,
        int _right_x, int _right_y)
        : filename (name), x (x1), y (y1), xt (xt1), yt (yt1), hotspot (h),
          dot_w (_dot_w), dots (_dots), left_filename (_left_filename),
          left_mask (_left_mask), left_x (_left_x), left_y (_left_y),
          right_filename (_right_filename), right_mask (_right_mask),
          right_x (_right_x), right_y (_right_y) {}
};

void options_menu_init ();
void options_menu_close ();
void options_menu_do_frame (float frametime);

// kill the options menu
void options_cancel_exit ();

#endif // FREESPACE2_MENUUI_OPTIONSMENU_HH
