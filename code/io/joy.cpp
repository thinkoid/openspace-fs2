/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

#include "pstypes.h"
#include "joy.h"

// No joystick support on this platform yet: joy_init() reports zero sticks
// and everything else is a well-behaved no-op over that state.  Real SDL
// joystick support comes later.

static int Joy_inited = 0;
int joy_num_sticks = 0;
int Dead_zone_size = 10;
int Joy_sensitivity = 9;

Joy_info joystick;

// --------------------------------------------------------------
//	joy_flush()
//
// Clear the state of the joystick.
//
void joy_flush()
{
}

// --------------------------------------------------------------
//	joy_init()
//
// Initialize the joystick system.  This is called once at game startup.
//
int joy_init()
{
	int i;

	if (Joy_inited)
		return 0;

	Joy_inited = 1;
	joy_num_sticks = 0;

	// Fake a neutral calibration so joy_get_cal_vals() hands out sane values
	for (i = 0; i < JOY_NUM_AXES; i++) {
		joystick.axis_valid[i] = 0;
		joystick.axis_min[i] = 0;
		joystick.axis_center[i] = 32768;
		joystick.axis_max[i] = 65536;
	}

	mprintf(("No joystick support on this platform yet, found 0 joysticks\n"));

	return joy_num_sticks;
}

// --------------------------------------------------------------
//	joy_cheap_cal()
//
//	Manual calibrate joystick routine
//
void joy_cheap_cal()
{
}

// --------------------------------------------------------------
//	joy_get_pos()
//
// Get the position of the joystick axes.  Returns 0 (no stick).
//
int joy_get_pos(int *x, int *y, int *z, int *r)
{
	if (x) *x = 0;
	if (y) *y = 0;
	if (z) *z = 0;
	if (r) *r = 0;

	return 0;
}

// --------------------------------------------------------------
//	joy_down_count()
//
// Return the number of times the button went down since
// joy_down_count() was last called
//
int joy_down_count(int btn, int reset_count)
{
	return 0;
}

// --------------------------------------------------------------
//	joy_down()
//
// Return the state of button number 'btn'
//
int joy_down(int btn)
{
	return 0;
}

// --------------------------------------------------------------
//	joy_up_count()
//
// Return the number of times the button went up since
// joy_up_count() was last called
//
int joy_up_count(int btn)
{
	return 0;
}

// --------------------------------------------------------------
//	joy_down_time()
//
// Return a number between 0 and 1.  This number represents the
// percentage of time that the button has been down since the last call
//
float joy_down_time(int btn)
{
	return 0.0f;
}

// --------------------------------------------------------------
//	joy_get_cal_vals()
//
//	Get the calibrated min, center, and max for all axes
//
void joy_get_cal_vals(int *axis_min, int *axis_center, int *axis_max)
{
	int i;

	for ( i = 0; i < 4; i++)		{
		axis_min[i] = joystick.axis_min[i];
		axis_center[i] = joystick.axis_center[i];
		axis_max[i] = joystick.axis_max[i];
	}
}

// --------------------------------------------------------------
//	joy_set_cal_vals()
//
//	Set the calibrated min, center, and max for all axes
//
void joy_set_cal_vals(int *axis_min, int *axis_center, int *axis_max)
{
	int i;

	for (i=0; i<4; i++)		{
		joystick.axis_min[i] = axis_min[i];
		joystick.axis_center[i] = axis_center[i];
		joystick.axis_max[i] = axis_max[i];
	}
}

// --------------------------------------------------------------
//	joystick_read_raw_axis()
//
//	Read the raw axis information.  With no sticks, centers every
// requested axis and reports failure like the retail no-stick path.
//
int joystick_read_raw_axis(int num_axes, int *axis)
{
	int i;

	Assert(num_axes <= JOY_NUM_AXES);
	for (i=0; i<num_axes; i++)
		axis[i] = 32768;

	return 0;
}

// --------------------------------------------------------------
//	joy_set_ul()
//
void joy_set_ul()
{
}

// --------------------------------------------------------------
//	joy_set_lr()
//
void joy_set_lr()
{
}

// --------------------------------------------------------------
//	joy_set_cen()
//
void joy_set_cen()
{
}

int joy_get_unscaled_reading(int raw, int axn)
{
	return 0;
}

// --------------------------------------------------------------
//	joy_get_scaled_reading()
//
int joy_get_scaled_reading(int raw, int axn)
{
	return 0;
}

void joy_get_delta(int *dx, int *dy)
{
	if (dx) *dx = 0;
	if (dy) *dy = 0;
}

void joy_close()
{
	// no joysticks to release yet
}
