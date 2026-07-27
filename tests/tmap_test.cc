// Renderer oracle for the 3D poly path: pushes textured quads through
// g3_draw_poly / g3_draw_laser / g3_draw_bitmap (the modelinterp, weapon,
// and thruster-glow paths) and checks that pixels actually land in the
// offscreen buffer.  Run with SDL_VIDEODRIVER=dummy for headless use.
//
//   tmap_test <game-root>

#include <stdio.h>
#include <string.h>

#include <globalincs/pstypes.hh>
#include <cfile/cfile.hh>
#include <osapi/osapi.hh>
#include <graphics/2d.hh>
#include <graphics/grzbuffer.hh>
#include <render/3d.hh>
#include <bmpman/bmpman.hh>
#include <model/model.hh>
#include <io/key.hh>
#include <io/mouse.hh>
#include <io/timer.hh>
#include <physics/physics.hh>
#include <math/vecmat.hh>
#include <lighting/lighting.hh>

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

// count non-zero pixels in a rect, and report the most common non-zero index
static int fb_count_rect(int x0, int y0, int x1, int y1, int *common)
{
	int hist[256];
	int n = 0;

	memset(hist, 0, sizeof(hist));
	for (int y = y0; y < y1; y++) {
		for (int x = x0; x < x1; x++) {
			ubyte c = fb_at(x, y);
			if (c) {
				n++;
				hist[c]++;
			}
		}
	}
	if (common) {
		int best = 0;
		*common = 0;
		for (int i = 1; i < 256; i++) {
			if (hist[i] > best) {
				best = hist[i];
				*common = i;
			}
		}
	}
	return n;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "usage: tmap_test <game-root>\n");
		return 2;
	}

	char exe_path[CF_MAX_PATHNAME_LENGTH];
	snprintf(exe_path, sizeof(exe_path), "%s/x", argv[1]);
	if (cfile_init(exe_path)) {
		fprintf(stderr, "cfile_init failed\n");
		return 1;
	}

	timer_init();
	os_init((char *)"FreeSpace", (char *)"FreeSpace 2 - tmap test");
	key_init();
	mouse_init();

	gr_init(GR_640, GR_SOFTWARE, 8);

	// identity-ramp grayscale palette (index == intensity), entry 1
	// perturbed to dodge the palman zero-state checksum collision
	ubyte pal[768];
	for (int i = 0; i < 256; i++)
		pal[i * 3 + 0] = pal[i * 3 + 1] = pal[i * 3 + 2] = (ubyte)i;
	pal[3 + 1] = 0;
	pal[3 + 2] = 0;
	gr_set_palette((char *)"tmap_test", pal, 0);

	// 64x64 texture, all texels palette index 200
	static ubyte texdata[64 * 64];
	memset(texdata, 200, sizeof(texdata));
	int tex = bm_create(8, 64, 64, texdata);
	CHECK(tex >= 0);

	gr_reset_clip();
	gr_clear();
	gr_zbuffer_clear(1);

	g3_start_frame(1);

	vector view_pos = { 0.0f, 0.0f, 0.0f };
	g3_set_view_matrix(&view_pos, &vmd_identity_matrix, 0.75f);

	// --- 1: plain textured quad dead ahead (the modelinterp poly path) ---
	// quad at z=20, x -5..5 (left half of screen), y -5..5
	vector corners[4] = {
		{ -8.0f,  5.0f, 20.0f },
		{  2.0f,  5.0f, 20.0f },
		{  2.0f, -5.0f, 20.0f },
		{ -8.0f, -5.0f, 20.0f },
	};
	vertex v[4], *vlist[4];
	for (int i = 0; i < 4; i++) {
		g3_rotate_vertex(&v[i], &corners[i]);
		vlist[i] = &v[i];
	}
	v[0].u = 0.0f; v[0].v = 0.0f;
	v[1].u = 1.0f; v[1].v = 0.0f;
	v[2].u = 1.0f; v[2].v = 1.0f;
	v[3].u = 0.0f; v[3].v = 1.0f;
	// modelinterp-style lit poly: b = lighting ramp value
	for (int i = 0; i < 4; i++)
		v[i].b = 255;

	gr_set_bitmap(tex);
	int rc1 = g3_draw_poly(4, vlist, TMAP_FLAG_TEXTURED);
	printf("g3_draw_poly(TEXTURED) rc=%d\n", rc1);

	// --- 2: same quad shifted right, with modelinterp's full flag set ---
	vector corners2[4] = {
		{  4.0f,  5.0f, 20.0f },
		{ 14.0f,  5.0f, 20.0f },
		{ 14.0f, -5.0f, 20.0f },
		{  4.0f, -5.0f, 20.0f },
	};
	vertex v2[4], *vlist2[4];
	for (int i = 0; i < 4; i++) {
		g3_rotate_vertex(&v2[i], &corners2[i]);
		vlist2[i] = &v2[i];
		v2[i].b = 255;
	}
	v2[0].u = 0.0f; v2[0].v = 0.0f;
	v2[1].u = 1.0f; v2[1].v = 0.0f;
	v2[2].u = 1.0f; v2[2].v = 1.0f;
	v2[3].u = 0.0f; v2[3].v = 1.0f;

	gr_set_bitmap(tex);
	int rc2 = g3_draw_poly(4, vlist2, TMAP_FLAG_TEXTURED | TMAP_FLAG_CORRECT | TMAP_FLAG_RAMP | TMAP_FLAG_GOURAUD);
	printf("g3_draw_poly(TEXTURED|CORRECT|RAMP|GOURAUD) rc=%d\n", rc2);

	// --- 3: laser quad (the weapon path) ---
	vector lhead = { -6.0f,  9.0f, 25.0f };
	vector ltail = {  6.0f, 12.0f, 25.0f };
	gr_set_bitmap(tex);
	g3_draw_laser(&lhead, 1.5f, &ltail, 1.5f, TMAP_FLAG_TEXTURED | TMAP_FLAG_XPARENT);
	printf("g3_draw_laser drawn\n");

	// --- 4: control: sprite through the scaler path (known good in-game) ---
	vector spos = { 0.0f, -10.0f, 25.0f };
	vertex sv;
	g3_rotate_vertex(&sv, &spos);
	gr_set_bitmap(tex);
	int rc4 = g3_draw_bitmap(&sv, 0, 2.0f, TMAP_FLAG_TEXTURED);
	printf("g3_draw_bitmap rc=%d\n", rc4);

	g3_end_frame();

	// dump before checking so a failure still leaves the image
	FILE *out = fopen("tmap_test.ppm", "wb");
	if (out) {
		fprintf(out, "P5\n%d %d\n255\n", gr_screen.max_w, gr_screen.max_h);
		for (int y = 0; y < gr_screen.max_h; y++)
			fwrite((ubyte *)gr_screen.offscreen_buffer + y * gr_screen.rowsize, 1, gr_screen.max_w, out);
		fclose(out);
	}

	// screen: 640x480, view center (320,240)
	int common;

	// quad 1 spans left-center; sample its middle
	int n1 = fb_count_rect(180, 180, 300, 300, &common);
	printf("quad1 region: %d lit pixels, common index %d\n", n1, common);
	CHECK(n1 > 5000);
	CHECK(common == 200);

	// quad 2 right of center
	int n2 = fb_count_rect(360, 180, 470, 300, &common);
	printf("quad2 region: %d lit pixels, common index %d\n", n2, common);
	CHECK(n2 > 5000);

	// laser upper area
	int n3 = fb_count_rect(150, 60, 500, 180, &common);
	printf("laser region: %d lit pixels, common index %d\n", n3, common);
	CHECK(n3 > 200);

	// sprite lower center
	int n4 = fb_count_rect(280, 320, 360, 420, &common);
	printf("sprite region: %d lit pixels, common index %d\n", n4, common);
	CHECK(n4 > 500);

	// --- 5: a real POF through modelinterp, several orientations ---
	// artifacts show up as pixels far outside the model's screen footprint
	int mn = model_load((char *)"fighter01.pof", 0, NULL);
	printf("model_load(fighter01.pof) = %d\n", mn);
	if (mn >= 0) {
		polymodel *pm = model_get(mn);
		float rad = pm->rad;
		printf("model radius %.1f\n", rad);

		const char *names[7] = { "front", "quarter", "side", "top",
			"close", "edge", "near" };
		angles rots[7] = {
			{ 0.0f, 0.0f, 0.0f },
			{ 0.4f, 0.8f, 0.0f },
			{ 0.0f, 1.57f, 0.0f },
			{ 1.57f, 0.0f, 0.3f },
			{ 0.4f, 0.8f, 0.0f },		// close: clipped by screen edges
			{ 0.2f, 2.2f, 0.5f },		// edge: half off-screen
			{ 0.4f, 0.8f, 0.0f },		// near: crosses the near plane
		};
		// distance multiplier and x offset per case
		float dists[7] = { 2.5f, 2.5f, 2.5f, 2.5f, 0.9f, 2.0f, 0.4f };
		float xoffs[7] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.8f, 0.2f };

		for (int k = 0; k < 7; k++) {
			gr_reset_clip();
			gr_clear();
			gr_zbuffer_clear(1);

			g3_start_frame(1);
			g3_set_view_matrix(&view_pos, &vmd_identity_matrix, 0.75f);

			matrix orient;
			vm_angles_2_matrix(&orient, &rots[k]);
			vector mpos = { rad * xoffs[k], 0.0f, rad * dists[k] };

			model_set_detail_level(0);
			model_render(mn, &orient, &mpos, MR_NO_LIGHTING | MR_LOCK_DETAIL);

			g3_end_frame();

			char fname[64];
			snprintf(fname, sizeof(fname), "model_%s.ppm", names[k]);
			FILE *mout = fopen(fname, "wb");
			if (mout) {
				fprintf(mout, "P5\n%d %d\n255\n", gr_screen.max_w, gr_screen.max_h);
				for (int y = 0; y < gr_screen.max_h; y++)
					fwrite((ubyte *)gr_screen.offscreen_buffer + y * gr_screen.rowsize, 1, gr_screen.max_w, mout);
				fclose(mout);
			}

			int center = fb_count_rect(220, 140, 420, 340, NULL);
			int border = fb_count_rect(0, 0, 640, 20, NULL)
					   + fb_count_rect(0, 460, 640, 480, NULL)
					   + fb_count_rect(0, 20, 20, 460, NULL)
					   + fb_count_rect(620, 20, 640, 460, NULL);
			printf("model %-8s center %6d px, border %6d px\n",
				names[k], center, border);
			if (k < 4) {
				// distant model fills only the center; border pixels = shards
				CHECK(center > 1000);
				CHECK(border == 0);
			}
			// clipped cases (close/edge/near) are judged from the dumps
		}
	} else {
		CHECK(mn >= 0);
	}

	// --- 6: model WITH the lit tmapper path (the in-mission configuration) ---
	if (mn >= 0) {
		polymodel *pm = model_get(mn);
		float rad = pm->rad;

		gr_reset_clip();
		gr_clear();
		gr_zbuffer_clear(1);

		g3_start_frame(1);
		g3_set_view_matrix(&view_pos, &vmd_identity_matrix, 0.75f);

		light_reset();
		vector ldir = { -0.5f, -0.7f, 0.5f };
		vm_vec_normalize(&ldir);
		light_add_directional(&ldir, 1.0f, 1.0f, 1.0f, 1.0f);
		light_rotate_all();

		angles ra = { 0.4f, 0.8f, 0.0f };
		matrix orient;
		vm_angles_2_matrix(&orient, &ra);
		vector mpos = { 0.0f, 0.0f, rad * 2.5f };

		model_set_detail_level(0);
		model_render(mn, &orient, &mpos, MR_LOCK_DETAIL);

		g3_end_frame();

		FILE *mout = fopen("model_lit.ppm", "wb");
		if (mout) {
			fprintf(mout, "P5\n%d %d\n255\n", gr_screen.max_w, gr_screen.max_h);
			for (int y = 0; y < gr_screen.max_h; y++)
				fwrite((ubyte *)gr_screen.offscreen_buffer + y * gr_screen.rowsize, 1, gr_screen.max_w, mout);
			fclose(mout);
		}

		int center = fb_count_rect(220, 140, 420, 340, NULL);
		int border = fb_count_rect(0, 0, 640, 20, NULL)
				   + fb_count_rect(0, 460, 640, 480, NULL)
				   + fb_count_rect(0, 20, 20, 460, NULL)
				   + fb_count_rect(620, 20, 640, 460, NULL);
		printf("model lit      center %6d px, border %6d px%s\n",
			center, border, border ? "  <-- ARTIFACTS" : "");
		CHECK(center > 1000);
		CHECK(border == 0);
	}

	// --- 7: laser storm: bolts at many angles, some near the camera ---
	{
		gr_reset_clip();
		gr_clear();
		gr_zbuffer_clear(1);

		g3_start_frame(1);
		g3_set_view_matrix(&view_pos, &vmd_identity_matrix, 0.75f);

		gr_set_bitmap(tex);
		srand(12345);
		for (int k = 0; k < 60; k++) {
			float ang = (float)(k * 0.7);
			float z0 = 0.02f + (float)(rand() % 100) / 20.0f;	// some heads nearly at the eye
			vector head = { cosf(ang) * 2.0f, sinf(ang) * 2.0f, z0 };
			vector tail = { cosf(ang) * 6.0f, sinf(ang) * 6.0f, z0 + 30.0f };
			g3_draw_laser(&head, 0.6f, &tail, 0.4f, TMAP_FLAG_TEXTURED | TMAP_FLAG_XPARENT);
		}

		g3_end_frame();

		FILE *lout = fopen("laser_storm.ppm", "wb");
		if (lout) {
			fprintf(lout, "P5\n%d %d\n255\n", gr_screen.max_w, gr_screen.max_h);
			for (int y = 0; y < gr_screen.max_h; y++)
				fwrite((ubyte *)gr_screen.offscreen_buffer + y * gr_screen.rowsize, 1, gr_screen.max_w, lout);
			fclose(lout);
		}
		int total = fb_count_rect(0, 0, 640, 480, NULL);
		printf("laser storm: %d lit pixels (see laser_storm.ppm)\n", total);
	}

	gr_flip();
	os_cleanup();

	if (failures == 0)
		printf("tmap_test: all checks passed\n");
	return failures ? 1 : 0;
}
