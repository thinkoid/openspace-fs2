/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmdline/cmdline.hh>

int Cmdline_freespace_no_sound = 0;
int Cmdline_freespace_no_music = 0;
int Cmdline_gimme_all_medals = 0;
int Cmdline_use_last_pilot = 0;
int Cmdline_cd_check = 1;
int Cmdline_spew_pof_info = 0;
int Cmdline_mouse_coords = 0;

int Cmdline_window = 0;
int Cmdline_opengl = 0;
int Cmdline_res_w = 0; // framebuffer size override; 0 = retail size
int Cmdline_res_h = 0;

// the command line options: one row per flag, help text alongside.
// getopt_long_only accepts both -name and --name spellings.
struct cmdline_opt
{
    const char *name;
    int *flag; // set to 1 when the option is seen (flag options only)
    char arg; // getopt val for argument-taking options; 0 for plain flags
    const char *help;
};

static cmdline_opt Cmdline_opts[] = {
    { "nosound", &Cmdline_freespace_no_sound, 0, "run without sound" },
    { "nomusic", &Cmdline_freespace_no_music, 0, "run without music" },
    { "pofspew", &Cmdline_spew_pof_info, 0,
      "dump info on all POF models, then exit" },
    { "coords", &Cmdline_mouse_coords, 0, "print mouse coordinates on click" },
    { "window", &Cmdline_window, 0, "run in a window instead of fullscreen" },
    { "opengl", &Cmdline_opengl, 0,
      "use the OpenGL renderer (default: software)" },
    { "res", NULL, 'r',
      "framebuffer size WxH (default 1024x768), e.g. -res 2048x1536" },
    { "help", NULL, 0, "show this help and exit" },
};

#define NUM_CMDLINE_OPTS (int)(sizeof(Cmdline_opts) / sizeof(Cmdline_opts[0]))

static void
print_help()
{
    printf("usage: fs2 [options]\n\noptions:\n");
    for (int i = 0; i < NUM_CMDLINE_OPTS; i++) {
        char name[32];
        snprintf(name, sizeof(name), "%s%s", Cmdline_opts[i].name,
                 Cmdline_opts[i].arg ? " WxH" : "");
        printf("  -%-11s %s\n", name, Cmdline_opts[i].help);
    }
}

// data/cmdline.cfg may hold extra arguments (a single line); they are
// parsed before the real command line so the command line wins
static void
read_cmdline_cfg(int *argc, char **argv, int max_args)
{
    FILE *fp = fopen("data/cmdline.cfg", "rt");
    if (fp == NULL) {
        return;
    }

    static char buf[1024];
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        for (char *tok = strtok(buf, " \t\r\n"); tok != NULL;
             tok = strtok(NULL, " \t\r\n")) {
            if (*argc >= max_args) {
                break;
            }
            argv[(*argc)++] = tok;
        }
    }

    fclose(fp);
}

// external entry point into this module
int
parse_cmdline(int argc, char **argv)
{
    // merged argument vector: program name, cmdline.cfg tokens, real arguments
    char *merged[256];
    int n = 0;

    merged[n++] = argv[0];
    read_cmdline_cfg(&n, merged, (int)(sizeof(merged) / sizeof(merged[0])));
    for (int i = 1; i < argc && n < (int)(sizeof(merged) / sizeof(merged[0]));
         i++) {
        merged[n++] = argv[i];
    }

    struct option longopts[NUM_CMDLINE_OPTS + 1];
    for (int i = 0; i < NUM_CMDLINE_OPTS; i++) {
        longopts[i].name = Cmdline_opts[i].name;
        longopts[i].has_arg = Cmdline_opts[i].arg ? required_argument
                                                  : no_argument;
        longopts[i].flag = Cmdline_opts[i].flag;
        longopts[i].val = Cmdline_opts[i].flag
                              ? 1
                              : (Cmdline_opts[i].arg
                                     ? Cmdline_opts[i].arg
                                     : 'h'); // help has no flag variable
    }
    memset(&longopts[NUM_CMDLINE_OPTS], 0, sizeof(struct option));

    int c;
    while ((c = getopt_long_only(n, merged, "", longopts, NULL)) != -1) {
        switch (c) {
        case 0: // a table flag; getopt set the variable
            break;
        case 'r':
            if (sscanf(optarg, "%dx%d", &Cmdline_res_w, &Cmdline_res_h) != 2 ||
                Cmdline_res_w < 640 || Cmdline_res_h < 480) {
                fprintf(stderr, "bad -res `%s' (want WxH, at least 640x480)\n",
                        optarg);
                exit(1);
            }
            break;
        case 'h':
            print_help();
            exit(0);
        default: // getopt printed the complaint already
            fprintf(stderr, "try `fs2 --help'\n");
            exit(1);
        }
    }

    if (optind < n) {
        fprintf(stderr, "%s: unexpected argument `%s'\ntry `fs2 --help'\n",
                argv[0], merged[optind]);
        exit(1);
    }

    return 1;
}
