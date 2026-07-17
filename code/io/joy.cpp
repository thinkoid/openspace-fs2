/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

// Joystick input on SDL2.  Retail polled winmm from a thread; here os_poll()
// pumps joy_process(), which tracks per-button down/up counts and down time
// the way the retail poller did.  Axis scaling/calibration is retail's.
// Shape follows the fs2open 2004 Linux port (joy-unix.cpp).

#include "pstypes.h"
#include "joy.h"
#include "fix.h"
#include "timer.h"
#include "osregistry.h"

#include "SDL.h"

static int Joy_inited = 0;
int joy_num_sticks = 0;
int Dead_zone_size = 10;
int Cur_joystick = -1;
int Joy_sensitivity = 9;

int joy_pollrate = 1000 / 18;		// poll at 18Hz

typedef struct joy_button_info {
	int	actual_state;		// Set if the button is physically down
	int	state;				// Set when the button goes from up to down, cleared on down to up.  Different than actual_state after a flush.
	int	down_count;
	int	up_count;
	int	down_time;
	uint	last_down_check;	// timestamp in milliseconds of last check
} joy_button_info;

Joy_info joystick;

static SDL_Joystick *sdljoy = NULL;

static joy_button_info joy_buttons[JOY_TOTAL_BUTTONS];

// --------------------------------------------------------------
//	joy_flush()
//
// Clear the state of the joystick.
//
void joy_flush()
{
	int			i;
	joy_button_info	*bi;

	if ( joy_num_sticks < 1 ) return;

	for ( i = 0; i < JOY_TOTAL_BUTTONS; i++) {
		bi = &joy_buttons[i];
		bi->state		= 0;
		bi->down_count	= 0;
		bi->up_count	= 0;
		bi->down_time	= 0;
		bi->last_down_check = timer_get_milliseconds();
	}
}

// --------------------------------------------------------------
//	joy_get_caps()
//
// Log the sticks SDL sees and mark the valid axes of the current one.
//
void joy_get_caps(int max)
{
	SDL_Joystick *joy;
	int j;

	for (j = 0; j < JOY_NUM_AXES; j++)
		joystick.axis_valid[j] = 0;

	for (j = 0; j < max; j++) {
		joy = SDL_JoystickOpen(j);
		if (joy) {
			nprintf(("JOYSTICK", "Joystick #%d: %s\n", j + 1, SDL_JoystickNameForIndex(j)));
			if (j == Cur_joystick) {
				for (int i = 0; i < SDL_JoystickNumAxes(joy) && i < JOY_NUM_AXES; i++) {
					joystick.axis_valid[i] = 1;
				}
			}
			SDL_JoystickClose(joy);
		}
	}
}

// --------------------------------------------------------------
//	joy_init()
//
// Initialize the joystick system.  This is called once at game startup.
//
int joy_init()
{
	int i, n;

	if (Joy_inited)
		return 0;

	if ( SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0 ) {
		mprintf(("Could not initialize joystick subsystem: %s\n", SDL_GetError()));
		return 0;
	}

	Joy_inited = 1;
	n = SDL_NumJoysticks();

	Cur_joystick = os_config_read_uint(NULL, "CurrentJoystick", 0);

	joy_get_caps(n);

	if (n < 1) {
		mprintf(("No joysticks found\n"));
		return 0;
	}

	sdljoy = SDL_JoystickOpen(Cur_joystick);
	if (sdljoy == NULL) {
		mprintf(("Unable to init joystick %d: %s\n", Cur_joystick, SDL_GetError()));
		return 0;
	}

	mprintf(("Using joystick #%d: %s (%d axes, %d buttons, %d hats)\n",
		Cur_joystick + 1, SDL_JoystickName(sdljoy),
		SDL_JoystickNumAxes(sdljoy), SDL_JoystickNumButtons(sdljoy),
		SDL_JoystickNumHats(sdljoy)));

	joy_num_sticks = n;

	joy_flush();

	// Neutral calibration over the SDL axis range (0..65535 after biasing)
	for (i = 0; i < JOY_NUM_AXES; i++) {
		joystick.axis_min[i] = 0;
		joystick.axis_center[i] = 32768;
		joystick.axis_max[i] = 65536;
	}

	return joy_num_sticks;
}

