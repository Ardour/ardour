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
#include "simpleline.h"
#include "waveview.h"
#include "simplerect.h"
#include "rgb_macros.h"
#include "i18n.h"
#include "logmeter.h"

using namespace ARDOUR;
using namespace std;
using namespace ArdourCanvas;

/** Construct Strip silence dialog box */
StripSilenceDialog::StripSilenceDialog (Session* s, list<boost::shared_ptr<ARDOUR::AudioRegion> > const & regions)
	: ArdourDialog (_("Strip Silence"))
	, ProgressReporter ()
        , _minimum_length (X_("silence duration"), true, "SilenceDurationClock", true, false, true, false)
        , _fade_length (X_("silence duration"), true, "SilenceDurationClock", true, false, true, false)
        , _wave_width (640)
        , _wave_height (64)
	, _peaks_ready_connection (0)
	, _destroying (false)
{
        set_session (s);

	Gtk::HBox* hbox = Gtk::manage (new Gtk::HBox);
        
	Gtk::Table* table = Gtk::manage (new Gtk::Table (3, 3));
	table->set_spacings (6);

	int n = 0;

	table->attach (*Gtk::manage (new Gtk::Label (_("Threshold"), 1, 0.5)), 0, 1, n, n + 1, Gtk::FILL);
	table->attach (_threshold, 1, 2, n, n + 1, Gtk::FILL);
	table->attach (*Gtk::manage (new Gtk::Label (_("dbFS"))), 2, 3, n, n + 1, Gtk::FILL);
	++n;
        
	_threshold.set_digits (1);
	_threshold.set_increments (1, 10);
	_threshold.set_range (-120, 0);
	_threshold.set_value (-60);

	table->attach (*Gtk::manage (new Gtk::Label (_("Minimum length"), 1, 0.5)), 0, 1, n, n + 1, Gtk::FILL);
	table->attach (_minimum_length, 1, 2, n, n + 1, Gtk::FILL);
	++n;
	
        _minimum_length.set_session (s);
        _minimum_length.set_mode (AudioClock::Frames);
        _minimum_length.set (1000, true);

	table->attach (*Gtk::manage (new Gtk::Label (_("Fade length"), 1, 0.5)), 0, 1, n, n + 1, Gtk::FILL);
        table->attach (_fade_length, 1, 2, n, n + 1, Gtk::FILL);
	++n;

        _fade_length.set_session (s);
        _fade_length.set_mode (AudioClock::Frames);
        _fade_length.set (64, true);

	hbox->pack_start (*table);

	table = Gtk::manage (new Gtk::Table (3, 2));
	table->set_spacings (6);
	
	n = 0;
	
	table->attach (*Gtk::manage (new Gtk::Label (_("Silent segments:"), 1, 0.5)), 3, 4, n, n + 1, Gtk::FILL);
        table->attach (_segment_count_label, 5, 6, n, n + 1, Gtk::FILL);
	_segment_count_label.set_alignment (0, 0.5);
	++n;

	table->attach (*Gtk::manage (new Gtk::Label (_("Shortest silence:"), 1, 0.5)), 3, 4, n, n + 1, Gtk::FILL);
        table->attach (_shortest_silence_label, 5, 6, n, n + 1, Gtk::FILL);
	_shortest_silence_label.set_alignment (0, 0.5);
	++n;

	table->attach (*Gtk::manage (new Gtk::Label (_("Shortest audible:"), 1, 0.5)), 3, 4, n, n + 1, Gtk::FILL);
        table->attach (_shortest_audible_label, 5, 6, n, n + 1, Gtk::FILL);
	_shortest_audible_label.set_alignment (0, 0.5);
	++n;
	
	hbox->pack_start (*table);

	/* dummy label for padding */
	hbox->pack_start (*Gtk::manage (new Gtk::Label ("")), true, true);

	get_vbox()->pack_start (*hbox, false, false);

	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button (Gtk::Stock::APPLY, Gtk::RESPONSE_OK);

	_canvas = new CanvasAA ();
	_canvas->signal_size_allocate().connect (sigc::mem_fun (*this, &StripSilenceDialog::canvas_allocation));
	_canvas->set_size_request (_wave_width, _wave_height * regions.size());

	for (list<boost::shared_ptr<ARDOUR::AudioRegion> >::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		Wave* w = new Wave (_canvas->root(), *i);
		_waves.push_back (w);
	}

	get_vbox()->pack_start (*_canvas, true, true);

	get_vbox()->pack_start (_progress_bar, true, true);

	show_all ();

        _threshold.get_adjustment()->signal_value_changed().connect (sigc::mem_fun (*this, &StripSilenceDialog::threshold_changed));
        _minimum_length.ValueChanged.connect (sigc::mem_fun (*this, &StripSilenceDialog::restart_thread));

	create_waves ();
	update_silence_rects ();
	update_threshold_line ();

	/* Create a thread which runs while the dialogue is open to compute the silence regions */
	Completed.connect (_completed_connection, MISSING_INVALIDATOR, ui_bind (&StripSilenceDialog::update, this), gui_context ());
	_thread_should_finish = false;
	pthread_create (&_thread, 0, StripSilenceDialog::_detection_thread_work, this);
}


