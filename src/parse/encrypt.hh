// -*- mode: c++; -*-

#ifndef FREESPACE2_PARSE_ENCRYPT_HH
#define FREESPACE2_PARSE_ENCRYPT_HH

#include <defs.hh>

// initialize encryption
void encrypt_init ();

// Return 1 if the file is encrypted, otherwise return 0
int is_encrypted (char* scrambled_text);

// Returns 1 if the data uses one of the FS1 style encryptions, 0 if FS2 style
int is_old_encrypt (char* scrambled_text);

// return text description of the encrypted text type
const char* encrypt_type (char* scrambled_text);

// Encrypt text data
void encrypt (
    char* text, int text_len, char* scrambled_text, int* scrambled_len,
    int use_8bit, bool new_encrypt = true);

// Decrypt scrambled_text
void unencrypt (
    char* scrambled_text, int scrambled_len, char* text, int* text_len);

#endif // FREESPACE2_PARSE_ENCRYPT_HH
