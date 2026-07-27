/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#include <hud/hudbrackets.hh>
#include <hud/hud.hh>
#include <playerman/player.hh>
#include <hud/hudtarget.hh>
#include <render/3d.hh>
#include <debris/debris.hh>
#include <ship/ai.hh>
#include <freespace2/freespace.hh>
#include <bmpman/bmpman.hh>
#include <globalincs/linklist.hh>
#include <weapon/emp.hh>

// how much the bounding brackets get faded
#define FADE_FACTOR 2
// lowest r value for bounding bracket
#define LOWEST_RED 50
// lowest g value for bounding bracket
#define LOWEST_GREEN 50
// lowest b value for bounding bracket
#define LOWEST_BLUE 50

char Ships_attack_fname[GR_NUM_RESOLUTIONS][MAX_FILENAME_LEN] = { "attacker",
                                                                  "attacker" };

// resolution adjust factors for the above defines
int Min_target_box_width[GR_NUM_RESOLUTIONS] = { 20, 30 };
int Min_target_box_height[GR_NUM_RESOLUTIONS] = { 20, 30 };
int Min_subtarget_box_width[GR_NUM_RESOLUTIONS] = { 12, 24 };
int Min_subtarget_box_height[GR_NUM_RESOLUTIONS] = { 12, 24 };

void
hud_init_brackets()
{ }

// find IFF color index for brackets based on team
int
hud_brackets_get_iff_color(int team)
{
    int color = IFF_COLOR_FRIENDLY;

    switch (team) {
    case TEAM_FRIENDLY:
    case TEAM_HOSTILE:
    case TEAM_NEUTRAL:
    case TEAM_TRAITOR:
    case TEAM_UNKNOWN:
        if ((team == Player_ship->team) && (team != TEAM_TRAITOR)) {
            color = IFF_COLOR_FRIENDLY;
        }
        else {
            switch (team) {
            case TEAM_NEUTRAL:
                color = IFF_COLOR_NEUTRAL;
                break;
            case TEAM_UNKNOWN:
                color = IFF_COLOR_UNKNOWN;
                break;
            case TEAM_HOSTILE:
            case TEAM_FRIENDLY:
            case TEAM_TRAITOR:
                color = IFF_COLOR_HOSTILE;
                break;
            }
        }
        break;
    case SELECTION_SET:
        color = IFF_COLOR_SELECTION;
        break;

    case MESSAGE_SENDER:
        color = IFF_COLOR_MESSAGE;
        break;

    default:
        color = IFF_COLOR_UNKNOWN;
        Int3();
    } // end switch

    return color;
}

// Called by draw_bounding_brackets.
void
draw_brackets_square(int x1, int y1, int x2, int y2)
{
    int width, height;

    width = x2 - x1;
    Assert(width > 0);
    height = y2 - y1;
    Assert(height > 0);

    // make the brackets extend 25% of the way along the width or height
    int bracket_width = width / 4;
    int bracket_height = height / 4;

    // horizontal lines
    if ((x1 + bracket_width > 0) && (x1 < gr_screen.clip_width)) {
        gr_gradient(x1, y1, x1 + bracket_width - 1, y1); // top left
        gr_gradient(x1, y2, x1 + bracket_width - 1, y2); // bottom left
    }

    if ((x2 - bracket_width < gr_screen.clip_width) && (x2 > 0)) {
        gr_gradient(x2, y1, x2 - bracket_width + 1, y1); // top right
        gr_gradient(x2, y2, x2 - bracket_width + 1, y2); // bottom right
    }

    // vertical lines
    if ((y1 + bracket_height > 0) && (y1 < gr_screen.clip_height)) {
        gr_gradient(x1, y1, x1, y1 + bracket_height - 1); // top left
        gr_gradient(x2, y1, x2, y1 + bracket_height - 1); // top right
    }

    if ((y2 - bracket_height < gr_screen.clip_height) && (y2 > 0)) {
        gr_gradient(x1, y2, x1, y2 - bracket_height + 1); // bottom left
        gr_gradient(x2, y2, x2, y2 - bracket_height + 1); // bottom right
    }
}

