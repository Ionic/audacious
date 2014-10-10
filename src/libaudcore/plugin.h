/*
 * plugin.h
 * Copyright 2005-2013 William Pitcock, Yoshiki Yazawa, Eugene Zagidullin, and
 *                     John Lindgren
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

#ifndef LIBAUDCORE_PLUGIN_H
#define LIBAUDCORE_PLUGIN_H

#include <libaudcore/audio.h>
#include <libaudcore/plugins.h>
#include <libaudcore/tuple.h>
#include <libaudcore/vfs.h>

struct PluginPreferences;

/* "Magic" bytes identifying an Audacious plugin header. */
#define _AUD_PLUGIN_MAGIC ((int) 0x8EAC8DE2)

/* API version.  Plugins are marked with this number at compile time.
 *
 * _AUD_PLUGIN_VERSION is the current version; _AUD_PLUGIN_VERSION_MIN is
 * the oldest one we are backward compatible with.  Plugins marked older than
 * _AUD_PLUGIN_VERSION_MIN or newer than _AUD_PLUGIN_VERSION are not loaded.
 *
 * Before releases that add new pointers to the end of the API tables, increment
 * _AUD_PLUGIN_VERSION but leave _AUD_PLUGIN_VERSION_MIN the same.
 *
 * Before releases that break backward compatibility (e.g. remove pointers from
 * the API tables), increment _AUD_PLUGIN_VERSION *and* set
 * _AUD_PLUGIN_VERSION_MIN to the same value. */

#define _AUD_PLUGIN_VERSION_MIN 46 /* 3.6-devel */
#define _AUD_PLUGIN_VERSION     46 /* 3.6-devel */

/* A NOTE ON THREADS
 *
 * How thread-safe a plugin must be depends on the type of plugin.  Note that
 * some parts of the Audacious API are *not* thread-safe and therefore cannot be
 * used in some parts of some plugins; for example, input plugins cannot use
 * GUI-related calls or access the playlist except in about() and configure().
 *
 * Thread-safe plugins: transport, playlist, input, effect, and output.  These
 * must be mostly thread-safe.  init() and cleanup() may be called from
 * secondary threads; however, no other functions provided by the plugin will be
 * called at the same time.  about() and configure() will be called only from
 * the main thread.  All other functions provided by the plugin may be called
 * from any thread and from multiple threads simultaneously.
 *
 * Exceptions:
 * - Because many existing input plugins are not coded to handle simultaneous
 *   calls to play(), play() will only be called from one thread at a time.  New
 *   plugins should not rely on this exception, though.
 * - Some combinations of calls, especially for output and effect plugins, make
 *   no sense; for example, flush() in an output plugin will only be called
 *   after open_audio() and before close_audio().
 *
 * Single-thread plugins: visualization, general, and interface.  Functions
 * provided by these plugins will only be called from the main thread. */

/* CROSS-PLUGIN MESSAGES
 *
 * Since 3.2, Audacious implements a basic messaging system between plugins.
 * Messages are sent using aud_plugin_send_message() and received through the
 * take_message() method specified in the header of the receiving plugin.
 * Plugins that do not need to receive messages can set take_message() to nullptr.
 *
 * Each message includes a code indicating the type of message, a pointer to
 * some data, and a value indicating the size of that data. What the message
 * data contains is entirely up to the two plugins involved. For this reason, it
 * is crucial that both plugins agree on the meaning of the message codes used.
 *
 * Once the message is sent, an integer error code is returned. If the receiving
 * plugin does not provide the take_message() method, -1 is returned. If
 * take_message() does not recognize the message code, it should ignore the
 * message and return -1. An error code of zero represents success. Other error
 * codes may be used with more specific meanings.
 *
 * For the time being, aud_plugin_send_message() should only be called from the
 * program's main thread. */