// --------------------------------------------------------------
//	joy_process()
//
// Called from the os_poll() event pump: refresh SDL joystick state and
// update the per-button bookkeeping the retail polling thread maintained.
//
void joy_process()
{
	int i;
	static uint last_ms = 0;
	uint now;
	int time_delta;

	if (!Joy_inited)
		return;
	if (sdljoy == NULL)
		return;

	now = timer_get_milliseconds();
	time_delta = (last_ms == 0) ? joy_pollrate : (int)(now - last_ms);
	last_ms = now;

	SDL_JoystickUpdate();

	int buttons = SDL_JoystickNumButtons(sdljoy);
	int hat = (SDL_JoystickNumHats(sdljoy) > 0) ? SDL_JoystickGetHat(sdljoy, 0) : SDL_HAT_CENTERED;

	for (i = 0; i < JOY_TOTAL_BUTTONS; i++) {
		int state = 0;

		if (i < JOY_NUM_BUTTONS) {
			if (i < buttons) {
				state = SDL_JoystickGetButton(sdljoy, i);
			}
		} else {
			switch (i) {
				case JOY_HATBACK:
					state = (hat & SDL_HAT_DOWN) ? 1 : 0;
					break;
				case JOY_HATFORWARD:
					state = (hat & SDL_HAT_UP) ? 1 : 0;
					break;
				case JOY_HATLEFT:
					state = (hat & SDL_HAT_LEFT) ? 1 : 0;
					break;
				case JOY_HATRIGHT:
					state = (hat & SDL_HAT_RIGHT) ? 1 : 0;
					break;
				default:
					break;
			}
		}

		if (state != joy_buttons[i].actual_state) {
			// Button position physically changed.
			joy_buttons[i].actual_state = state;

			if ( state )	{
				// went from up to down
				joy_buttons[i].down_count++;
				joy_buttons[i].down_time = 0;

				joy_buttons[i].state = 1;
			} else {
				// went from down to up
				if ( joy_buttons[i].state )	{
					joy_buttons[i].up_count++;
				}

				joy_buttons[i].state = 0;
			}
		} else {
			// Didn't move... increment time down if down.
			if (joy_buttons[i].state) {
				joy_buttons[i].down_time += time_delta;
			}
		}
	}
}

// --------------------------------------------------------------
//	joy_cheap_cal()
//
//	Manual calibrate joystick routine
//
void joy_cheap_cal()
{
	// SDL axes arrive pre-ranged; the neutral calibration from joy_init() stands
}

// --------------------------------------------------------------
//	joy_get_pos()
//
//	input:	x		=>		OUTPUT PARAMETER: x-axis position of stick (-1 to 1)
//				y		=>		OUTPUT PARAMETER: y-axis position of stick (-1 to 1)
//				z		=>		OUTPUT PARAMETER: z-axis (throttle) position of stick (0 to 1)
//				r		=>		OUTPUT PARAMETER: rudder position of stick (-1 to 1)
//
//	return:	success	=> 1
//				failure	=> 0
//
int joy_get_pos(int *x, int *y, int *z, int *r)
{
	int axis[JOY_NUM_AXES];

	if (x) *x = 0;
	if (y) *y = 0;
	if (z) *z = 0;
	if (r) *r = 0;

	if (joy_num_sticks < 1) return 0;

	joystick_read_raw_axis( JOY_NUM_AXES, axis );

	//	joy_get_scaled_reading will return a value represents the joystick pos from -1 to +1
	if (x && joystick.axis_valid[0])
		*x = joy_get_scaled_reading(axis[0], 0);
	if (y && joystick.axis_valid[1])
		*y = joy_get_scaled_reading(axis[1], 1);
	if (z && joystick.axis_valid[2])
		*z = joy_get_unscaled_reading(axis[2], 2);
	if (r && joystick.axis_valid[3])
		*r = joy_get_scaled_reading(axis[3], 3);

	return 1;
}

