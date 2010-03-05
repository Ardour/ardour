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

#include <iostream>

#include <gtkmm/table.h>
#include <gtkmm/label.h>
#include <gtkmm/stock.h>
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"

#include "ardour/dB.h"
#include "ardour_ui.h"
#include "ardour/session.h"

#include "gui_thread.h"
#include "strip_silence_dialog.h"
#include "canvas_impl.h"
#include "waveview.h"
#include "simplerect.h"
#include "rgb_macros.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace ArdourCanvas;

Glib::StaticMutex StripSilenceDialog::run_lock;
Glib::Cond*       StripSilenceDialog::thread_waiting = 0;
Glib::Cond*       StripSilenceDialog::thread_run = 0;
bool              StripSilenceDialog::thread_should_exit = false;
InterThreadInfo   StripSilenceDialog::itt;
StripSilenceDialog* StripSilenceDialog::current = 0;

/** Construct Strip silence dialog box */
StripSilenceDialog::StripSilenceDialog (Session* s, std::list<boost::shared_ptr<ARDOUR::AudioRegion> > const & regions)
	: ArdourDialog (_("Strip Silence"))
        , _minimum_length (X_("silence duration"), true, "SilenceDurationClock", true, false, true, false)
        , _fade_length (X_("silence duration"), true, "SilenceDurationClock", true, false, true, false)
        , _wave_width (640)
        , _wave_height (64)
        , restart_queued (false)
{
        set_session (s);

        if (thread_waiting == 0) {
                thread_waiting = new Glib::Cond;
                thread_run = new Glib::Cond;
        }
        
	for (std::list<boost::shared_ptr<ARDOUR::AudioRegion> >::const_iterator i = regions.begin(); i != regions.end(); ++i) {

		Wave w;
		w.region = *i;
		_waves.push_back (w);

	}

	Gtk::HBox* hbox = Gtk::manage (new Gtk::HBox);
	hbox->set_spacing (16);

	Gtk::Table* table = Gtk::manage (new Gtk::Table (4, 3));
	table->set_spacings (4);

	Gtk::Label* l = Gtk::manage (new Gtk::Label (_("Threshold:")));
	l->set_alignment (1, 0.5);
        
        hbox->pack_start (*l, false, false);
        hbox->pack_start (_threshold, true, true);

	_threshold.set_digits (1);
	_threshold.set_increments (1, 10);
	_threshold.set_range (-120, 0);
	_threshold.set_value (-60);

	l = Gtk::manage (new Gtk::Label (_("dBFS")));
	l->set_alignment (0, 0.5);

        hbox->pack_start (*l, false, false);
        
	l = Gtk::manage (new Gtk::Label (_("Minimum length:")));
	l->set_alignment (1, 0.5);

        hbox->pack_start (*l, false, false);
        hbox->pack_start (_minimum_length, true, true);

        _minimum_length.set_session (s);
        _minimum_length.set_mode (AudioClock::Frames);
        _minimum_length.set (1000, true);

	l = Gtk::manage (new Gtk::Label (_("Fade length:")));
	l->set_alignment (1, 0.5);

        hbox->pack_start (*l, false, false);
        hbox->pack_start (_fade_length, true, true);

        _fade_length.set_session (s);
        _fade_length.set_mode (AudioClock::Frames);
        _fade_length.set (64, true);

        _segment_count_label.set_text (_("Silent segments: none"));

	get_vbox()->pack_start (*hbox, false, false);


	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button (Gtk::Stock::APPLY, Gtk::RESPONSE_OK);

	_canvas = new CanvasAA ();
	_canvas->signal_size_allocate().connect (sigc::mem_fun (*this, &StripSilenceDialog::canvas_allocation));
	_canvas->set_size_request (_wave_width, _wave_height * _waves.size ());

	get_vbox()->pack_start (*_canvas, true, true);
	get_vbox()->pack_start (_segment_count_label, false, false);

	show_all ();

        _threshold.get_adjustment()->signal_value_changed().connect (sigc::mem_fun (*this, &StripSilenceDialog::maybe_start_silence_detection));
        _minimum_length.ValueChanged.connect (sigc::mem_fun (*this, &StripSilenceDialog::maybe_start_silence_detection));

	create_waves ();
	update_silence_rects ();
}


StripSilenceDialog::~StripSilenceDialog ()
{
	for (std::list<Wave>::iterator i = _waves.begin(); i != _waves.end(); ++i) {
		delete i->view;
		for (std::list<SimpleRect*>::iterator j = i->silence_rects.begin(); j != i->silence_rects.end(); ++j) {
			delete *j;
		}
	}

	delete _canvas;
}