StripSilenceDialog::~StripSilenceDialog ()
{
	_destroying = true;
	
	/* Terminate our thread */
	
	_lock.lock ();
	_interthread_info.cancel = true;
	_thread_should_finish = true;
	_lock.unlock ();

	_run_cond.signal ();
	pthread_join (_thread, 0);
	
	for (list<Wave*>::iterator i = _waves.begin(); i != _waves.end(); ++i) {
		delete *i;
	}

	_waves.clear ();
	
	delete _peaks_ready_connection;
	delete _canvas;
}

void
StripSilenceDialog::create_waves ()
{
	int n = 0;

	delete _peaks_ready_connection;
	_peaks_ready_connection = 0;

	for (list<Wave*>::iterator i = _waves.begin(); i != _waves.end(); ++i) {
		if ((*i)->region->audio_source(0)->peaks_ready (boost::bind (&StripSilenceDialog::peaks_ready, this), &_peaks_ready_connection, gui_context())) {
			(*i)->view = new WaveView (*(_canvas->root()));
			(*i)->view->property_data_src() = static_cast<gpointer>((*i)->region.get());
			(*i)->view->property_cache() = WaveView::create_cache ();
			(*i)->view->property_cache_updater() = true;
			(*i)->view->property_channel() = 0;
			(*i)->view->property_length_function() = (void *) region_length_from_c;
			(*i)->view->property_sourcefile_length_function() = (void *) sourcefile_length_from_c;
			(*i)->view->property_peak_function() = (void *) region_read_peaks_from_c;
			(*i)->view->property_x() = 0;
			(*i)->view->property_y() = n * _wave_height;
			(*i)->view->property_height() = _wave_height;
			(*i)->view->property_samples_per_unit() = (*i)->samples_per_unit;
			(*i)->view->property_region_start() = (*i)->region->start();
			(*i)->view->property_wave_color() = ARDOUR_UI::config()->canvasvar_WaveForm.get();
			(*i)->view->property_fill_color() = ARDOUR_UI::config()->canvasvar_WaveFormFill.get();
			(*i)->view->property_logscaled() = true;
			(*i)->view->property_rectified() = true;
		}

		++n;
	}
}

void
StripSilenceDialog::peaks_ready ()
{
	delete _peaks_ready_connection;
	_peaks_ready_connection = 0;
	create_waves ();
}

