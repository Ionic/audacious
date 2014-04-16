/*
 * equalizer.h
 * Copyright 2014 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#ifndef LIBAUDCORE_EQUALIZER_H
#define LIBAUDCORE_EQUALIZER_H

#include <libaudcore/index.h>
#include <libaudcore/vfs.h>

#define AUD_EQ_NBANDS 10
#define AUD_EQ_MAX_GAIN 12

typedef struct {
    char * name;
    float preamp;
    float bands[AUD_EQ_NBANDS];
} EqualizerPreset;

void aud_eq_set_bands (const double * values);
void aud_eq_get_bands (double * values);
void aud_eq_set_band (int band, double value);
double aud_eq_get_band (int band);

EqualizerPreset * aud_eq_preset_new (const char * name);
void aud_eq_preset_free (EqualizerPreset * preset);

Index * aud_eq_read_presets (const char * basename);
bool_t aud_eq_write_presets (Index * list, const char * basename);

/* note: legacy code! these are local filenames, not URIs */
EqualizerPreset * aud_load_preset_file (const char * filename);
bool_t aud_save_preset_file (EqualizerPreset * preset, const char * filename);

Index * aud_import_winamp_presets (VFSFile * file);
bool_t aud_export_winamp_preset (EqualizerPreset * preset, VFSFile * file);

/* TODO: make these private */
void eq_set_format (int new_channels, int new_rate);
void eq_filter (float * data, int samples);

#endif /* LIBAUDCORE_EQUALIZER_H */