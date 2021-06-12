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

DspStatisticsGUI::DspStatisticsGUI ()
	: buffer_size_label ("", ALIGN_RIGHT, ALIGN_CENTER)
{
	const size_t nlabels = Session::NTT + AudioEngine::NTT + AudioBackend::NTT;

	labels = new Label*[nlabels];
	for (size_t n = 0; n < nlabels; ++n) {
		labels[n] = new Label ("", ALIGN_RIGHT, ALIGN_CENTER);
		set_size_request_to_display_given_text (*labels[n], string_compose (_("%1 (%2 - %3 .. %4 "), 10000, 1000, 10000, 1000), 0, 0);
	}

	int row = 0;

	attach (*manage (new Gtk::Label (_("Buffer size: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (buffer_size_label, 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	attach (*manage (new Gtk::Label (_("Device Wait: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (*labels[AudioEngine::NTT + Session::NTT + AudioBackend::DeviceWait], 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	attach (*manage (new Gtk::Label (_("Backend process: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (*labels[AudioEngine::NTT + Session::NTT + AudioBackend::ProcessCallback], 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	attach (*manage (new Gtk::Label (_("Engine: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (*labels[AudioEngine::ProcessCallback], 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	attach (*manage (new Gtk::Label (_("Session: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (*labels[AudioEngine::NTT + Session::OverallProcess], 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	show_all ();
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
	uint64_t min;
	uint64_t max;
	double   avg;
	double   dev;
	double minf;
	double maxf;
	char buf[64];

	int bufsize = AudioEngine::instance()->raw_buffer_size (DataType::AUDIO);
	double bufsize_msecs = (bufsize * 1000.0) / AudioEngine::instance()->sample_rate();
	snprintf (buf, sizeof (buf), "%d samples / %5.2f msecs", bufsize, bufsize_msecs);
	buffer_size_label.set_text (buf);

	AudioEngine::instance()->dsp_stats[AudioEngine::ProcessCallback].get_stats (min, max, avg, dev);

	minf = floor (min / 1000.0);
	maxf = floor (max / 1000.0);
	avg /= 1000.0;
	dev /= 1000.0;

	snprintf (buf, sizeof (buf), "%7.2g msec %5.2g%% (%7.4g - %-7.2g .. %7.2g)", avg, (100.0 * avg) / bufsize_msecs, minf, maxf, dev);
	labels[AudioEngine::ProcessCallback]->set_text (buf);

	AudioEngine::instance()->current_backend()->dsp_stats[AudioBackend::DeviceWait].get_stats (min, max, avg, dev);

	minf = floor (min / 1000.0);
	maxf = floor (max / 1000.0);
	avg /= 1000.0;
	dev /= 1000.0;

	snprintf (buf, sizeof (buf), "%7.2g msec %5.2g%% (%7.4g - %-7.2g .. %7.2g)", avg, (100.0 * avg) / bufsize_msecs, minf, maxf, dev);
	labels[AudioEngine::NTT + Session::NTT + AudioBackend::DeviceWait]->set_text (buf);

	AudioEngine::instance()->current_backend()->dsp_stats[AudioBackend::ProcessCallback].get_stats (min, max, avg, dev);

	minf = floor (min / 1000.0);
	maxf = floor (max / 1000.0);
	avg /= 1000.0;
	dev /= 1000.0;

	snprintf (buf, sizeof (buf), "%7.2g msec %5.2g%% (%7.4g - %-7.2g .. %7.2g)", avg, (100.0 * avg) / bufsize_msecs, minf, maxf, dev);
	labels[AudioEngine::NTT + Session::NTT + AudioBackend::ProcessCallback]->set_text (buf);

	if (_session) {
		_session->dsp_stats[AudioEngine::ProcessCallback].get_stats (min, max, avg, dev);

		min = (uint64_t) floor (min / 1000.0);
		max = (uint64_t) floor (max / 1000.0);
		avg /= 1000.0;
		dev /= 1000.0;

		snprintf (buf, sizeof (buf), "%7.2g msec %5.2g%% (%7.4g - %-7.2g .. %7.2g)", avg, (100.0 * avg) / bufsize_msecs, minf, maxf, dev);
		labels[AudioEngine::NTT + Session::OverallProcess]->set_text (buf);
	}
}