void
StripSilenceDialog::canvas_allocation (Gtk::Allocation& alloc)
{
        int n = 0;

	_canvas->set_scroll_region (0.0, 0.0, alloc.get_width(), alloc.get_height());
	_wave_width = alloc.get_width ();
        _wave_height = alloc.get_height ();

	for (list<Wave*>::iterator i = _waves.begin(); i != _waves.end(); ++i, ++n) {
		(*i)->samples_per_unit = ((double) (*i)->region->length() / _wave_width);

                if ((*i)->view) {
                        (*i)->view->property_y() = n * _wave_height;
                        (*i)->view->property_samples_per_unit() = (*i)->samples_per_unit;
                        (*i)->view->property_height() = _wave_height;
                }
	}

        resize_silence_rects ();
	update_threshold_line ();
}

void
StripSilenceDialog::resize_silence_rects ()
{
	int n = 0;

	/* Lock so that we don't contend with the detection thread for access to the silence regions */
	Glib::Mutex::Lock lm (_lock);
	
	for (list<Wave*>::iterator i = _waves.begin(); i != _waves.end(); ++i) {

                list<pair<frameoffset_t, framecnt_t> >::const_iterator j;
                list<SimpleRect*>::iterator r;

		for (j = (*i)->silence.begin(), r = (*i)->silence_rects.begin(); 
                     j != (*i)->silence.end() && r != (*i)->silence_rects.end(); ++j, ++r) {
                        (*r)->property_x1() = j->first / (*i)->samples_per_unit;
                        (*r)->property_x2() = j->second / (*i)->samples_per_unit;
                        (*r)->property_y1() = n * _wave_height;
                        (*r)->property_y2() = (n + 1) * _wave_height;
                        (*r)->property_outline_pixels() = 0;
                        (*r)->property_fill_color_rgba() = RGBA_TO_UINT (128, 128, 128, 128);
                }

                ++n;
        }
}

void
StripSilenceDialog::update_threshold_line ()
{
	int n = 0;

	/* Don't need to lock here as we're not reading the _waves silence details */

	for (list<Wave*>::iterator i = _waves.begin(); i != _waves.end(); ++i) {
		(*i)->threshold_line->property_x1() = 0;
		(*i)->threshold_line->property_x2() = _wave_width;
		
		double const y = alt_log_meter (_threshold.get_value());

		(*i)->threshold_line->property_y1() = (n + 1 - y) * _wave_height;
		(*i)->threshold_line->property_y2() = (n + 1 - y) * _wave_height;
	}

	++n;
}

void
StripSilenceDialog::update ()
{
	update_silence_rects ();
	update_threshold_line ();
	/* XXX: first one only?! */
	update_stats (_waves.front()->silence);
}

void
StripSilenceDialog::update_silence_rects ()
{
	int n = 0;
        uint32_t max_segments = 0;
        uint32_t sc;

	/* Lock so that we don't contend with the detection thread for access to the silence regions */
	Glib::Mutex::Lock lm (_lock);

	for (list<Wave*>::iterator i = _waves.begin(); i != _waves.end(); ++i) {
		for (list<SimpleRect*>::iterator j = (*i)->silence_rects.begin(); j != (*i)->silence_rects.end(); ++j) {
			delete *j;
		}

                (*i)->silence_rects.clear ();                
                sc = 0;

		for (list<pair<frameoffset_t, framecnt_t> >::const_iterator j = (*i)->silence.begin(); j != (*i)->silence.end(); ++j) {

			SimpleRect* r = new SimpleRect (*(_canvas->root()));
			r->property_x1() = j->first / (*i)->samples_per_unit;
			r->property_x2() = j->second / (*i)->samples_per_unit;
			r->property_y1() = n * _wave_height;
			r->property_y2() = (n + 1) * _wave_height;
			r->property_outline_pixels() = 0;
			r->property_fill_color_rgba() = RGBA_TO_UINT (128, 128, 128, 128);
			(*i)->silence_rects.push_back (r);
                        sc++;
		}

                max_segments = max (max_segments, sc);
		++n;
	}

        if (min_audible > 0) {
                float ms, ma;
                char const * aunits;
                char const * sunits;

                ma = (float) min_audible/_session->frame_rate();
                ms = (float) min_silence/_session->frame_rate();

                if (min_audible < _session->frame_rate()) {
                        aunits = _("ms");
                        ma *= 1000.0;
                } else {
                        aunits = _("s");
                }

                if (min_silence < _session->frame_rate()) {
                        sunits = _("ms");
                        ms *= 1000.0;
                } else {
                        sunits = _("s");
                }

                _segment_count_label.set_text (string_compose ("%1", max_segments));
		if (max_segments > 0) {
			_shortest_silence_label.set_text (string_compose ("%1 %2", ms, sunits));
			_shortest_audible_label.set_text (string_compose ("%1 %2", ma, aunits));
		} else {
			_shortest_silence_label.set_text ("");
			_shortest_audible_label.set_text ("");
		}
        } else {
                _segment_count_label.set_text (_("Full silence"));
		_shortest_silence_label.set_text ("");
		_shortest_audible_label.set_text ("");
        }
}

