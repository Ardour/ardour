/*
    Copyright (C) 2009 Paul Davis

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

#include <gtkmm/spinbutton.h>
#include <glibmm/thread.h>

#include "ardour/types.h"
#include "ardour_dialog.h"
#include "canvas.h"
#include "progress_reporter.h"

namespace ARDOUR {
	class AudioRegion;
        class Session;
}

/// Dialog box to set options for the `strip silence' filter
class StripSilenceDialog : public ArdourDialog, public ProgressReporter
{
public:
        StripSilenceDialog (ARDOUR::Session*, std::list<boost::shared_ptr<ARDOUR::AudioRegion> > const &);
	~StripSilenceDialog ();

	double threshold () const {
		return _threshold.get_value ();
	}

        nframes_t minimum_length () const;
        nframes_t fade_length () const;

private:
        typedef std::list<std::pair<ARDOUR::frameoffset_t,ARDOUR::framecnt_t> > SilenceResult;
	
	void create_waves ();
	void peaks_ready ();
	void canvas_allocation (Gtk::Allocation &);
	void update_silence_rects ();
        void resize_silence_rects ();
	void update ();
	void update_threshold_line ();
	void update_stats (SilenceResult const &);
	void threshold_changed ();
	void update_progress_gui (float);
	void restart_thread ();

	Gtk::SpinButton _threshold;
	AudioClock      _minimum_length;
        AudioClock      _fade_length;
        Gtk::Label      _segment_count_label;
	Gtk::Label      _shortest_silence_label;
	Gtk::Label      _shortest_audible_label;
	Gtk::ProgressBar _progress_bar;

	struct Wave {
		boost::shared_ptr<ARDOUR::AudioRegion> region;
		ArdourCanvas::WaveView* view;
		std::list<ArdourCanvas::SimpleRect*> silence_rects;
		ArdourCanvas::SimpleLine* threshold_line;
		double samples_per_unit;
		SilenceResult silence;
		
		Wave (ArdourCanvas::Group *, boost::shared_ptr<ARDOUR::AudioRegion>);
		~Wave ();
	};

	ArdourCanvas::Canvas* _canvas;
	std::list<Wave*> _waves;
	int _wave_width;
	int _wave_height;

        ARDOUR::framecnt_t max_audible;
        ARDOUR::framecnt_t min_audible;
        ARDOUR::framecnt_t max_silence;
        ARDOUR::framecnt_t min_silence;

	PBD::ScopedConnection* _peaks_ready_connection;

	pthread_t _thread; ///< thread to compute silence in the background
	static void * _detection_thread_work (void *);
	void * detection_thread_work ();
	Glib::Mutex _lock; ///< lock held while the thread is doing work
	Glib::Cond _run_cond; ///< condition to wake the thread
	bool _thread_should_finish; ///< true if the thread should terminate
	PBD::Signal0<void> Completed; ///< emitted when a silence detection has completed
	PBD::ScopedConnection _completed_connection;
	ARDOUR::InterThreadInfo _interthread_info;
};
