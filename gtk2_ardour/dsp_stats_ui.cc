/*
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
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

#include "gtkmm2ext/utils.h"

#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/audio_backend.h"

#include "dsp_stats_ui.h"
#include "timers.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace Gtk;

DspStatisticsGUI::DspStatisticsGUI (Session* s)
	: SessionHandlePtr (s)
{
	const size_t nlabels = Session::NTT + AudioEngine::NTT + AudioBackend::NTT;

	labels = new Label*[nlabels];
	for (size_t n = 0; n < nlabels; ++n) {
		labels[n] = new Label ("", ALIGN_RIGHT, ALIGN_CENTER);
		set_size_request_to_display_given_text (*labels[n], string_compose (_("%1 [ms]"), 99.123), 0, 0);
	}

	// attach (*manage (new Gtk::Label (_("Minimum"), ALIGN_RIGHT, ALIGN_CENTER)),
	// 0, 1, 0, 1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	//attach (_lbl_min, 1, 2, 0, 1, Gtk::FILL, Gtk::SHRINK, 2, 0);
}

void
DspStatisticsGUI::start_updating ()
{
	update ();
	update_connection = Timers::second_connect (sigc::mem_fun(*this, &DspStatisticsGUI::update));
}

void
DspStatisticsGUI::stop_updating ()
{
	update_connection.disconnect ();
}


void
DspStatisticsGUI::update ()
{
}