// --------------------------------------------------------------
//	joy_down_count()
//
// Return the number of times the button went down since
// joy_down_count() was last called
//
int joy_down_count(int btn, int reset_count)
{
	int tmp;

	if ( joy_num_sticks < 1 ) return 0;
	if ( (btn < 0) || (btn >= JOY_TOTAL_BUTTONS) ) return 0;

	tmp = joy_buttons[btn].down_count;
	if ( reset_count ) {
		joy_buttons[btn].down_count = 0;
	}

	return tmp;
}

// --------------------------------------------------------------
//	joy_down()
//
// Return the state of button number 'btn'
//
int joy_down(int btn)
{
	if ( joy_num_sticks < 1 ) return 0;
	if ( (btn < 0) || (btn >= JOY_TOTAL_BUTTONS) ) return 0;

	return joy_buttons[btn].state;
}

// --------------------------------------------------------------
//	joy_up_count()
//
// Return the number of times the button went up since
// joy_up_count() was last called
//
int joy_up_count(int btn)
{
	int tmp;

	if ( joy_num_sticks < 1 ) return 0;
	if ( (btn < 0) || (btn >= JOY_TOTAL_BUTTONS) ) return 0;

	tmp = joy_buttons[btn].up_count;
	joy_buttons[btn].up_count = 0;

	return tmp;
}