void *
StripSilenceDialog::_detection_thread_work (void* arg)
{
	StripSilenceDialog* d = reinterpret_cast<StripSilenceDialog*> (arg);
	return d->detection_thread_work ();
}

/** Body of our silence detection thread */
void *
StripSilenceDialog::detection_thread_work ()
{
        ARDOUR_UI::instance()->register_thread ("gui", pthread_self(), "silence", 32);

	/* Hold this lock when we are doing work */
	_lock.lock ();
	
	while (1) {
		for (list<Wave*>::iterator i = _waves.begin(); i != _waves.end(); ++i) {
			(*i)->silence = (*i)->region->find_silence (dB_to_coefficient (threshold ()), minimum_length (), _interthread_info);
			if (_interthread_info.cancel) {
				break;
			}
		}

		if (!_interthread_info.cancel) {
			Completed (); /* EMIT SIGNAL */
		}

		/* Our work is done; sleep until there is more to do.
		 * The lock is released while we are waiting.
		 */
		_run_cond.wait (_lock);

		if (_thread_should_finish) {
			_lock.unlock ();
			return 0;
		}
	}

	return 0;
}

void
StripSilenceDialog::restart_thread ()
{
	if (_destroying) {
		/* I don't know how this happens, but it seems to be possible for this
		   method to be called after our destructor has finished executing.
		   If this happens, bad things follow; _lock cannot be locked and
		   Ardour hangs.  So if we are destroying, just bail early.
		*/
		return;
	}
	
	/* Cancel any current run */
	_interthread_info.cancel = true;

	/* Block until the thread waits() */
	_lock.lock ();
	/* Reset the flag */
	_interthread_info.cancel = false;
	_lock.unlock ();

	/* And re-awake the thread */
	_run_cond.signal ();
}

void
StripSilenceDialog::threshold_changed ()
{
	update_threshold_line ();
	restart_thread ();
}

void
StripSilenceDialog::update_stats (const SilenceResult& res)
{
        if (res.empty()) {
                return;
        }

        max_silence = 0;
        min_silence = max_framepos;
        max_audible = 0;
        min_audible = max_framepos;
        
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

framecnt_t
StripSilenceDialog::minimum_length () const
{
        return _minimum_length.current_duration (_waves.front()->region->position());
}

framecnt_t
StripSilenceDialog::fade_length () const
{
        return _minimum_length.current_duration (_waves.front()->region->position());
}
		
StripSilenceDialog::Wave::Wave (Group* g, boost::shared_ptr<AudioRegion> r)
	: region (r), view (0), samples_per_unit (1)
{
	threshold_line = new ArdourCanvas::SimpleLine (*g);
	threshold_line->property_color_rgba() = RGBA_TO_UINT (0, 0, 0, 128);
}

StripSilenceDialog::Wave::~Wave ()
{
	delete view;
	delete threshold_line;
	for (list<SimpleRect*>::iterator i = silence_rects.begin(); i != silence_rects.end(); ++i) {
		delete *i;
	}
}

void
StripSilenceDialog::update_progress_gui (float p)
{
	_progress_bar.set_fraction (p);
}
