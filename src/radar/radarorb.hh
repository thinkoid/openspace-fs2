// -*- mode: c++; -*-

#ifndef FREESPACE2_RADAR_RADARORB_HH
#define FREESPACE2_RADAR_RADARORB_HH

#include <defs.hh>

#include <radar/radarsetup.hh>

class object;
struct blip;
struct color;
struct vec3d;

#define NUM_ORB_RING_SLICES 16

class HudGaugeRadarOrb : public HudGaugeRadar {
    char Radar_fname[MAX_FILENAME_LEN];
    hud_frames Radar_gauge;

    vec3d target_position;

    vec3d orb_ring_yz[NUM_ORB_RING_SLICES];
    vec3d orb_ring_xy[NUM_ORB_RING_SLICES];
    vec3d orb_ring_xz[NUM_ORB_RING_SLICES];

    color Orb_color_orange;
    color Orb_color_teal;
    color Orb_color_purple;
    color Orb_crosshairs;

    float Radar_center_offsets[2];

public:
    HudGaugeRadarOrb ();
    void initBitmaps (char* fname);
    void initCenterOffsets (float x, float y);

    void loadDefaultPositions ();
    void blipDrawDistorted (blip* b, vec3d* pos);
    void blipDrawFlicker (blip* b, vec3d* pos);
    void blitGauge ();
    void drawBlips (int blip_type, int bright, int distort);
    void drawBlipsSorted (int distort);
    void drawContact (vec3d* pnt, int rad);
    void drawContactHtl (vec3d* pnt, int rad);
    void
    drawContactImage (vec3d* pnt, int rad, int idx, int clr_idx, float mult);
    void drawCrosshairs (vec3d pnt);
    void doneDrawing ();
    void doneDrawingHtl ();
    void drawOutlines ();
    void drawOutlinesHtl ();
    void setupView ();
    void setupViewHtl ();
    int calcAlpha (vec3d* pt);
    void render (float frametime) override;
    void pageIn () override;
    void plotBlip (blip* b, vec3d* scaled_pos);
};

#endif // FREESPACE2_RADAR_RADARORB_HH
