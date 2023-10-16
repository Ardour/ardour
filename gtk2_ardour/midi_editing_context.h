/*
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2006-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2011 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2013-2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2019 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2019 Damien Zammit <damien@zamaudio.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_midi_editing_context_h__
#define __ardour_midi_editing_context_h__

#include "pbd/signals.h"

#include "temporal/timeline.h"

#include "ardour/session_handle.h"
#include "ardour/types.h"

#include "editing.h"
#include "selection.h"

using ARDOUR::samplepos_t;
using ARDOUR::samplecnt_t;

class VerboseCursor;
class MouseCursors;

class MidiEditingContext : public ARDOUR::SessionHandlePtr
{
public:
	MidiEditingContext ();
	~MidiEditingContext ();

	void set_session (ARDOUR::Session*);

	virtual samplepos_t pixel_to_sample_from_event (double pixel) const = 0;
	virtual samplepos_t pixel_to_sample (double pixel) const = 0;
	virtual double sample_to_pixel (samplepos_t sample) const = 0;
	virtual double sample_to_pixel_unrounded (samplepos_t sample) const = 0;
	virtual double time_to_pixel (Temporal::timepos_t const & pos) const = 0;
	virtual double time_to_pixel_unrounded (Temporal::timepos_t const & pos) const = 0;
	virtual double duration_to_pixels (Temporal::timecnt_t const & pos) const = 0;
	virtual double duration_to_pixels_unrounded (Temporal::timecnt_t const & pos) const = 0;

	virtual Temporal::Beats get_grid_type_as_beats (bool& success, Temporal::timepos_t const & position) = 0;
	virtual Temporal::Beats get_draw_length_as_beats (bool& success, Temporal::timepos_t const & position) = 0;

	virtual int32_t get_grid_beat_divisions (Editing::GridType gt) = 0;
	virtual int32_t get_grid_music_divisions (Editing::GridType gt, uint32_t event_state) = 0;

	/** Set the snap type.
	 * @param t Snap type (defined in editing_syms.h)
	 */
	virtual void set_grid_to (Editing::GridType t) = 0;

	virtual Editing::GridType grid_type () const = 0;
	virtual Editing::SnapMode snap_mode () const = 0;

	/** Set the snap mode.
	 * @param m Snap mode (defined in editing_syms.h)
	 */
	virtual void set_snap_mode (Editing::SnapMode m) = 0;

	virtual void snap_to (Temporal::timepos_t & first,
	                      Temporal::RoundMode   direction = Temporal::RoundNearest,
	                      ARDOUR::SnapPref      pref = ARDOUR::SnapToAny_Visual,
	                      bool                  ensure_snap = false) = 0;

	virtual void snap_to_with_modifier (Temporal::timepos_t & first,
	                                    GdkEvent const*       ev,
	                                    Temporal::RoundMode   direction = Temporal::RoundNearest,
	                                    ARDOUR::SnapPref      gpref = ARDOUR::SnapToAny_Visual,
	                                    bool ensure_snap = false) = 0;

	virtual Temporal::timepos_t snap_to_bbt (Temporal::timepos_t const & start,
	                                         Temporal::RoundMode   direction,
	                                         ARDOUR::SnapPref    gpref) = 0;

	virtual double get_y_origin () const = 0;
	virtual void reset_x_origin (samplepos_t) = 0;
	virtual void reset_y_origin (double) = 0;

	virtual void set_zoom_focus (Editing::ZoomFocus) = 0;
	virtual Editing::ZoomFocus get_zoom_focus () const = 0;
	virtual samplecnt_t get_current_zoom () const = 0;
	virtual void reset_zoom (samplecnt_t) = 0;
	virtual void reposition_and_zoom (samplepos_t, double) = 0;

	virtual Selection& get_selection() const = 0;

	/** Set the mouse mode (gain, object, range, timefx etc.)
	 * @param m Mouse mode (defined in editing_syms.h)
	 * @param force Perform the effects of the change even if no change is required
	 * (ie even if the current mouse mode is equal to @p m)
	 */
	virtual void set_mouse_mode (Editing::MouseMode, bool force = false) = 0;
	/** Step the mouse mode onto the next or previous one.
	 * @param next true to move to the next, otherwise move to the previous
	 */
	virtual void step_mouse_mode (bool next) = 0;
	/** @return The current mouse mode (gain, object, range, timefx etc.)
	 * (defined in editing_syms.h)
	 */
	virtual Editing::MouseMode current_mouse_mode () const = 0;
	/** @return Whether the current mouse mode is an "internal" editing mode. */
	virtual bool internal_editing() const = 0;

	virtual Gdk::Cursor* get_canvas_cursor () const = 0;
	virtual MouseCursors const* cursors () const = 0;
	virtual VerboseCursor* verbose_cursor () const = 0;
};

#endif /* __ardour_midi_editing_context_h__ */
