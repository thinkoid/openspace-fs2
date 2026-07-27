/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/

#ifndef _PARSELO_H
#define _PARSELO_H

#include <setjmp.h>
#include <cfile/cfile.hh>

#define MISSION_TEXT_SIZE 390000

extern char Mission_text[MISSION_TEXT_SIZE];
extern char Mission_text_raw[MISSION_TEXT_SIZE];
extern char *Mp;
extern char *token_found;
extern int fred_parse_flag;
extern int Token_found_flag;
extern jmp_buf parse_abort;

#define COMMENT_CHAR (char)';'
#define EOF_CHAR (char)-128
#define EOLN (char)0x0a

#define F_NAME 1
#define F_DATE 2
#define F_NOTES 3
#define F_FILESPEC 4
// needed for backwards compatability with old briefing format
#define F_MULTITEXTOLD 5
#define F_SEXP 6
#define F_PATHNAME 7
#define F_SHIPCHOICE 8
// this is now obsolete for mission messages - all messages in missions should now use $MessageNew and stuff strings as F_MULTITEXT
#define F_MESSAGE 9
#define F_MULTITEXT 10

#define PATHNAME_LENGTH 192
#define NAME_LENGTH 32
#define SEXP_LENGTH 128
#define DATE_LENGTH 32
#define TIME_LENGTH 16
#define DATE_TIME_LENGTH 48
#define NOTES_LENGTH 1024
#define MULTITEXT_LENGTH 1024
#define FILESPEC_LENGTH 64
#define MESSAGE_LENGTH 512

// used to identify which kind of array to do a search for a name in
#define SHIP_TYPE 0
#define SHIP_INFO_TYPE 1
// to parse an int_list of weapons
#define WEAPON_LIST_TYPE 2
// to parse a list of integers
#define RAW_INTEGER_TYPE 3
#define WEAPON_POOL_TYPE 4

#define SEXP_SAVE_MODE 1
#define SEXP_ERROR_CHECK_MODE 2

// white space
extern int is_white_space(char ch);
extern void ignore_white_space();
extern void drop_trailing_white_space(char *str);
extern void drop_leading_white_space(char *str);
extern char *drop_white_space(char *str);

// gray space
void ignore_gray_space();

// error
extern int get_line_num();
extern char *next_tokens();
extern void diag_printf(char *format, ...);
extern void error_display(int error_level, char *format, ...);

// skip
extern int skip_to_string(char *pstr, char *end = NULL);
extern int skip_to_start_of_strings(char *pstr1, char *pstr2);
extern void advance_to_eoln(char *terminators);
extern void skip_token();

// required
extern int required_string(char *pstr);
extern int optional_string(char *pstr);
extern int required_string_either(char *str1, char *str2);
extern int required_string_3(char *str1, char *str2, char *str3);

// stuff
extern void copy_to_eoln(char *outstr, char *more_terminators, char *instr,
                         int max);
extern void copy_text_until(char *outstr, char *instr, char *endstr,
                            int max_chars);
extern void stuff_string_white(char *pstr);
extern void stuff_string(char *pstr, int type, char *terminators, int len = 0);
extern void stuff_string_line(char *pstr, int len);

// Exactly the same as stuff string only Malloc's the buffer.
// Supports various FreeSpace primitive types.  If 'len' is supplied, it will override
// the default string length if using the F_NAME case.
char *stuff_and_malloc_string(int type, char *terminators, int len);
extern void stuff_float(float *f);
extern void stuff_int(int *i);
extern void stuff_byte(ubyte *i);
extern int stuff_string_list(char slp[][NAME_LENGTH], int max_strings);
extern int stuff_int_list(int *ilp, int max_ints, int lookup_type);
extern int stuff_vector_list(vector *vlp, int max_vecs);
extern void stuff_vector(vector *vp);
extern void stuff_matrix(matrix *mp);
extern int string_lookup(char *str1, char *strlist[], int max,
                         char *description = NULL, int say_errors = 0);
extern void find_and_stuff(char *id, int *addr, int f_type, char *strlist[],
                           int max, char *description);
extern int match_and_stuff(int f_type, char *strlist[], int max,
                           char *description);
extern void find_and_stuff_or_add(char *id, int *addr, int f_type,
                                  char *strlist[], int *total, int max,
                                  char *description);
extern int get_string(char *str);
extern void stuff_parenthesized_vector(vector *vp);
void stuff_boolean(int *i);
int check_for_string(char *pstr);
int check_for_string_raw(char *pstr);

// general
extern void init_parse();
extern void reset_parse();
extern void display_parse_diagnostics();
extern void parse_main();

// utility
extern void mark_int_list(int *ilp, int max_ints, int lookup_type);
extern void compact_multitext_string(char *str);
extern void read_file_text(char *filename, int mode = CF_TYPE_ANY);
extern void debug_show_mission_text();
extern void convert_sexp_to_string(int cur_node, char *outstr, int mode);
char *split_str_once(char *src, int max_pixel_w);
int split_str(char *src, int max_pixel_w, int *n_chars, char **p_str,
              int max_lines, char ignore_char = -1);

// fred
extern int required_string_fred(char *pstr, char *end = NULL);
extern int required_string_either_fred(char *str1, char *str2);
extern int optional_string_fred(char *pstr, char *end = NULL, char *end2 = NULL);
#endif