// this enum is also in interface.h
#ifndef _AUD_VIS_TYPE_DEFINED
#define _AUD_VIS_TYPE_DEFINED
enum {
    AUD_VIS_TYPE_CLEAR,
    AUD_VIS_TYPE_MONO_PCM,
    AUD_VIS_TYPE_MULTI_PCM,
    AUD_VIS_TYPE_FREQ,
    AUD_VIS_TYPES
};
#endif

struct PluginInfo {
    const char * name;
    const char * domain; // for gettext
    const char * about;
    const PluginPreferences * prefs;
};

class Plugin
{
public:
    constexpr Plugin (int type, PluginInfo info) :
        type (type),
        info (info) {}

    const int magic = _AUD_PLUGIN_MAGIC;
    const int version = _AUD_PLUGIN_VERSION;

    const int type;  // see PLUGIN_TYPE_* enum
    const PluginInfo info;

    virtual bool init () { return true; }
    virtual void cleanup () {}

    virtual int take_message (const char * code, const void * data, int size) { return -1; }
};

class TransportPlugin : public Plugin
{
public:
    constexpr TransportPlugin (const PluginInfo info,
     const ArrayRef<const char *> schemes, VFSFile::OpenFunc fopen_impl) :
        Plugin (PLUGIN_TYPE_TRANSPORT, info),
        schemes (schemes),
        fopen_impl (fopen_impl) {}

    /* supported URI schemes (without "://") */
    const ArrayRef<const char *> schemes;

    /* fopen() implementation */
    const VFSFile::OpenFunc fopen_impl;
};

class PlaylistPlugin : public Plugin
{
public:
    constexpr PlaylistPlugin (const PluginInfo info,
     const ArrayRef<const char *> extensions, bool can_save) :
        Plugin (PLUGIN_TYPE_PLAYLIST, info),
        extensions (extensions),
        can_save (can_save) {}

    /* supported file extensions (without periods) */
    const ArrayRef<const char *> extensions;

    /* true if the plugin can save playlists */
    const bool can_save;

    /* path: URI of playlist file (in)
     * file: VFS handle of playlist file (in, read-only file, not seekable)
     * title: title of playlist (out)
     * items: playlist entries (out) */
    virtual bool load (const char * path, VFSFile & file, String & title,
     Index<PlaylistAddItem> & items) = 0;

    /* path: URI of playlist file (in)
     * file: VFS handle of playlist file (in, write-only file, not seekable)
     * title: title of playlist (in)
     * items: playlist entries (in) */
    virtual bool save (const char * path, VFSFile & file, const char * title,
     const Index<PlaylistAddItem> & items) { return false; }
};

class OutputPlugin : public Plugin
{
public:
    constexpr OutputPlugin (const PluginInfo info, int priority, bool force_reopen = false) :
        Plugin (PLUGIN_TYPE_OUTPUT, info),
        priority (priority),
        force_reopen (force_reopen) {}

    /* During probing, plugins with higher priority (10 to 0) are tried first. */
    const int priority;

    /* Whether close_audio() and open_audio() must always be called between
     * songs, even if the audio format is the same.  Note that this defeats
     * gapless playback. */
    const bool force_reopen;

    /* Returns current volume for left and right channels (0 to 100). */
    virtual StereoVolume get_volume () = 0;

    /* Changes volume for left and right channels (0 to 100). */
    virtual void set_volume (StereoVolume volume) = 0;

    /* Begins playback of a PCM stream.  <format> is one of the FMT_*
     * enumeration values defined in libaudcore/audio.h.  Returns true on
     * success. */
    virtual bool open_audio (int format, int rate, int chans) = 0;

    /* Ends playback.  Any buffered audio data is discarded. */
    virtual void close_audio () = 0;

    /* Returns how many bytes of data may be passed to a following write_audio()
     * call. */
    virtual int buffer_free () = 0;

    /* Waits until buffer_free() will return a size greater than zero.
     * output_time(), pause(), and flush() may be called meanwhile; if flush()
     * is called, period_wait() should return immediately. */
    virtual void period_wait () = 0;