void
draw_brackets_square_quick(int x1, int y1, int x2, int y2, int thick)
{
    int width, height;

    width = x2 - x1;
    height = y2 - y1;

    // make the brackets extend 25% of the way along the width or height
    int bracket_width = width / 4;
    int bracket_height = height / 4;

    // top line
    gr_line(x1, y1, x1 + bracket_width, y1);
    gr_line(x2, y1, x2 - bracket_width, y1);
    if (thick) {
        gr_line(x1, y1 + 1, x1 + bracket_width, y1 + 1);
        gr_line(x2, y1 + 1, x2 - bracket_width, y1 + 1);
    }

    // bottom line
    gr_line(x1, y2, x1 + bracket_width, y2);
    gr_line(x2, y2, x2 - bracket_width, y2);
    if (thick) {
        gr_line(x1, y2 - 1, x1 + bracket_width, y2 - 1);
        gr_line(x2, y2 - 1, x2 - bracket_width, y2 - 1);
    }

    // left line
    if (thick) {
        gr_line(x1, y1 + 2, x1, y1 + bracket_height);
        gr_line(x1, y2 - 2, x1, y2 - bracket_height);
        gr_line(x1 + 1, y1 + 2, x1 + 1, y1 + bracket_height);
        gr_line(x1 + 1, y2 - 2, x1 + 1, y2 - bracket_height);
    }
    else {
        gr_line(x1, y1 + 1, x1, y1 + bracket_height);
        gr_line(x1, y2 - 1, x1, y2 - bracket_height);
    }

    // right line
    if (thick) {
        gr_line(x2, y1 + 2, x2, y1 + bracket_height);
        gr_line(x2, y2 - 2, x2, y2 - bracket_height);
        gr_line(x2 - 1, y1 + 2, x2 - 1, y1 + bracket_height);
        gr_line(x2 - 1, y2 - 2, x2 - 1, y2 - bracket_height);
    }
    else {
        gr_line(x2, y1 + 1, x2, y1 + bracket_height);
        gr_line(x2, y2 - 1, x2, y2 - bracket_height);
    }
}

#define NUM_DASHES 2
void
draw_brackets_dashed_square_quick(int x1, int y1, int x2, int y2)
{
    int width, height, i;

    width = x2 - x1;
    height = y2 - y1;

    // make the brackets extend 25% of the way along the width or height
    float bracket_width = width / 4.0f;
    float bracket_height = height / 4.0f;

    int dash_width;
    dash_width = fl2i(bracket_width / (NUM_DASHES * 2 - 1) + 0.5f);

    if (dash_width < 1) {
        draw_brackets_square_quick(x1, y1, x2, y2);
        return;
    }

    int dash_height;
    dash_height = fl2i(bracket_height / (NUM_DASHES * 2 - 1) + 0.5f);

    if (dash_height < 1) {
        draw_brackets_square_quick(x1, y1, x2, y2);
        return;
    }

    int dash_x1, dash_x2, dash_y1, dash_y2;

    dash_x1 = x1;
    dash_x2 = x2;
    dash_y1 = y1;
    dash_y2 = y2;

    for (i = 0; i < NUM_DASHES; i++) {
        // top line
        gr_line(dash_x1, y1, dash_x1 + (dash_width - 1), y1);
        gr_line(dash_x2, y1, dash_x2 - (dash_width - 1), y1);

        // bottom line
        gr_line(dash_x1, y2, dash_x1 + (dash_width - 1), y2);
        gr_line(dash_x2, y2, dash_x2 - (dash_width - 1), y2);

        dash_x1 += dash_width * 2;
        dash_x2 -= dash_width * 2;

        // left line
        gr_line(x1, dash_y1, x1, dash_y1 + (dash_height - 1));
        gr_line(x1, dash_y2, x1, dash_y2 - (dash_height - 1));

        // right line
        gr_line(x2, dash_y1, x2, dash_y1 + (dash_height - 1));
        gr_line(x2, dash_y2, x2, dash_y2 - (dash_height - 1));

        dash_y1 += dash_height * 2;
        dash_y2 -= dash_height * 2;
    }
}

