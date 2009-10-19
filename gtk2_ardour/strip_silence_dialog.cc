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

#include <gtkmm/table.h>
#include <gtkmm/label.h>
#include <gtkmm/stock.h>
#include "ardour/audioregion.h"
#include "ardour/audiosource.h"
#include "ardour/dB.h"
#include "ardour_ui.h"
#include "strip_silence_dialog.h"
#include "canvas_impl.h"
#include "waveview.h"
#include "simplerect.h"
#include "rgb_macros.h"
#include "i18n.h"

/** Construct Strip silence dialog box */
StripSilenceDialog::StripSilenceDialog (std::list<boost::shared_ptr<ARDOUR::AudioRegion> > const & regions)
	: ArdourDialog (_("Strip Silence")), _wave_width (640), _wave_height (64)
{
	for (std::list<boost::shared_ptr<ARDOUR::AudioRegion> >::const_iterator i = regions.begin(); i != regions.end(); ++i) {

		Wave w;
		w.region = *i;
		w.view = 0;
		w.samples_per_unit = 1;
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

	Gtk::VBox* v = Gtk::manage (new Gtk::VBox);
	Gtk::Button* b = Gtk::manage (new Gtk::Button (_("Update display")));
	b->signal_clicked().connect (mem_fun (*this, &StripSilenceDialog::update_silence_rects));
	v->pack_start (*b, false, false);
	hbox->pack_start (*v, false, false);

	get_vbox()->add (*hbox);

	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button (Gtk::Stock::APPLY, Gtk::RESPONSE_OK);

	_canvas = new ArdourCanvas::CanvasAA ();
	_canvas->signal_size_allocate().connect (mem_fun (*this, &StripSilenceDialog::canvas_allocation));
	_canvas->set_size_request (_wave_width, _wave_height * _waves.size ());

	get_vbox()->pack_start (*_canvas, true, true);

	show_all ();

	create_waves ();
	update_silence_rects ();
}


StripSilenceDialog::~StripSilenceDialog ()
{
	for (std::list<Wave>::iterator i = _waves.begin(); i != _waves.end(); ++i) {
		delete i->view;
		for (std::list<ArdourCanvas::SimpleRect*>::iterator j = i->silence_rects.begin(); j != i->silence_rects.end(); ++j) {
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
		if (i->region->audio_source(0)->peaks_ready (mem_fun (*this, &StripSilenceDialog::peaks_ready), _peaks_ready_connection)) {
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
	_canvas->set_scroll_region (0.0, 0.0, alloc.get_width(), alloc.get_height());
	_wave_width = alloc.get_width ();

	for (std::list<Wave>::iterator i = _waves.begin(); i != _waves.end(); ++i) {
		i->samples_per_unit = ((double) i->region->length() / _wave_width);
	}
}

void
StripSilenceDialog::update_silence_rects ()
{
	int n = 0;

	for (std::list<Wave>::iterator i = _waves.begin(); i != _waves.end(); ++i) {
		for (std::list<ArdourCanvas::SimpleRect*>::iterator j = i->silence_rects.begin(); j != i->silence_rects.end(); ++j) {
			delete *j;
		}

		i->silence_rects.clear ();

		std::list<std::pair<nframes_t, nframes_t> > const silence =
			i->region->find_silence (dB_to_coefficient (threshold ()), minimum_length ());

		for (std::list<std::pair<nframes_t, nframes_t> >::const_iterator j = silence.begin(); j != silence.end(); ++j) {

			ArdourCanvas::SimpleRect* r = new ArdourCanvas::SimpleRect (*(_canvas->root()));
			r->property_x1() = j->first / i->samples_per_unit;
			r->property_x2() = j->second / i->samples_per_unit;
			r->property_y1() = n * _wave_height;
			r->property_y2() = (n + 1) * _wave_height;
			r->property_outline_pixels() = 0;
			r->property_fill_color_rgba() = RGBA_TO_UINT (128, 128, 128, 128);
			i->silence_rects.push_back (r);

		}

		++n;
	}
}