void
StripSilenceDialog::create_waves ()
{
	int n = 0;

	for (std::list<Wave>::iterator i = _waves.begin(); i != _waves.end(); ++i) {
		if (i->region->audio_source(0)->peaks_ready (boost::bind (&StripSilenceDialog::peaks_ready, this), _peaks_ready_connection, gui_context())) {
			i->view = new WaveView (*(_canvas->root()));
			i->view->property_data_src() = static_cast<gpointer>(i->region.get());
			i->view->property_cache() = WaveView::create_cache ();
			i->view->property_cache_updater() = true;
			i->view->property_channel() = 0;
			i->view->property_length_function() = (void *) region_length_from_c;
			i->view->property_sourcefile_length_function() = (void *) sourcefile_length_from_c;
			i->view->property_peak_function() = (void *) region_read_peaks_from_c;
			i->view->property_x() = 0;
			i->view->property_y() = n * _wave_height;
			i->view->property_height() = _wave_height;
			i->view->property_samples_per_unit() = i->samples_per_unit;
			i->view->property_region_start() = i->region->start();
			i->view->property_wave_color() = ARDOUR_UI::config()->canvasvar_WaveForm.get();
			i->view->property_fill_color() = ARDOUR_UI::config()->canvasvar_WaveFormFill.get();
		}

		++n;
	}
}

void
StripSilenceDialog::peaks_ready ()
{
	_peaks_ready_connection.disconnect ();
	create_waves ();
}

void
StripSilenceDialog::canvas_allocation (Gtk::Allocation& alloc)
{
        int n = 0;

	_canvas->set_scroll_region (0.0, 0.0, alloc.get_width(), alloc.get_height());
	_wave_width = alloc.get_width ();
        _wave_height = alloc.get_height ();

	for (std::list<Wave>::iterator i = _waves.begin(); i != _waves.end(); ++i, ++n) {
		i->samples_per_unit = ((double) i->region->length() / _wave_width);

                if (i->view) {
                        i->view->property_y() = n * _wave_height;
                        i->view->property_samples_per_unit() = i->samples_per_unit;
                        i->view->property_height() = _wave_height;
                }
	}

        redraw_silence_rects ();
}

void
StripSilenceDialog::redraw_silence_rects ()
{
	int n = 0;

	for (std::list<Wave>::iterator i = _waves.begin(); i != _waves.end(); ++i) {

                std::list<std::pair<frameoffset_t, framecnt_t> >::const_iterator j;
                std::list<SimpleRect*>::iterator r;

		for (j = i->silence.begin(), r = i->silence_rects.begin(); 
                     j != i->silence.end() && r != i->silence_rects.end(); ++j, ++r) {
                        (*r)->property_x1() = j->first / i->samples_per_unit;
                        (*r)->property_x2() = j->second / i->samples_per_unit;
                        (*r)->property_y1() = n * _wave_height;
                        (*r)->property_y2() = (n + 1) * _wave_height;
                        (*r)->property_outline_pixels() = 0;
                        (*r)->property_fill_color_rgba() = RGBA_TO_UINT (128, 128, 128, 128);
                }

                ++n;
        }
}

void
StripSilenceDialog::update_silence_rects ()
{
	int n = 0;
        uint32_t max_segments = 0;
        uint32_t sc;

	for (std::list<Wave>::iterator i = _waves.begin(); i != _waves.end(); ++i) {
		for (std::list<SimpleRect*>::iterator j = i->silence_rects.begin(); j != i->silence_rects.end(); ++j) {
			delete *j;
		}

                i->silence_rects.clear ();                
                sc = 0;

		for (std::list<std::pair<frameoffset_t, framecnt_t> >::const_iterator j = i->silence.begin(); j != i->silence.end(); ++j) {

			SimpleRect* r = new SimpleRect (*(_canvas->root()));
			r->property_x1() = j->first / i->samples_per_unit;
			r->property_x2() = j->second / i->samples_per_unit;
			r->property_y1() = n * _wave_height;
			r->property_y2() = (n + 1) * _wave_height;
			r->property_outline_pixels() = 0;
			r->property_fill_color_rgba() = RGBA_TO_UINT (128, 128, 128, 128);
			i->silence_rects.push_back (r);
                        sc++;
		}

                max_segments = max (max_segments, sc);
		++n;
	}

        if (min_audible > 0) {
                float ms, ma;
                char* aunits;
                char* sunits;

                ma = (float) min_audible/_session->frame_rate();
                ms = (float) min_silence/_session->frame_rate();

                if (min_audible > _session->frame_rate()) {
                        aunits = _("secs");
                        ma /= 1000.0;
                } else {
                        aunits = _("msecs");
                }

                if (min_silence > _session->frame_rate()) {
                        sunits = _("secs");
                        ms /= 1000.0;
                } else {
                        sunits = _("msecs");
                }

                _segment_count_label.set_text (string_compose (_("Silent segments: %1\nShortest silence %2 %3 Shortest audible %4 %5"), 
                                                               max_segments, ms, sunits, ma, aunits));
        } else {
                _segment_count_label.set_text (_("Full silence"));
        }
}