// draw_brackets_diamond()
// Called by draw_bounding_brackets.

void
draw_brackets_diamond(int x1, int y1, int x2, int y2)
{
    int width, height, half_width, half_height;
    int center_x, center_y;
    int x_delta, y_delta;

    float side_len, bracket_len;

    width = x2 - x1;
    height = y2 - y1;

    half_width = fl2i(width / 2.0f + 0.5f);
    half_height = fl2i(height / 2.0f + 0.5f);

    side_len = (float)_hypot(half_width, half_height);
    bracket_len = side_len / 8;

    x_delta = fl2i(bracket_len * width / side_len + 0.5f);
    y_delta = fl2i(bracket_len * height / side_len + 0.5f);

    center_x = x1 + half_width;
    center_y = y1 + half_height;

    // top left line
    gr_gradient(center_x - x_delta, y1 + y_delta, center_x, y1);
    gr_gradient(x1 + x_delta, center_y - y_delta, x1, center_y);

    // top right line
    gr_gradient(center_x + x_delta, y1 + y_delta, center_x, y1);
    gr_gradient(x2 - x_delta, center_y - y_delta, x2, center_y);

    // bottom left line
    gr_gradient(x1 + x_delta, center_y + y_delta, x1, center_y);
    gr_gradient(center_x - x_delta, y2 - y_delta, center_x, y2);

    // bottom right line
    gr_gradient(x2 - x_delta, center_y + y_delta, x2, center_y);
    gr_gradient(center_x + x_delta, y2 - y_delta, center_x, y2);
}

void
draw_brackets_diamond_quick(int x1, int y1, int x2, int y2, int thick)
{
    int width, height, half_width, half_height;
    int center_x, center_y;
    int x_delta, y_delta;

    float side_len, bracket_len;

    width = x2 - x1;
    height = y2 - y1;

    half_width = fl2i(width / 2.0f + 0.5f);
    half_height = fl2i(height / 2.0f + 0.5f);

    side_len = (float)_hypot(half_width, half_height);
    bracket_len = side_len / 8;

    x_delta = fl2i(bracket_len * width / side_len + 0.5f);
    y_delta = fl2i(bracket_len * height / side_len + 0.5f);

    center_x = x1 + half_width;
    center_y = y1 + half_height;

    // top left line
    gr_line(center_x - x_delta, y1 + y_delta, center_x, y1);
    gr_line(x1 + x_delta, center_y - y_delta, x1, center_y);

    // top right line
    gr_line(center_x + x_delta, y1 + y_delta, center_x, y1);
    gr_line(x2 - x_delta, center_y - y_delta, x2, center_y);

    // bottom left line
    gr_line(x1 + x_delta, center_y + y_delta, x1, center_y);
    gr_line(center_x - x_delta, y2 - y_delta, center_x, y2);

    // bottom right line
    gr_line(x2 - x_delta, center_y + y_delta, x2, center_y);
    gr_line(center_x + x_delta, y2 - y_delta, center_x, y2);

    // draw an 'X' in the middle of the brackets
    gr_line(center_x - x_delta, center_y - y_delta, center_x + x_delta,
            center_y + y_delta);
    gr_line(center_x - x_delta, center_y + y_delta, center_x + x_delta,
            center_y - y_delta);
}

int
subsys_is_fighterbay(ship_subsys *ss)
{
    if (!strnicmp(NOX("fighter"), ss->system_info->name, 7)) {
        return 1;
    }

    return 0;
}