    /* Buffers <size> bytes of data, in the format given to open_audio(). */
    virtual void write_audio (const void * data, int size) = 0;

    /* Waits until all buffered data has been heard by the user. */
    virtual void drain () = 0;

    /* Returns time count (in milliseconds) of how much data has been heard by
     * the user. */
    virtual int output_time () = 0;

    /* Pauses the stream if <p> is nonzero; otherwise unpauses it.
     * write_audio() will not be called while the stream is paused. */
    virtual void pause (bool pause) = 0;

    /* Discards any buffered audio data and sets the time counter (in
     * milliseconds) of data written. */
    virtual void flush (int time) = 0;
};

class EffectPlugin : public Plugin
{
public:
    constexpr EffectPlugin (const PluginInfo info, int order, bool preserves_format) :
        Plugin (PLUGIN_TYPE_EFFECT, info),
        order (order),
        preserves_format (preserves_format) {}

    /* Effects with lowest order (0 to 9) are applied first. */
    const int order;

    /* If the effect does not change the number of channels or the sampling
     * rate, it can be enabled and disabled more smoothly. */
    const bool preserves_format;

    /* All processing is done in floating point.  If the effect plugin wants to
     * change the channel count or sample rate, it can change the parameters
     * passed to start().  They cannot be changed in the middle of a song. */
    virtual void start (int & channels, int & rate) = 0;

    /* Performs effect processing.  process() may modify the audio samples in
     * place and return a reference to the same buffer, or it may return a
     * reference to an internal working buffer.  The number of output samples
     * need not be the same as the number of input samples. */
    virtual Index<float> & process (Index<float> & data) = 0;

    /* Optional.  A seek is taking place; any buffers should be discarded.
     * Unless the "force" flag is set, the plugin may choose to override the
     * normal flush behavior and handle the flush itself (for example, to
     * perform crossfading).  The flush() function should return false in this
     * case to prevent flush() from being called in downstream effect plugins. */
    virtual bool flush (bool force)
        { return true; }

    /* Exactly like process() except that any buffers should be drained (i.e.
     * the data processed and returned).  finish() will be called a second time
     * at the end of the last song in the playlist. */
    virtual Index<float> & finish (Index<float> & data, bool end_of_playlist)
        { return process (data); }

    /* Required only for plugins that change the time domain (e.g. a time
     * stretch) or use read-ahead buffering.  translate_delay() must do two
     * things: first, translate <delay> (which is in milliseconds) from the
     * output time domain back to the input time domain; second, increase
     * <delay> by the size of the read-ahead buffer.  It should return the
     * adjusted delay. */
    virtual int adjust_delay (int delay)
        { return delay; }
};

struct InputPluginInfo
{
    /* How quickly the plugin should be tried in searching for a plugin to
     * handle a file which could not be identified from its extension.  Plugins
     * with priority 0 are tried first, 10 last. */
    int priority;

    /* True if the files handled by the plugin may contain more than one song.
     * When reading the tuple for such a file, the plugin should set the
     * FIELD_SUBSONG_NUM field to the number of songs in the file.  For all
     * other files, the field should be left unset.
     *
     * Example:
     * 1. User adds a file named "somefile.xxx" to the playlist.  Having
     * determined that this plugin can handle the file, Audacious opens the file
     * and calls probe_for_tuple().  probe_for_tuple() sees that there are 3
     * songs in the file and sets FIELD_SUBSONG_NUM to 3.
     * 2. For each song in the file, Audacious opens the file and calls
     * probe_for_tuple() -- this time, however, a question mark and song number
     * are appended to the file name passed: "somefile.sid?2" refers to the
     * second song in the file "somefile.sid".
     * 3. When one of the songs is played, Audacious opens the file and calls
     * play() with a file name modified in this way.
     */
    bool has_subtunes;

    /* True if the plugin can write file tags. */
    bool can_write_tuple;

    /* File extensions associated with file types the plugin can handle. */
    ArrayRef<const char *> extensions;

