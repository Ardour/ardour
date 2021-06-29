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

#include <gtkmm/frame.h>

#include "gtkmm2ext/utils.h"

#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/audio_backend.h"

#include "widgets/tooltips.h"

#include "ardour_ui.h"
#include "dsp_stats_ui.h"
#include "timers.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace Gtk;

DspStatisticsGUI::DspStatisticsGUI ()
	: buffer_size_label ("", ALIGN_RIGHT, ALIGN_CENTER)
	, reset_button (_("Reset"))
{
	const size_t nlabels = Session::NTT + AudioEngine::NTT + AudioBackend::NTT;
	char buf[64];

	labels = new Label*[nlabels];
	snprintf (buf, sizeof (buf), "%7.2f msec %6.2f%%", 10000.0, 100.0);

	for (size_t n = 0; n < nlabels; ++n) {
		labels[n] = new Label ("", ALIGN_RIGHT, ALIGN_CENTER);
		set_size_request_to_display_given_text (*labels[n], buf, 0, 0);
	}

	int row = 0;

	table.attach (*manage (new Gtk::Label (_("Buffer size: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	table.attach (buffer_size_label, 2, 3, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	table.attach (*manage (new Gtk::Label (_("Idle: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	table.attach (*labels[AudioEngine::NTT + Session::NTT + AudioBackend::DeviceWait], 2, 3, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	table.attach (*manage (new Gtk::Label (_("DSP: "), ALIGN_RIGHT, ALIGN_CENTER)), 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	table.attach (*labels[AudioEngine::NTT + Session::NTT + AudioBackend::RunLoop], 2, 3, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	Label* right_angle_text1 = manage (new Label ("\xe2\x94\x94", ALIGN_RIGHT, ALIGN_CENTER));

	table.attach (*manage (new Gtk::Label (_("Engine: "), ALIGN_RIGHT, ALIGN_CENTER)), 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	table.attach (*right_angle_text1, 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	table.attach (*labels[AudioEngine::ProcessCallback], 2, 3, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	Label* right_angle_text2 = manage (new Label ("\xe2\x94\x94", ALIGN_RIGHT, ALIGN_CENTER));

	table.attach (*manage (new Gtk::Label (_("Session: "), ALIGN_RIGHT, ALIGN_CENTER)), 1, 2, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	table.attach (*right_angle_text2, 0, 1, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	table.attach (*labels[AudioEngine::NTT + Session::OverallProcess], 2, 3, row, row+1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	row++;

	HBox* hbox2 = manage (new HBox);
	hbox2->pack_start (reset_button, true, true);

	set_border_width (12);
	set_spacing (6);

	info_text.set_markup (_("The measurements shown below are <b>worst case</b>.\n"
	                        "\n"
	                        "This is more important in determining system load\n"
	                        "than an average. To see average values mouse-over\n"
	                        "any line"));

	Gtk::Frame* frame = manage (new Gtk::Frame);
	frame->set_shadow_type (Gtk::SHADOW_IN);
	frame->add (info_text);

	pack_start (*frame, false, false);
	pack_start (table, true, true, 20);
	pack_start (*hbox2, false, false);

	reset_button.signal_clicked().connect (sigc::mem_fun (*this, &DspStatisticsGUI::reset_button_clicked));

	show_all ();
}

void
DspStatisticsGUI::reset_button_clicked ()
{
	ARDOUR::reset_performance_meters (_session);
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
	PBD::microseconds_t min = 0;
	PBD::microseconds_t max = 0;
	double   avg = 0.;
	double   dev = 0.;
	char buf[64];
	char const * const not_measured_string = X_("--");
	double devf;
	double avgf;

	/* translatable math and unit terms */
	const char * const str_msec = _("msec");
	const char * const str_usec = _("usec");
	const char * const str_average = _("average");
	const char * const str_std_dev = _("std dev");

	int bufsize = AudioEngine::instance()->samples_per_cycle ();
	double bufsize_usecs = (bufsize * 1000000.0) / AudioEngine::instance()->sample_rate();
	double bufsize_msecs = (bufsize * 1000.0) / AudioEngine::instance()->sample_rate();
	snprintf (buf, sizeof (buf), "%d samples / %5.2f msecs", bufsize, bufsize_msecs);
	buffer_size_label.set_text (buf);

	if (AudioEngine::instance()->current_backend()->dsp_stats[AudioBackend::DeviceWait].get_stats (min, max, avg, dev)) {

		/* We show the min time here, since that's the worst case
		 * (other timers are max == worst case)
		 */

		if (min > 1000) {
			double minf = min / 1000.0;
			devf = dev / 1000.0;
			avgf = avg / 1000.0;
			snprintf (buf, sizeof (buf), "%7.2f %s %5.2f%%", minf, str_msec, (100.0 * minf) / bufsize_msecs);
		} else {
			snprintf (buf, sizeof (buf), "%" PRId64 " %s %5.2f%%", min, str_usec, (100.0 * min) / bufsize_usecs);
		}
		labels[AudioEngine::NTT + Session::NTT + AudioBackend::DeviceWait]->set_text (buf);

		if (min > 1000) {
			snprintf (buf, sizeof (buf), "%s: %7.2f %s %5.2f%% (%s. %5.2f)", str_average, avgf, str_msec, (100.0 * avgf) / bufsize_msecs, str_std_dev, devf);
		} else {
			snprintf (buf, sizeof (buf), "%s: %7.2f %s %5.2f%% (%s. %5.2f)", str_average, avg, str_usec, (100.0 * avg) / bufsize_usecs, str_std_dev, dev);
		}

		ArdourWidgets::set_tooltip (labels[AudioEngine::NTT + Session::NTT + AudioBackend::DeviceWait], buf);

	} else {
		labels[AudioEngine::NTT + Session::NTT + AudioBackend::DeviceWait]->set_text (not_measured_string);
		ArdourWidgets::set_tooltip (labels[AudioEngine::NTT + Session::NTT + AudioBackend::DeviceWait], "");
	}

	if (AudioEngine::instance()->current_backend()->dsp_stats[AudioBackend::RunLoop].get_stats (min, max, avg, dev)) {

		if (max > 1000) {
			double maxf = max / 1000.0;
			snprintf (buf, sizeof (buf), "%7.2f %s %5.2f%%", maxf, str_msec, (100.0 * maxf) / bufsize_msecs);
		} else {
			snprintf (buf, sizeof (buf), "%" PRId64 " %s %5.2f%%", max, str_usec, (100.0 * max) / bufsize_usecs);
		}
		labels[AudioEngine::NTT + Session::NTT + AudioBackend::RunLoop]->set_text (buf);


		if (min > 1000) {
			devf = dev / 1000.0;
			avgf = avg / 1000.0;
			snprintf (buf, sizeof (buf), "%s: %7.2f %s %5.2f%% (%s. %5.2f)", str_average, avgf, str_msec, (100.0 * avgf) / bufsize_msecs, str_std_dev, devf);
		} else {
			snprintf (buf, sizeof (buf), "%s: %7.2f %s %5.2f%% (%s. %5.2f)", str_average, avg, str_usec, (100.0 * avg) / bufsize_usecs, str_std_dev, dev);
		}

		ArdourWidgets::set_tooltip (labels[AudioEngine::NTT + Session::NTT + AudioBackend::RunLoop], buf);

	} else {
		labels[AudioEngine::NTT + Session::NTT + AudioBackend::RunLoop]->set_text (not_measured_string);
		ArdourWidgets::set_tooltip (labels[AudioEngine::NTT + Session::NTT + AudioBackend::RunLoop], "");
	}

	AudioEngine::instance()->dsp_stats[AudioEngine::ProcessCallback].get_stats (min, max, avg, dev);

	if (_session) {

		PBD::microseconds_t smin = 0;
		PBD::microseconds_t smax = 0;
		double   savg = 0.;
		double   sdev = 0.;

		_session->dsp_stats[AudioEngine::ProcessCallback].get_stats (smin, smax, savg, sdev);

		if (smax > 1000) {
			double maxf = smax / 1000.0;
			snprintf (buf, sizeof (buf), "%7.2f %s %5.2f%%", maxf, str_msec, (100.0 * maxf) / bufsize_msecs);
		} else {
			snprintf (buf, sizeof (buf), "%" PRId64 " %s %5.2f%%", max, str_usec, (100.0 * max) / bufsize_usecs);
		}
		labels[AudioEngine::NTT + Session::OverallProcess]->set_text (buf);

		if (max > 1000) {
			devf = dev / 1000.0;
			avgf = avg / 1000.0;
			snprintf (buf, sizeof (buf), "%s: %7.2f %s %5.2f%% (%s. %5.2f)", str_average, avgf, str_msec, (100.0 * avgf) / bufsize_msecs, str_std_dev, devf);
		} else {
			snprintf (buf, sizeof (buf), "%s: %7.2f %s %5.2f%% (%s. %5.2f)", str_average, avg, str_usec, (100.0 * avg) / bufsize_usecs, str_std_dev, dev);
		}

		ArdourWidgets::set_tooltip (labels[AudioEngine::NTT + Session::OverallProcess], buf);

		/* Subtract session time from engine process time to show
		 * engine overhead
		 */

		min -= smin;
		max -= smax;
		avg -= savg;
		dev -= sdev;

		if (max > 1000) {
			double maxf = max / 1000.0;
			snprintf (buf, sizeof (buf), "%7.2f %s %5.2f%%", maxf, str_msec, (100.0 * maxf) / bufsize_msecs);
		} else {
			snprintf (buf, sizeof (buf), "%" PRId64 " %s %5.2f%%", max, str_usec, (100.0 * max) / bufsize_usecs);
		}
		labels[AudioEngine::ProcessCallback]->set_text (buf);

		if (max > 1000) {
			devf = dev / 1000.0;
			avgf = avg / 1000.0;
			snprintf (buf, sizeof (buf), "%s: %7.2f %s %5.2f%% (%s. %5.2f)", str_average, avgf, str_msec, (100.0 * avgf) / bufsize_msecs, str_std_dev, devf);
		} else {
			snprintf (buf, sizeof (buf), "%s: %7.2f %s %5.2f%% (%s. %5.2f)", str_average, avg, str_usec, (100.0 * avg) / bufsize_usecs, str_std_dev, dev);
		}

		ArdourWidgets::set_tooltip (labels[AudioEngine::ProcessCallback], buf);

	} else {

		if (max > 1000) {
			double maxf = max / 1000.0;
			snprintf (buf, sizeof (buf), "%7.2f %s %5.2f%%", maxf, str_msec, (100.0 * maxf) / bufsize_msecs);
		} else {
			snprintf (buf, sizeof (buf), "%" PRId64 " %s %5.2f%%", max, str_usec, (100.0 * max) / bufsize_usecs);
		}
		labels[AudioEngine::ProcessCallback]->set_text (buf);

		if (max > 1000) {
			devf = dev / 1000.0;
			avgf = avg / 1000.0;
			snprintf (buf, sizeof (buf), "%s: %7.2f %s %5.2f%% (%s. %5.2f)", str_average, avgf, str_msec, (100.0 * avgf) / bufsize_msecs, str_std_dev, devf);
		} else {
			snprintf (buf, sizeof (buf), "%s: %7.2f %s %5.2f%% (%s. %5.2f)", str_average, avg, str_usec, (100.0 * avg) / bufsize_usecs, str_std_dev, dev);
		}

		ArdourWidgets::set_tooltip (labels[AudioEngine::ProcessCallback], buf);


		labels[AudioEngine::NTT + Session::OverallProcess]->set_text (_("No session loaded"));
		ArdourWidgets::set_tooltip (labels[AudioEngine::NTT + Session::OverallProcess], "");
	}
}

bool
DspStatisticsGUI::on_key_press_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}
