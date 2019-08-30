/*
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <gtkmm/spinbutton.h>
#include <glibmm/threads.h>

#include <pbd/xml++.h>

#include "ardour/types.h"
#include "ardour_dialog.h"
#include "progress_reporter.h"

namespace ARDOUR {
	class Session;
}

class AudioClock;
class RegionView;

/// Dialog box to set options for the `strip silence' filter
class StripSilenceDialog : public ArdourDialog, public ProgressReporter
{
public:
	StripSilenceDialog (ARDOUR::Session*, std::list<RegionView*> const &);
	~StripSilenceDialog ();

	double threshold () const {
		return _threshold.get_value ();
	}

	void drop_rects ();

	void silences (ARDOUR::AudioIntervalMap&);

	ARDOUR::samplecnt_t minimum_length () const;
	ARDOUR::samplecnt_t fade_length () const;

	void on_response (int response_id) {
		Gtk::Dialog::on_response (response_id);
	}

	XMLNode& get_state ();
	void set_state (const XMLNode &);

private:
	void create_waves ();
	void canvas_allocation (Gtk::Allocation &);
	void update_silence_rects ();
	void resize_silence_rects ();
	void update ();
	void update_threshold_line ();
	void update_stats (ARDOUR::AudioIntervalResult const &);
	void threshold_changed ();
	void update_progress_gui (float);
	void restart_thread ();
	void finished(int);

	Gtk::SpinButton _threshold;
	AudioClock*      _minimum_length;
	AudioClock*      _fade_length;
	Gtk::ProgressBar _progress_bar;

	Gtk::Button* cancel_button;
	Gtk::Button* apply_button;

	struct ViewInterval {
		RegionView* view;
		ARDOUR::AudioIntervalResult intervals;

		ViewInterval (RegionView* rv) : view (rv) {}
	};

	std::list<ViewInterval> views;

	bool _destroying;

	pthread_t _thread; ///< thread to compute silence in the background
	static void * _detection_thread_work (void *);
	void * detection_thread_work ();
	Glib::Threads::Mutex _lock; ///< lock held while the thread is doing work
	Glib::Threads::Cond  _run_cond; ///< condition to wake the thread
	bool _thread_should_finish; ///< true if the thread should terminate
	PBD::Signal0<void> Completed; ///< emitted when a silence detection has completed
	PBD::ScopedConnection _completed_connection;
	ARDOUR::InterThreadInfo _interthread_info;

	sigc::connection progress_idle_connection;
	bool idle_update_progress(); ///< GUI-thread progress updates of background silence computation
	int analysis_progress_cur;
	int analysis_progress_max;

	int _threshold_value;
	ARDOUR::samplecnt_t _minimum_length_value;
	ARDOUR::samplecnt_t _fade_length_value;
};