// Draw bounding brackets for a subobject.
void
draw_bounding_brackets_subobject()
{
    if (Player_ai->targeted_subsys_parent == Player_ai->target_objnum)
        if (Player_ai->targeted_subsys != NULL) {
            ship_subsys *subsys;
            int target_objnum;
            object *targetp;
            vertex subobj_vertex;
            vector subobj_pos;
            int x1, x2, y1, y2;

            subsys = Player_ai->targeted_subsys;
            target_objnum = Player_ai->target_objnum;
            Assert(target_objnum != -1);
            targetp = &Objects[target_objnum];
            Assert(targetp->type == OBJ_SHIP);

            get_subsystem_world_pos(targetp, subsys, &subobj_pos);

            g3_rotate_vertex(&subobj_vertex, &subobj_pos);

            g3_project_vertex(&subobj_vertex);
            if (subobj_vertex.flags &
                PF_OVERFLOW) // if overflow, no point in drawing brackets
                return;

            int subobj_x = fl2i(subobj_vertex.sx + 0.5f);
            int subobj_y = fl2i(subobj_vertex.sy + 0.5f);
            int hud_subtarget_w, hud_subtarget_h, bound_rc;

            bound_rc = subobj_find_2d_bound(subsys->system_info->radius,
                                            &targetp->orient, &subobj_pos, &x1,
                                            &y1, &x2, &y2);
            if (bound_rc != 0)
                return;

            hud_subtarget_w = x2 - x1 + 1;
            if (hud_subtarget_w > gr_screen.clip_width) {
                hud_subtarget_w = gr_screen.clip_width;
            }

            hud_subtarget_h = y2 - y1 + 1;
            if (hud_subtarget_h > gr_screen.clip_height) {
                hud_subtarget_h = gr_screen.clip_height;
            }

            if (hud_subtarget_w > gr_screen.max_w) {
                x1 = subobj_x - (gr_screen.max_w >> 1);
                x2 = subobj_x + (gr_screen.max_w >> 1);
            }
            if (hud_subtarget_h > gr_screen.max_h) {
                y1 = subobj_y - (gr_screen.max_h >> 1);
                y2 = subobj_y + (gr_screen.max_h >> 1);
            }

            if (hud_subtarget_w < Min_subtarget_box_width[gr_screen.res]) {
                x1 = subobj_x - (Min_subtarget_box_width[gr_screen.res] >> 1);
                x2 = subobj_x + (Min_subtarget_box_width[gr_screen.res] >> 1);
            }
            if (hud_subtarget_h < Min_subtarget_box_height[gr_screen.res]) {
                y1 = subobj_y - (Min_subtarget_box_height[gr_screen.res] >> 1);
                y2 = subobj_y + (Min_subtarget_box_height[gr_screen.res] >> 1);
            }

            // determine if subsystem is on far or near side of the ship
            Player->subsys_in_view = ship_subsystem_in_sight(
                targetp, subsys, &View_position, &subobj_pos, 0);

            // AL 29-3-98: If subsystem is destroyed, draw gray brackets
            if ((Player_ai->targeted_subsys->current_hits <= 0) &&
                (!subsys_is_fighterbay(Player_ai->targeted_subsys))) {
                gr_set_color_fast(&IFF_colors[IFF_COLOR_MESSAGE][1]);
            }
            else {
                hud_set_iff_color(targetp, 1);
            }

            if (Player->subsys_in_view) {
                draw_brackets_square_quick(x1, y1, x2, y2);
            }
            else {
                draw_brackets_diamond_quick(x1, y1, x2, y2);
            }
            // mprintf(("Drawing subobject brackets at %4i, %4i\n", sx, sy));
        }
}

extern int HUD_drew_selection_bracket_on_target;

// Display the current target distance, right justified at (x,y)
void
hud_target_show_dist_on_bracket(int x, int y, float distance)
{
    char text_dist[64];
    int w, h;

    if (y < 0 || y > gr_screen.clip_height) {
        return;
    }

    if (x < 0 || x > gr_screen.clip_width) {
        return;
    }

    sprintf(text_dist, "%d", fl2i(distance + 0.5f));
    hud_num_make_mono(text_dist);
    gr_get_string_size(&w, &h, text_dist);

    int y_delta = 4;
    if (HUD_drew_selection_bracket_on_target) {
        y += 4;
    }

    gr_string(x - w + 2, y + y_delta, text_dist);
}

