/*
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_audio_region_edit_h__
#define __gtk_ardour_audio_region_edit_h__

#include <map>

#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/box.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/button.h>
#include <gtkmm/arrow.h>
#include <gtkmm/frame.h>
#include <gtkmm/table.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/separator.h>
#include <gtkmm/spinbutton.h>

#include "widgets/ardour_dropdown.h"

#include "pbd/signals.h"
#include "pbd/crossthread.h"

#include "audio_clock.h"
#include "ardour_dialog.h"
#include "region_editor.h"

namespace ARDOUR {
	class AudioRegion;
	class Session;
}

class AudioRegionView;

class AudioRegionEditor : public RegionEditor
{
public:
	AudioRegionEditor (ARDOUR::Session*, AudioRegionView*);
	~AudioRegionEditor ();

	void peak_amplitude_thread ();
	void on_unmap ();

private:

	void region_changed (PBD::PropertyChange const &);
	void region_fx_changed ();

	void gain_changed ();
	void gain_adjustment_changed ();

	void refill_region_line ();
	void show_on_touch_changed ();
	void show_touched_automation (std::weak_ptr<PBD::Controllable>);

	AudioRegionView*                     _arv;
	std::shared_ptr<ARDOUR::AudioRegion> _audio_region;

	Gtk::Label      gain_label;
	Gtk::Adjustment gain_adjustment;
	Gtk::SpinButton gain_entry;

	Gtk::Label        _polarity_label;
	Gtk::CheckButton  _polarity_toggle;

	Gtk::Label _peak_amplitude_label;
	Gtk::Entry _peak_amplitude;

	Gtk::Label                    _region_line_label;
	ArdourWidgets::ArdourDropdown _region_line;

	Gtk::CheckButton      _show_on_touch;
	PBD::ScopedConnection _ctrl_touched_connection;

	void signal_peak_thread ();
	pthread_t _peak_amplitude_thread_handle;
	void peak_amplitude_found (double);
	PBD::Signal1<void, double> PeakAmplitudeFound;
	PBD::ScopedConnection _peak_amplitude_connection;
	CrossThreadChannel _peak_channel;
};

#endif /* __gtk_ardour_audio_region_edit_h__ */