    /* MIME types the plugin can handle. */
    ArrayRef<const char *> mimes;

    /* Custom URI schemes the plugin supports.  Plugins using custom URI
     * schemes are expected to handle their own I/O.  Hence, any VFSFile passed
     * to play(), probe_for_tuple(), etc. will be null. */
    ArrayRef<const char *> schemes;
};

class InputPlugin : public Plugin
{
public:
    constexpr InputPlugin (PluginInfo info, InputPluginInfo input_info) :
        Plugin (PLUGIN_TYPE_INPUT, info),
        input_info (input_info) {}

    const InputPluginInfo input_info;

    /* Returns true if the plugin can handle the file. */
    virtual bool is_our_file (const char * filename, VFSFile & file) = 0;

    /* Reads metadata from the file. */
    virtual Tuple read_tuple (const char * filename, VFSFile & file) = 0;

    /* Plays the file.  Returns false on error.  Also see input-api.h. */
    virtual bool play (const char * filename, VFSFile & file) = 0;

    /* Optional.  Writes metadata to the file, returning false on error. */
    virtual bool write_tuple (const char * filename, VFSFile & file, const Tuple & tuple)
        { return false; }

    /* Optional.  Reads an album art image (JPEG or PNG data) from the file.
     * Returns an empty buffer on error. */
    virtual Index<char> read_image (const char * filename, VFSFile & file)
        { return Index<char> (); }

    /* Optional.  Displays a window showing info about the file.  In general,
     * this function should be avoided since Audacious already provides a file
     * info window. */
    virtual bool file_info_box (const char * filename, VFSFile & file)
        { return false; }
};

class DockablePlugin : public Plugin
{
public:
    constexpr DockablePlugin (int type, PluginInfo info) :
        Plugin (type, info) {}

    /* GtkWidget * get_gtk_widget () */
    virtual void * get_gtk_widget () { return nullptr; }

    /* QWidget * get_qt_widget () */
    virtual void * get_qt_widget () { return nullptr; }
};

class GeneralPlugin : public DockablePlugin
{
public:
    constexpr GeneralPlugin (PluginInfo info, bool enabled_by_default) :
        DockablePlugin (PLUGIN_TYPE_GENERAL, info),
        enabled_by_default (enabled_by_default) {}

    const bool enabled_by_default;
};

class VisPlugin : public DockablePlugin
{
public:
    constexpr VisPlugin (PluginInfo info, int vis_type) :
        DockablePlugin (PLUGIN_TYPE_VIS, info),
        vis_type (vis_type) {}

    const int vis_type;  // see AUD_VIS_TYPE_* enum

    /* reset internal state and clear display */
    virtual void clear () = 0;

    /* 512 frames of a single-channel PCM signal */
    virtual void render_mono_pcm (const float * pcm) {}

    /* 512 frames of an interleaved multi-channel PCM signal */
    virtual void render_multi_pcm (const float * pcm, int channels) {}

    /* intensity of frequencies 1/512, 2/512, ..., 256/512 of sample rate */
    virtual void render_freq (const float * freq) {}
};

class IfacePlugin : public Plugin
{
public:
    constexpr IfacePlugin (PluginInfo info) :
        Plugin (PLUGIN_TYPE_IFACE, info) {}

    virtual void show (bool show) = 0;
    virtual void run () = 0;
    virtual void quit () = 0;

    virtual void show_about_window () = 0;
    virtual void hide_about_window () = 0;
    virtual void show_filebrowser (bool open) = 0;
    virtual void hide_filebrowser () = 0;
    virtual void show_jump_to_song () = 0;
    virtual void hide_jump_to_song () = 0;
    virtual void show_prefs_window () = 0;
    virtual void hide_prefs_window () = 0;
    virtual void plugin_menu_add (int id, void func (), const char * name, const char * icon) = 0;
    virtual void plugin_menu_remove (int id, void func ()) = 0;
};

#endif