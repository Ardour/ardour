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
StripSilenceDialog::StripSilenceDialog (std::list<boost::shared_ptr<ARDOUR::AudioRegion> > const & regions)
	: ArdourDialog (_("Strip Silence"))
        , _wave_width (640)
        , _wave_height (64)
        , restart_queued (false)
{
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
	table->attach (*l, 0, 1, 0, 1, Gtk::FILL, Gtk::FILL);
	_threshold.set_digits (1);
	_threshold.set_increments (1, 10);
	_threshold.set_range (-120, 0);
	_threshold.set_value (-60);
	table->attach (_threshold, 1, 2, 0, 1, Gtk::FILL, Gtk::FILL);
	l = Gtk::manage (new Gtk::Label (_("dBFS")));
	l->set_alignment (0, 0.5);
	table->attach (*l, 2, 3, 0, 1, Gtk::FILL, Gtk::FILL);

	l = Gtk::manage (new Gtk::Label (_("Minimum length:")));
	l->set_alignment (1, 0.5);
	table->attach (*l, 0, 1, 1, 2, Gtk::FILL, Gtk::FILL);
	_minimum_length.set_digits (0);
	_minimum_length.set_increments (1, 10);
	_minimum_length.set_range (0, 65536);
	_minimum_length.set_value (256);
	table->attach (_minimum_length, 1, 2, 1, 2, Gtk::FILL, Gtk::FILL);
	l = Gtk::manage (new Gtk::Label (_("samples")));
	table->attach (*l, 2, 3, 1, 2, Gtk::FILL, Gtk::FILL);

	l = Gtk::manage (new Gtk::Label (_("Fade length:")));
	l->set_alignment (1, 0.5);
	table->attach (*l, 0, 1, 2, 3, Gtk::FILL, Gtk::FILL);
	_fade_length.set_digits (0);
	_fade_length.set_increments (1, 10);
	_fade_length.set_range (0, 1024);
	_fade_length.set_value (64);
	table->attach (_fade_length, 1, 2, 2, 3, Gtk::FILL, Gtk::FILL);
	l = Gtk::manage (new Gtk::Label (_("samples")));
	table->attach (*l, 2, 3, 2, 3, Gtk::FILL, Gtk::FILL);

	hbox->pack_start (*table, false, false);

        _segment_count_label.set_text (_("Silent segments: none"));
	hbox->pack_start (_segment_count_label, false, false);

	get_vbox()->pack_start (*hbox, false, false);

	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button (Gtk::Stock::APPLY, Gtk::RESPONSE_OK);

	_canvas = new CanvasAA ();
	_canvas->signal_size_allocate().connect (sigc::mem_fun (*this, &StripSilenceDialog::canvas_allocation));
	_canvas->set_size_request (_wave_width, _wave_height * _waves.size ());

	get_vbox()->pack_start (*_canvas, true, true);

	show_all ();

        _threshold.get_adjustment()->signal_value_changed().connect (sigc::mem_fun (*this, &StripSilenceDialog::maybe_start_silence_detection));
        _minimum_length.get_adjustment()->signal_value_changed().connect (sigc::mem_fun (*this, &StripSilenceDialog::maybe_start_silence_detection));

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

        _segment_count_label.set_text (string_compose (_("Silent segments: %1"), max_segments));
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
        
        cerr << pthread_self() << ": thread exists\n";

        while (1) {

                run_lock.lock ();
                cerr << pthread_self() << ": thread notes that its waiting\n";
                thread_waiting->signal ();
                cerr << pthread_self() << ": thread waits to run\n";
                thread_run->wait (run_lock);

                cerr << pthread_self() << ": silence thread active\n";

                if (thread_should_exit) {
                        thread_waiting->signal ();
                        run_lock.unlock ();
                        cerr << pthread_self() << ": silence thread exited\n";
                        break;
                }

                if (current) {
                        StripSilenceDialog* ssd = current;
                        run_lock.unlock ();
                        
                        for (std::list<Wave>::iterator i = ssd->_waves.begin(); i != ssd->_waves.end(); ++i) {
                                i->silence = i->region->find_silence (dB_to_coefficient (ssd->threshold ()), ssd->minimum_length (), ssd->itt);
                        }
                        
                        if (!ssd->itt.cancel) {
                                g_idle_add ((gboolean (*)(void*)) StripSilenceDialog::_detection_done, ssd);
                        }
                }

                cerr << pthread_self() << ": silence iteration done\n";
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
                cerr << "Wait for new thread to be ready\n";
                thread_waiting->wait (run_lock);
                cerr << "\tits ready\n";

        } else {
                
                /* stop whatever the thread is doing */

                itt.cancel = 1;
                current = 0;

                while (!itt.done) {
                        cerr << "tell existing thread to stop\n";
                        thread_run->signal ();
                        cerr << "wait for existing thread to stop\n";
                        thread_waiting->wait (run_lock);
                        cerr << "its stopped\n";
                }
        }


        itt.cancel = false;
        itt.done = false;
        itt.progress = 0.0;
        current = this;
        
        /* and start it up (again) */
        
        cerr << "signal thread to run again\n";
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
}
