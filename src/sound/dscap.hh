// -*- mode: c++; -*-

#ifndef FREESPACE2_SOUND_DSCAP_HH
#define FREESPACE2_SOUND_DSCAP_HH

#include <defs.hh>

int dscap_init ();
void dscap_close ();
int dscap_supported ();
int dscap_create_buffer (
    int freq, int bits_per_sample, int nchannels, int nseconds);
void dscap_release_buffer ();

int dscap_start_record ();
int dscap_stop_record ();
int dscap_max_buffersize ();
int dscap_get_raw_data (unsigned char* outbuf, unsigned int max_size);

#endif // FREESPACE2_SOUND_DSCAP_HH