bool
StripSilenceDialog::_detection_done (void* arg)
{
        StripSilenceDialog* ssd = (StripSilenceDialog*) arg;
        return ssd->detection_done ();
}

bool
StripSilenceDialog::detection_done ()
{
        get_window()->set_cursor (Gdk::Cursor (Gdk::LEFT_PTR));
        update_silence_rects ();
        return false;
}

void*
StripSilenceDialog::_detection_thread_work (void* arg)
{
        StripSilenceDialog* ssd = (StripSilenceDialog*) arg;
        return ssd->detection_thread_work ();
}

void*
StripSilenceDialog::detection_thread_work ()
{
        ARDOUR_UI::instance()->register_thread ("gui", pthread_self(), "silence", 32);
        
        while (1) {

                run_lock.lock ();
                thread_waiting->signal ();
                thread_run->wait (run_lock);

                if (thread_should_exit) {
                        thread_waiting->signal ();
                        run_lock.unlock ();
                        break;
                }

                if (current) {
                        StripSilenceDialog* ssd = current;
                        run_lock.unlock ();
                        
                        for (std::list<Wave>::iterator i = ssd->_waves.begin(); i != ssd->_waves.end(); ++i) {
                                i->silence = i->region->find_silence (dB_to_coefficient (ssd->threshold ()), ssd->minimum_length (), ssd->itt);
                                ssd->update_stats (i->silence);
                        }
                        
                        if (!ssd->itt.cancel) {
                                g_idle_add ((gboolean (*)(void*)) StripSilenceDialog::_detection_done, ssd);
                        }
                } else {
                        run_lock.unlock ();
                }

        }
        
        return 0;
}

void
StripSilenceDialog::maybe_start_silence_detection ()
{
        if (!restart_queued) {
                restart_queued = true;
                Glib::signal_idle().connect (sigc::mem_fun (*this, &StripSilenceDialog::start_silence_detection));
        }
}

bool
StripSilenceDialog::start_silence_detection ()
{
        Glib::Mutex::Lock lm (run_lock);
        restart_queued = false;

        if (!itt.thread) {

                itt.done = false;
                itt.cancel = false;
                itt.progress = 0.0;
                current = this;

                pthread_create (&itt.thread, 0, StripSilenceDialog::_detection_thread_work, this);
                /* wait for it to get started */
                thread_waiting->wait (run_lock);

        } else {
                
                /* stop whatever the thread is doing */

                itt.cancel = 1;
                current = 0;

                while (!itt.done) {
                        thread_run->signal ();
                        thread_waiting->wait (run_lock);
                }
        }


        itt.cancel = false;
        itt.done = false;
        itt.progress = 0.0;
        current = this;
        
        /* and start it up (again) */
        
        thread_run->signal ();

        /* change cursor */

        get_window()->set_cursor (Gdk::Cursor (Gdk::WATCH));

        /* don't call again until needed */
        
        return false;
}

void
StripSilenceDialog::stop_thread ()
{
        Glib::Mutex::Lock lm (run_lock);

        itt.cancel = true;
        thread_should_exit = true;
        thread_run->signal (); 
        thread_waiting->wait (run_lock);
        itt.thread = 0;
}

void
StripSilenceDialog::update_stats (const SilenceResult& res)
{
        if (res.empty()) {
                return;
        }

        max_silence = 0;
        min_silence = max_frames;
        max_audible = 0;
        min_audible = max_frames;
        
        SilenceResult::const_iterator cur;

        cur = res.begin();

        framepos_t start = 0;
        framepos_t end;
        bool in_silence;

        if (cur->first == 0) {
                /* initial segment, starting at zero, is silent */
                end = cur->second;
                in_silence = true;
        } else {
                /* initial segment, starting at zero, is audible */
                end = cur->first;
                in_silence = false;
        }

        while (cur != res.end()) {

                framecnt_t interval_duration;

                interval_duration = end - start;

                if (in_silence) {

                        max_silence = max (max_silence, interval_duration);
                        min_silence = min (min_silence, interval_duration);
                } else {

                        max_audible = max (max_audible, interval_duration);
                        min_audible = min (min_audible, interval_duration);
                }

                start = end;
                ++cur;
                end = cur->first;
                in_silence = !in_silence;
        }
}

nframes_t
StripSilenceDialog::minimum_length () const
{
        return _minimum_length.current_duration (_waves.front().region->position());
}

nframes_t
StripSilenceDialog::fade_length () const
{
        return _minimum_length.current_duration (_waves.front().region->position());
}