// --------------------------------------------------------------
//	joy_down_time()
//
// Return a number between 0 and 1.  This number represents the
// percentage of time that the button has been down since the last call
//
float joy_down_time(int btn)
{
	float			rval;
	unsigned int	now;
	joy_button_info	*bi;

	if ( joy_num_sticks < 1 ) return 0.0f;
	if ( (btn < 0) || (btn >= JOY_TOTAL_BUTTONS) ) return 0.0f;
	bi = &joy_buttons[btn];

	now = timer_get_milliseconds();

	if ( bi->down_time == 0 && joy_down(btn) ) {
		bi->down_time += joy_pollrate;
	}

	if ( (now - bi->last_down_check) > 0)
		rval = i2fl(bi->down_time) / (now - bi->last_down_check);
	else
		rval = 0.0f;

	bi->down_time = 0;
	bi->last_down_check = now;

	if (rval < 0)
		rval = 0.0f;
	if (rval > 1)
		rval = 1.0f;

	return rval;
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
//	Read the raw axis information.  SDL axes are -32768..32767; bias into
// the retail 0..65535 space the calibration works in.  Axes the stick
// doesn't have read as centered.
//
int joystick_read_raw_axis(int num_axes, int *axis)
{
	int i, num;

	Assert(num_axes <= JOY_NUM_AXES);

	if (sdljoy == NULL) {
		for (i=0; i<num_axes; i++)
			axis[i] = 32768;
		return 0;
	}

	num = SDL_JoystickNumAxes(sdljoy);

	for (i = 0; i < num_axes; i++) {
		if (i < num) {
			axis[i] = (int)SDL_JoystickGetAxis(sdljoy, i) + 32768;
		} else {
			axis[i] = 32768;
		}
	}

	return 1;
}

// --------------------------------------------------------------
//	joy_set_ul()
//
void joy_set_ul()
{
	joystick_read_raw_axis( 2, joystick.axis_min );
}

// --------------------------------------------------------------
//	joy_set_lr()
//
void joy_set_lr()
{
	joystick_read_raw_axis( 2, joystick.axis_max );
}

// --------------------------------------------------------------
//	joy_set_cen()
//
void joy_set_cen()
{
	joystick_read_raw_axis( 2, joystick.axis_center );
}

int joy_get_unscaled_reading(int raw, int axn)
{
	int rng;

	// Make sure it's calibrated properly.
	if (joystick.axis_center[axn] - joystick.axis_min[axn] < 5)
		return 0;

	if (joystick.axis_max[axn] - joystick.axis_center[axn] < 5)
		return 0;

	rng = joystick.axis_max[axn] - joystick.axis_min[axn];
	raw -= joystick.axis_min[axn];  // adjust for linear range starting at 0

	// cap at limits
	if (raw < 0)
		raw = 0;
	if (raw > rng)
		raw = rng;

	return (int) ((unsigned int) raw * (unsigned int) F1_0 / (unsigned int) rng);  // convert to 0 - F1_0 range.
}

// --------------------------------------------------------------
//	joy_get_scaled_reading()
//
//	input:	raw	=>	the raw value for an axis position
//				axn	=>	axis number, numbered starting at 0
//
// return:	joy_get_scaled_reading will return a value that represents
//				the joystick pos from -1 to +1 for the specified axis number 'axn', and
//				the raw value 'raw'
//
int joy_get_scaled_reading(int raw, int axn)
{
	int x, d, dead_zone, rng;
	float percent, sensitivity_percent, non_sensitivity_percent;

	// Make sure it's calibrated properly.
	if (joystick.axis_center[axn] - joystick.axis_min[axn] < 5)
		return 0;

	if (joystick.axis_max[axn] - joystick.axis_center[axn] < 5)
		return 0;

	raw -= joystick.axis_center[axn];

	dead_zone = (joystick.axis_max[axn] - joystick.axis_min[axn]) * Dead_zone_size / 100;

	if (raw < -dead_zone) {
		rng = joystick.axis_center[axn] - joystick.axis_min[axn] - dead_zone;
		d = -raw - dead_zone;

	} else if (raw > dead_zone) {
		rng = joystick.axis_max[axn] - joystick.axis_center[axn] - dead_zone;
		d = raw - dead_zone;

	} else
		return 0;

	if (d > rng)
		d = rng;

	Assert(Joy_sensitivity >= 0 && Joy_sensitivity <= 9);

	// compute percentages as a range between 0 and 1
	sensitivity_percent = (float) Joy_sensitivity / 9.0f;
	non_sensitivity_percent = (float) (9 - Joy_sensitivity) / 9.0f;

	// find percent of max axis is at
	percent = (float) d / (float) rng;

	// work sensitivity on axis value
	percent = (percent * sensitivity_percent + percent * percent * percent * percent * percent * non_sensitivity_percent);

	x = (int) ((float) F1_0 * percent);

	if (raw < 0)
		return -x;

	return x;
}

void joy_get_delta(int *dx, int *dy)
{
	if (dx) *dx = 0;
	if (dy) *dy = 0;
}

void joy_close()
{
	if (!Joy_inited)
		return;

	Joy_inited = 0;
	joy_num_sticks = 0;

	if (sdljoy)
		SDL_JoystickClose(sdljoy);
	sdljoy = NULL;

	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

// ----------------------------------------------------------------------
// force feedback: no devices yet; the whole surface is a polite no-op
// (SDL2 haptics can back these later)
// ----------------------------------------------------------------------

#include "joy_ff.h"

int joy_ff_init() { return 0; }
void joy_ff_shutdown() {}
void joy_ff_stop_effects() {}
void joy_ff_mission_init(vector /*v*/) {}
void joy_reacquire_ff() {}
void joy_unacquire_ff() {}
void joy_ff_play_vector_effect(vector * /*v*/, float /*scaler*/) {}
void joy_ff_play_dir_effect(float /*x*/, float /*y*/) {}
void joy_ff_play_primary_shoot(int /*gain*/) {}
void joy_ff_play_secondary_shoot(int /*gain*/) {}
void joy_ff_adjust_handling(int /*speed*/) {}
void joy_ff_docked() {}
void joy_ff_play_reload_effect() {}
void joy_ff_afterburn_on() {}
void joy_ff_afterburn_off() {}
void joy_ff_explode() {}
void joy_ff_fly_by(int /*mag*/) {}
void joy_ff_deathroll() {}