// !!!!!!!!!!!!!!!
// Given an object number, return the number of ships attacking it.
// MWA 5/26/98 -- copied from aicode num_attacking_ships()!!!
// !!!!!!!!!!!!!!!
int
hud_bracket_num_ships_attacking(int objnum)
{
    object *objp;
    ship_obj *so;
    int count = 0;

    for (so = GET_FIRST(&Ship_obj_list); so != END_OF_LIST(&Ship_obj_list);
         so = GET_NEXT(so)) {
        objp = &Objects[so->objnum];
        if (objp->instance != -1) {
            ai_info *aip;
            aip = &Ai_info[Ships[objp->instance].ai_index];

            // don't count instructor
            int is_training_mission();
            if (is_training_mission() &&
                stricmp(Ships[objp->instance].ship_name, "Instructor") == 0) {
                break;
            }

            if ((aip->mode == AIM_CHASE) && (aip->target_objnum == objnum))
                if (Ships[objp->instance].team !=
                    Ships[Objects[objnum].instance].team)
                    count++;
        }
    }

    return count;
}

int Ships_attacking_bitmap = -1;

// draw_bounding_brackets() will draw the faded brackets that surround the current target
void
draw_bounding_brackets(int x1, int y1, int x2, int y2, int w_correction,
                       int h_correction, float distance, int target_objnum)
{
    int width, height;

    if ((x1 < 0 && x2 < 0) || (y1 < 0 && y2 < 0))
        return;

    if ((x1 > gr_screen.clip_width && x2 > gr_screen.clip_width) ||
        (y1 > gr_screen.clip_height && y2 > gr_screen.clip_height))
        return;

    width = x2 - x1;
    Assert(width >= 0);

    height = y2 - y1;
    Assert(height >= 0);

    if ((width > (gr_screen.max_w - 1)) && (height > (gr_screen.max_h - 1))) {
        return;
    }
    if (width > 1200) {
        return;
    }
    if (height > 1200) {
        return;
    }

    if (width < Min_target_box_width[gr_screen.res]) {
        x1 = x1 - (Min_target_box_width[gr_screen.res] - width) / 2;
        x2 = x2 + (Min_target_box_width[gr_screen.res] - width) / 2;
    }

    if (height < Min_target_box_height[gr_screen.res]) {
        y1 = y1 - (Min_target_box_height[gr_screen.res] - height) / 2;
        y2 = y2 + (Min_target_box_height[gr_screen.res] - height) / 2;
    }

    draw_brackets_square(x1 - w_correction, y1 - h_correction, x2 + w_correction,
                         y2 + h_correction);

    // draw distance to target in lower right corner of box
    if (distance > 0) {
        hud_target_show_dist_on_bracket(x2 + w_correction, y2 + h_correction,
                                        distance);
    }

    //   Maybe show + for each additional fighter or bomber attacking target.
    if ((target_objnum != -1) && hud_gauge_active(HUD_ATTACKING_TARGET_COUNT)) {
        int num_attacking = hud_bracket_num_ships_attacking(target_objnum);

        if (Ships_attacking_bitmap == -1) {
            Ships_attacking_bitmap = bm_load(Ships_attack_fname[gr_screen.res]);
        }

        if (Ships_attacking_bitmap == -1) {
            Int3();
            return;
        }

        //  If a ship not on player's team, show one fewer plus since it is targeted and attacked by player.
        int k = 0;
        if (Objects[target_objnum].type == OBJ_SHIP) {
            if (Ships[Objects[target_objnum].instance].team !=
                Player_ship->team) {
                k = 1;
            }
        }
        else {
            k = 1;
        }

        if (num_attacking > k) {
            int i, num_blips;

            num_blips = num_attacking - k;
            if (num_blips > 4) {
                num_blips = 4;
            }

            //int bitmap = get_blip_bitmap();

            if (Ships_attacking_bitmap > -1) {
                if (num_blips > 3)
                    y1 -= 3;

                for (i = 0; i < num_blips; i++) {
                    GR_AABITMAP(Ships_attacking_bitmap, x2 + 3, y1 + i * 7);
                }
            }
        }
    }
}
