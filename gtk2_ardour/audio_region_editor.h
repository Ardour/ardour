/*
    Copyright (C) 2001 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __gtk_ardour_audio_region_edit_h__
#define __gtk_ardour_audio_region_edit_h__

#include <map>

#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/box.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/button.h>
#include <gtkmm/arrow.h>
#include <gtkmm/frame.h>
#include <gtkmm/table.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/separator.h>
#include <gtkmm/spinbutton.h>


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
	AudioRegionEditor (ARDOUR::Session*, boost::shared_ptr<ARDOUR::AudioRegion>);
	~AudioRegionEditor ();

	void peak_amplitude_thread ();

  private:

	void region_changed (PBD::PropertyChange const &);

	void gain_changed ();
	void gain_adjustment_changed ();

	boost::shared_ptr<ARDOUR::AudioRegion> _audio_region;

	Gtk::Label gain_label;
	Gtk::Adjustment gain_adjustment;
	Gtk::SpinButton gain_entry;

	Gtk::Label _peak_amplitude_label;
	Gtk::Entry _peak_amplitude;

	pthread_t _peak_amplitude_thread_handle;
	void peak_amplitude_found (double);
	PBD::Signal1<void, double> PeakAmplitudeFound;
	PBD::ScopedConnection _peak_amplitude_connection;
	CrossThreadChannel _peak_channel;
};

#endif /* __gtk_ardour_audio_region_edit_h__ */
