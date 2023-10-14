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


class MidiEditingContext : public SessionHandlePtr, public PBD::ScopedConnectionList
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
	virtual void reset_zoom (samplecnt_t) = 0;
	virtual void reposition_and_zoom (samplepos_t, double) = 0;

	virtual Selection& get_selection() const = 0;

	virtual void set_mouse_mode (Editing::MouseMode, bool force = false) = 0;
	virtual void step_mouse_mode (bool next) = 0;
	virtual Editing::MouseMode current_mouse_mode () const = 0;
	virtual Editing::MidiEditMode current_midi_edit_mode () const = 0;
	virtual bool internal_editing() const = 0;

	Gdk::Cursor* get_canvas_cursor () const;

	MouseCursors const* cursors () const {
		return _cursors;
	}

	VerboseCursor* verbose_cursor () const {
		return _verbose_cursor;
	}
};

#endif /* __ardour_midi_editing_context_h__ */
