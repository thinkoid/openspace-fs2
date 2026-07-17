// Point-4 milestone: os + gr on SDL2, software renderer, first pixels.
//
//   first_pixels <game-root> [seconds]
//
// Inits the platform layer and the 8bpp software renderer, draws test
// content through the gr_* API, self-checks the offscreen buffer, dumps
// it as first_pixels.ppm, and presents the window for [seconds] (default 3;
// 0 = no wait, useful with SDL_VIDEODRIVER=dummy).

#include <stdio.h>
#include <string.h>

#include "pstypes.h"
#include "cfile.h"
#include "osapi.h"
#include "2d.h"
#include "key.h"
#include "mouse.h"
#include "timer.h"

static int failures = 0;

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
		failures++; \
	} \
} while (0)

static ubyte fb_at(int x, int y)
{
	return *((ubyte *)gr_screen.offscreen_buffer + y * gr_screen.rowsize + x);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "usage: first_pixels <game-root> [seconds]\n");
		return 2;
	}
	int seconds = (argc > 2) ? atoi(argv[2]) : 3;

	char exe_path[CF_MAX_PATHNAME_LENGTH];
	snprintf(exe_path, sizeof(exe_path), "%s/x", argv[1]);
	if (cfile_init(exe_path)) {
		fprintf(stderr, "cfile_init failed\n");
		return 1;
	}

	timer_init();
	os_init((char *)"FreeSpace", (char *)"FreeSpace 2 - first pixels");
	key_init();
	mouse_init();

	gr_init(GR_640, GR_SOFTWARE, 8);

	// identity-ramp grayscale palette: index i -> (i,i,i).  Entry 1 is
	// perturbed: the pure ramp's Fletcher checksum collides with palman's
	// initial zero state (sum = 3*32640 = 255*384), which would make
	// palette_update early-return without ever clearing the lookup cache.
	ubyte pal[768];
	for (int i = 0; i < 256; i++)
		pal[i * 3 + 0] = pal[i * 3 + 1] = pal[i * 3 + 2] = (ubyte)i;
	pal[3 + 1] = 0;
	pal[3 + 2] = 0;
	gr_set_palette((char *)"first_pixels", pal, 0);

	// draw: clear, gradient of vertical lines, a rect, a circle
	gr_reset_clip();
	gr_clear();

	for (int x = 0; x < 256; x++) {
		gr_set_color(x, x, x);
		gr_line(x + 50, 100, x + 50, 200);
	}
	gr_set_color(255, 255, 255);
	gr_rect(400, 100, 100, 100);
	gr_circle(320, 350, 80);

	// self-checks straight from the offscreen buffer (grayscale identity
	// palette makes index == intensity)
	// the ramp makes index == intensity; palette policy keeps the top
	// indexes special (255 reserved, nondarkening slots), so "bright"
	// rather than exactly 255
	CHECK(fb_at(10, 10) == 0);			// cleared
	CHECK(fb_at(305, 150) >= 250);		// bright end of the gradient
	CHECK(fb_at(55, 150) < 16);			// dark end of the gradient
	CHECK(fb_at(450, 150) >= 250);		// inside the rect
	CHECK(fb_at(320, 350) >= 250);		// center of the filled circle

	// dump the raw 8bpp buffer as a grayscale PPM for eyeball/tool review
	FILE *out = fopen("first_pixels.ppm", "wb");
	if (out) {
		fprintf(out, "P5\n%d %d\n255\n", gr_screen.max_w, gr_screen.max_h);
		for (int y = 0; y < gr_screen.max_h; y++)
			fwrite((ubyte *)gr_screen.offscreen_buffer + y * gr_screen.rowsize, 1, gr_screen.max_w, out);
		fclose(out);
	}

	gr_flip();

	// hold the frame so a human can see it
	int frames = seconds * 60;
	for (int i = 0; i < frames; i++) {
		os_poll();
		gr_flip();
		os_sleep(16);
	}

	os_cleanup();

	if (failures == 0)
		printf("first_pixels: all checks passed\n");
	return failures ? 1 : 0;
}
