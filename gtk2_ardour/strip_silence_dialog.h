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

namespace ARDOUR {
	class AudioRegion;
        class Session;
}

/// Dialog box to set options for the `strip silence' filter
class StripSilenceDialog : public ArdourDialog
{
public:
        StripSilenceDialog (ARDOUR::Session*, std::list<boost::shared_ptr<ARDOUR::AudioRegion> > const &);
	~StripSilenceDialog ();

	double threshold () const {
		return _threshold.get_value ();
	}

        nframes_t minimum_length () const;
        nframes_t fade_length () const;
        static void stop_thread ();

private:
	void create_waves ();
	void peaks_ready ();
	void canvas_allocation (Gtk::Allocation &);
	void update_silence_rects ();
        void redraw_silence_rects ();

	Gtk::SpinButton _threshold;
	AudioClock      _minimum_length;
        AudioClock      _fade_length;
        Gtk::Label      _segment_count_label;
        typedef std::list<std::pair<ARDOUR::frameoffset_t,ARDOUR::framecnt_t> > SilenceResult;

	struct Wave {
            boost::shared_ptr<ARDOUR::AudioRegion> region;
            ArdourCanvas::WaveView* view;
            std::list<ArdourCanvas::SimpleRect*> silence_rects;
            double samples_per_unit;
            SilenceResult silence;
          
            Wave() : view (0), samples_per_unit (1) { }
	};

	ArdourCanvas::Canvas* _canvas;
	std::list<Wave> _waves;
	int _wave_width;
	int _wave_height;
        bool restart_queued;

        static ARDOUR::InterThreadInfo itt;
        static bool thread_should_exit;
        static Glib::Cond *thread_run;
        static Glib::Cond *thread_waiting;
        static Glib::StaticMutex run_lock;
        static StripSilenceDialog* current;

        ARDOUR::framecnt_t max_audible;
        ARDOUR::framecnt_t min_audible;
        ARDOUR::framecnt_t max_silence;
        ARDOUR::framecnt_t min_silence;

	PBD::ScopedConnection* _peaks_ready_connection;
    
        static bool  _detection_done (void*);
        static void* _detection_thread_work (void*);

        bool  detection_done ();
        void* detection_thread_work ();
        bool  start_silence_detection ();
        void  maybe_start_silence_detection ();

        void update_stats (const SilenceResult&);
};
