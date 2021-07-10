/*
 * Copyright (C) 2008-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2008-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
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

#include <algorithm>
#include <math.h>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef COMPILER_MSVC
# include <float.h>
/* isinf() & isnan() are C99 standards, which older MSVC doesn't provide */
# define ISINF(val) !((bool)_finite((double)val))
# define ISNAN(val) (bool)_isnan((double)val)
#else
# define ISINF(val) std::isinf((val))
# define ISNAN(val) std::isnan((val))
#endif

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>

#include "gtkmm2ext/utils.h"

#include "ardour/audio_buffer.h"
#include "ardour/data_type.h"
#include "ardour/chan_mapping.h"
#include "ardour/plugin_insert.h"
#include "ardour/session.h"

#include "plugin_eq_gui.h"
#include "fft.h"
#include "ardour_ui.h"
#include "gui_thread.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

PluginEqGui::PluginEqGui (boost::shared_ptr<ARDOUR::PluginInsert> pluginInsert)
	: _min_dB (-12.0)
	, _max_dB (+12.0)
	, _step_dB (3.0)
	, _block_size (0)
	, _buffer_size (0)
	, _signal_buffer_size (0)
	, _impulse_fft (0)
	, _signal_input_fft (0)
	, _signal_output_fft (0)
	, _plugin_insert (pluginInsert)
	, _pointer_in_area_xpos (-1)
{
	_signal_analysis_running = false;
	_samplerate = ARDOUR_UI::instance()->the_session()->sample_rate();

	_log_coeff = (1.0 - 2.0 * (1000.0 / (_samplerate / 2.0))) / powf (1000.0 / (_samplerate / 2.0), 2.0);
	_log_max = log10f (1 + _log_coeff);

	// Setup analysis drawing area
	_analysis_scale_surface = 0;

	_analysis_area = new Gtk::DrawingArea();
	_analysis_width = 256.0;
	_analysis_height = 256.0;
	_analysis_area->set_size_request (_analysis_width, _analysis_height);

	_analysis_area->add_events (Gdk::POINTER_MOTION_MASK | Gdk::LEAVE_NOTIFY_MASK | Gdk::BUTTON_PRESS_MASK);

	_analysis_area->signal_expose_event().connect (sigc::mem_fun (*this, &PluginEqGui::expose_analysis_area));
	_analysis_area->signal_size_allocate().connect (sigc::mem_fun (*this, &PluginEqGui::resize_analysis_area));
	_analysis_area->signal_motion_notify_event().connect (sigc::mem_fun (*this, &PluginEqGui::analysis_area_mouseover));
	_analysis_area->signal_leave_notify_event().connect (sigc::mem_fun (*this, &PluginEqGui::analysis_area_mouseexit));

	// dB selection
	dBScaleModel = Gtk::ListStore::create (dBColumns);

	dBScaleCombo = new Gtk::ComboBox (dBScaleModel, false);

#define ADD_DB_ROW(MIN,MAX,STEP,NAME) \
	{ \
		Gtk::TreeModel::Row row = *(dBScaleModel->append()); \
		row[dBColumns.dBMin]  = (MIN); \
		row[dBColumns.dBMax]  = (MAX); \
		row[dBColumns.dBStep] = (STEP); \
		row[dBColumns.name]   = NAME; \
	}

	ADD_DB_ROW( -6,  +6, 1, "-6dB .. +6dB");
	ADD_DB_ROW(-12, +12, 3, "-12dB .. +12dB");
	ADD_DB_ROW(-24, +24, 5, "-24dB .. +24dB");
	ADD_DB_ROW(-36, +36, 6, "-36dB .. +36dB");
	ADD_DB_ROW(-64, +64,12, "-64dB .. +64dB");

#undef ADD_DB_ROW

	dBScaleCombo -> pack_start(dBColumns.name);
	dBScaleCombo -> set_active(1);

	dBScaleCombo -> signal_changed().connect (sigc::mem_fun(*this, &PluginEqGui::change_dB_scale));

	Gtk::Label *dBComboLabel = new Gtk::Label (_("Range:"));

	Gtk::HBox *dBSelectBin = new Gtk::HBox (false, 4);
	dBSelectBin->add (*manage(dBComboLabel));
	dBSelectBin->add (*manage(dBScaleCombo));

	_live_signal_combo = new Gtk::ComboBoxText ();
	_live_signal_combo->append_text (_("Off"));
	_live_signal_combo->append_text (_("Output / Input"));
	_live_signal_combo->append_text (_("Input"));
	_live_signal_combo->append_text (_("Output"));
	_live_signal_combo->append_text (_("Input +40dB"));
	_live_signal_combo->append_text (_("Output +40dB"));
	_live_signal_combo->set_active (0);

	Gtk::Label *live_signal_label = new Gtk::Label (_("Live signal:"));

	Gtk::HBox *liveSelectBin = new Gtk::HBox (false, 4);
	liveSelectBin->add (*manage(live_signal_label));
	liveSelectBin->add (*manage(_live_signal_combo));

	// Phase checkbutton
	_phase_button = new Gtk::CheckButton (_("Show phase"));
	_phase_button->set_active (true);
	_phase_button->signal_toggled().connect (sigc::mem_fun(*this, &PluginEqGui::redraw_scales));

	// Freq/dB info for mouse over
	_pointer_info = new Gtk::Label ("", 1, 0.5);
	_pointer_info->set_name ("PluginAnalysisInfoLabel");
	Gtkmm2ext::set_size_request_to_display_given_text (*_pointer_info, "10.0kHz_000.0dB_180.0\u00B0", 0, 0);

	// populate table
	attach (*manage(_analysis_area), 0, 4, 0, 1);
	attach (*manage(dBSelectBin),    0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
	attach (*manage(liveSelectBin),  1, 2, 1, 2, Gtk::SHRINK, Gtk::SHRINK, 4, 0);
	attach (*manage(_phase_button),  2, 3, 1, 2, Gtk::SHRINK, Gtk::SHRINK, 4, 0);
	attach (*manage(_pointer_info),  3, 4, 1, 2, Gtk::FILL,   Gtk::SHRINK);
}

PluginEqGui::~PluginEqGui ()
{
	stop_updating ();
	stop_listening ();

	if (_analysis_scale_surface) {
		cairo_surface_destroy (_analysis_scale_surface);
	}

	delete _impulse_fft;
	_impulse_fft = 0;
	delete _signal_input_fft;
	_signal_input_fft = 0;
	delete _signal_output_fft;
	_signal_output_fft = 0;

	// all gui objects are *manage'd by the inherited Table object
}

static inline float
power_to_dB (float a)
{
	return 10.0 * log10f (a);
}

void
PluginEqGui::start_listening ()
{
	if (!_plugin) {
		_plugin = _plugin_insert->get_impulse_analysis_plugin ();
	}

	_plugin->activate ();
	set_buffer_size (8192, 16384);
	_block_size = 0; // re-initialize the plugin next time.

	/* Connect the realtime signal collection callback */
	_plugin_insert->AnalysisDataGathered.connect (analysis_connection, invalidator (*this), boost::bind (&PluginEqGui::signal_collect_callback, this, _1, _2), gui_context());
}

void
PluginEqGui::stop_listening ()
{
	analysis_connection.disconnect ();
	if (_plugin) {
		_plugin->deactivate ();
		_plugin->drop_references ();
		_plugin.reset ();
	}
}

void
PluginEqGui::on_hide ()
{
	stop_updating ();
	stop_listening ();
	Gtk::Table::on_hide ();
}

void
PluginEqGui::stop_updating ()
{
	if (_update_connection.connected ()) {
		_update_connection.disconnect ();
	}
	_signal_analysis_running = false;
}

void
PluginEqGui::start_updating ()
{
	if (!_update_connection.connected() && is_visible()) {
		_update_connection = Glib::signal_timeout().connect (sigc::mem_fun (this, &PluginEqGui::timeout_callback), 250, Glib::PRIORITY_DEFAULT_IDLE);
	}
}

void
PluginEqGui::on_show ()
{
	Gtk::Table::on_show ();

	start_updating ();
	start_listening ();

	Gtk::Widget *toplevel = get_toplevel ();
	if (toplevel) {
		if (!_window_unmap_connection.connected ()) {
			_window_unmap_connection = toplevel->signal_unmap().connect (sigc::mem_fun (this, &PluginEqGui::stop_updating));
		}

		if (!_window_map_connection.connected ()) {
			_window_map_connection = toplevel->signal_map().connect (sigc::mem_fun (this, &PluginEqGui::start_updating));
		}
	}
}

void
PluginEqGui::change_dB_scale ()
{
	Gtk::TreeModel::iterator iter = dBScaleCombo -> get_active ();

	Gtk::TreeModel::Row row;

	if (iter && (row = *iter)) {
		_min_dB = row[dBColumns.dBMin];
		_max_dB = row[dBColumns.dBMax];
		_step_dB = row[dBColumns.dBStep];

		redraw_scales ();
	}
}

void
PluginEqGui::redraw_scales ()
{

	if (_analysis_scale_surface) {
		cairo_surface_destroy (_analysis_scale_surface);
		_analysis_scale_surface = 0;
	}

	_analysis_area->queue_draw ();

	// TODO: Add graph legend!
}

void
PluginEqGui::set_buffer_size (uint32_t size, uint32_t signal_size)
{
	if (_buffer_size == size && _signal_buffer_size == signal_size) {
		return;
	}

	GTKArdour::FFT *tmp1 = _impulse_fft;
	GTKArdour::FFT *tmp2 = _signal_input_fft;
	GTKArdour::FFT *tmp3 = _signal_output_fft;

	try {
		_impulse_fft       = new GTKArdour::FFT (size);
		_signal_input_fft  = new GTKArdour::FFT (signal_size);
		_signal_output_fft = new GTKArdour::FFT (signal_size);
	} catch (...) {
		// Don't care about lost memory, we're screwed anyhow
		_impulse_fft       = tmp1;
		_signal_input_fft  = tmp2;
		_signal_output_fft = tmp3;
		throw;
	}

	delete tmp1;
	delete tmp2;
	delete tmp3;

	_buffer_size = size;
	_signal_buffer_size = signal_size;

	/* allocate separate in+out buffers, VST cannot process in-place */
	ARDOUR::ChanCount acount (_plugin->get_info()->n_inputs + _plugin->get_info()->n_outputs);
	ARDOUR::ChanCount ccount = ARDOUR::ChanCount::max (_plugin->get_info()->n_inputs, _plugin->get_info()->n_outputs);

	for (ARDOUR::DataType::iterator i = ARDOUR::DataType::begin(); i != ARDOUR::DataType::end(); ++i) {
		_bufferset.ensure_buffers (*i, acount.get (*i), _buffer_size);
		_collect_bufferset.ensure_buffers (*i, ccount.get (*i), _buffer_size);
	}

	_bufferset.set_count (acount);
	_collect_bufferset.set_count (ccount);
}

void
PluginEqGui::resize_analysis_area (Gtk::Allocation& size)
{
	_analysis_width  = (float)size.get_width();
	_analysis_height = (float)size.get_height();

	if (_analysis_scale_surface) {
		cairo_surface_destroy (_analysis_scale_surface);
		_analysis_scale_surface = 0;
	}
}

bool
PluginEqGui::timeout_callback ()
{
	if (!_signal_analysis_running) {
		_signal_analysis_running = true;
		_plugin_insert -> collect_signal_for_analysis (_signal_buffer_size);
	}

	run_impulse_analysis ();
	return true;
}

void
PluginEqGui::signal_collect_callback (ARDOUR::BufferSet* in, ARDOUR::BufferSet* out)
{
	ENSURE_GUI_THREAD (*this, &PluginEqGui::signal_collect_callback, in, out);

	_signal_input_fft ->reset ();
	_signal_output_fft->reset ();

	for (uint32_t i = 0; i < _plugin_insert->input_streams().n_audio(); ++i) {
		_signal_input_fft ->analyze (in ->get_audio (i).data(), GTKArdour::FFT::HANN);
	}

	for (uint32_t i = 0; i < _plugin_insert->output_streams().n_audio(); ++i) {
		_signal_output_fft->analyze (out->get_audio (i).data(), GTKArdour::FFT::HANN);
	}

	_signal_input_fft ->calculate ();
	_signal_output_fft->calculate ();

	_signal_analysis_running = false;
	_analysis_area->queue_draw ();
}

void
PluginEqGui::run_impulse_analysis ()
{
	/* Allocate some thread-local buffers so that Plugin::connect_and_run can use them */
	ARDOUR_UI::instance()->get_process_buffers ();

	uint32_t inputs  = _plugin->get_info()->n_inputs.n_audio();
	uint32_t outputs = _plugin->get_info()->n_outputs.n_audio();

	/* Create the impulse, can't use silence() because consecutive calls won't work */
	for (uint32_t i = 0; i < inputs; ++i) {
		ARDOUR::AudioBuffer& buf = _bufferset.get_audio (i);
		ARDOUR::Sample* d = buf.data ();
		memset (d, 0, sizeof (ARDOUR::Sample) * _buffer_size);
		*d = 1.0;
	}

	/* Silence collect buffers to copy data to */
	for (uint32_t i = 0; i < outputs; ++i) {
		ARDOUR::AudioBuffer &buf = _collect_bufferset.get_audio (i);
		ARDOUR::Sample *d = buf.data ();
		memset (d, 0, sizeof (ARDOUR::Sample) * _buffer_size);
	}

	/* create default linear I/O maps */
	ARDOUR::ChanMapping in_map (_plugin->get_info()->n_inputs);
	ARDOUR::ChanMapping out_map (_plugin->get_info()->n_outputs);
	/* map output buffers after input buffers (no inplace for VST) */
	out_map.offset_to (DataType::AUDIO, inputs);

	/* run at most at session's block size chunks.
	 *
	 * This is important since VSTs may call audioMasterGetBlockSize
	 * or access various other /real/ session parameters using the
	 * audioMasterCallback
	 */
	samplecnt_t block_size = ARDOUR_UI::instance()->the_session()->get_block_size();
	if (_block_size != block_size) {
		_block_size = block_size;
		_plugin->set_block_size (block_size);
	}

	samplepos_t sample_pos = 0;
	samplecnt_t latency = _plugin_insert->effective_latency ();
	samplecnt_t samples_remain = _buffer_size + latency;

	/* Note: https://discourse.ardour.org/t/plugins-ladspa-questions/101292/15
	 * Capture the complete response from the beginning, and more than "latency" samples,
	 * Then unwrap the phase-response corresponding to reported latency, leaving the
	 * magnitude unchanged.
	 */

	_impulse_fft->reset ();

	while (samples_remain > 0) {

		samplecnt_t n_samples = std::min (samples_remain, block_size);
		_plugin->connect_and_run (_bufferset, sample_pos, sample_pos + n_samples, 1.0, in_map, out_map, n_samples, 0);
		samples_remain -= n_samples;

		/* zero input buffers */
		if (sample_pos == 0 && samples_remain > 0) {
			for (uint32_t i = 0; i < inputs; ++i) {
				_bufferset.get_audio (i).data()[0] = 0.f;
			}
		}

#ifndef NDEBUG
		if (samples_remain > 0) {
			for (uint32_t i = 0; i < inputs; ++i) {
				pframes_t unused;
				assert (_bufferset.get_audio (i).check_silence (block_size, unused));
			}
		}
#endif

		if (sample_pos + n_samples > latency) {
			samplecnt_t dst_off = sample_pos >= latency ? sample_pos - latency : 0;
			samplecnt_t src_off = sample_pos >= latency ? 0 : latency - sample_pos;
			samplecnt_t n_copy = std::min (n_samples, sample_pos + n_samples - latency);

			assert (dst_off + n_copy <= _buffer_size);
			assert (src_off + n_copy <= _block_size);

			for (uint32_t i = 0; i < outputs; ++i) {
				memcpy (
						&(_collect_bufferset.get_audio (i).data()[dst_off]),
						&(_bufferset.get_audio (inputs + i).data()[src_off]),
						n_copy * sizeof (float));
			}
		}

		sample_pos += n_samples;
	}

	for (uint32_t i = 0; i < outputs; ++i) {
		_impulse_fft->analyze (_collect_bufferset.get_audio (i).data());
	}
	_impulse_fft->calculate ();

	_analysis_area->queue_draw ();

	ARDOUR_UI::instance ()->drop_process_buffers ();
}

void
PluginEqGui::update_pointer_info( float x)
{
	/* find the bin corresponding to x (see plot_impulse_amplitude) */
	int i = roundf ((powf (10, _log_max * x / _analysis_width) - 1.0) * _impulse_fft->bins() / _log_coeff);
	float dB = power_to_dB (_impulse_fft->power_at_bin (i));
	/* calc freq corresponding to bin */
	const int freq = std::max (1, (int) roundf ((float)i / (float)_impulse_fft->bins() * _samplerate / 2.f));

	_pointer_in_area_freq = round (_analysis_width * log10f (1.0 + (float)i / (float)_impulse_fft->bins() * _log_coeff) / _log_max);

	std::stringstream ss;
	ss << std::fixed;
	if (freq >= 10000) {
		ss <<  std::setprecision (1) << freq / 1000.0 << "kHz";
	} else if (freq >= 1000) {
		ss <<  std::setprecision (2) << freq / 1000.0 << "kHz";
	} else {
		ss <<  std::setprecision (0) << freq << "Hz";
	}
	ss << " " << std::setw (6) << std::setprecision (1) << std::showpos << dB;
	ss << std::setw (0) << "dB";

	if (_phase_button->get_active ()) {
		float phase = 180. * _impulse_fft->phase_at_bin (i) / M_PI;
		ss << " " << std::setw (6) << std::setprecision (1) << std::showpos << phase;
		ss << std::setw (0) << "\u00B0";
	}
	_pointer_info->set_text (ss.str());
}

bool
PluginEqGui::analysis_area_mouseover (GdkEventMotion *event)
{
	update_pointer_info (event->x);
	_pointer_in_area_xpos = event->x;
	_analysis_area->queue_draw ();
	return true;
}

bool
PluginEqGui::analysis_area_mouseexit (GdkEventCrossing *)
{
	_pointer_info->set_text ("");
	_pointer_in_area_xpos = -1;
	_analysis_area->queue_draw ();
	return true;
}

bool
PluginEqGui::expose_analysis_area (GdkEventExpose *)
{
	redraw_analysis_area ();
	return true;
}

void
PluginEqGui::draw_analysis_scales (cairo_t *ref_cr)
{
	// TODO: check whether we need rounding
	_analysis_scale_surface = cairo_surface_create_similar (cairo_get_target (ref_cr),
			CAIRO_CONTENT_COLOR,
			_analysis_width,
			_analysis_height);

	cairo_t *cr = cairo_create (_analysis_scale_surface);

	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_rectangle (cr, 0.0, 0.0, _analysis_width, _analysis_height);
	cairo_fill (cr);

	draw_scales_power (_analysis_area, cr);

	if (_phase_button->get_active ()) {
		draw_scales_phase (_analysis_area, cr);
	}

	cairo_destroy (cr);
}

void
PluginEqGui::redraw_analysis_area ()
{
	cairo_t *cr;

	cr = gdk_cairo_create (GDK_DRAWABLE(_analysis_area->get_window()->gobj()));

	if (_analysis_scale_surface == 0) {
		draw_analysis_scales (cr);
	}

	cairo_copy_page (cr);

	cairo_set_source_surface (cr, _analysis_scale_surface, 0.0, 0.0);
	cairo_paint (cr);

	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);

	if (_phase_button->get_active()) {
		plot_impulse_phase (_analysis_area, cr);
	}

	plot_impulse_amplitude (_analysis_area, cr);

	if (_pointer_in_area_xpos >= 0) {
		update_pointer_info (_pointer_in_area_xpos);
	}

	if (_live_signal_combo->get_active_row_number() > 0) {
		plot_signal_amplitude_difference (_analysis_area, cr);
	}

	if (_pointer_in_area_xpos >= 0 && _pointer_in_area_freq > 0) {
		const double dashed[] = {0.0, 2.0};
		cairo_set_dash (cr, dashed, 2, 0);
		cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		cairo_set_line_width (cr, 1.0);
		cairo_move_to (cr, _pointer_in_area_freq - .5, -.5);
		cairo_line_to (cr, _pointer_in_area_freq - .5, _analysis_height - .5);
		cairo_stroke (cr);
	}

	cairo_destroy (cr);
}

#define PHASE_PROPORTION 0.5

void
PluginEqGui::draw_scales_phase (Gtk::Widget*, cairo_t *cr)
{
	float y;
	cairo_font_extents_t extents;
	cairo_font_extents (cr, &extents);

	char buf[256];
	cairo_text_extents_t t_ext;

	for (uint32_t i = 0; i < 5; i++) {

		y = _analysis_height / 2.0 - (float)i * (_analysis_height / 8.0) * PHASE_PROPORTION;

		cairo_set_source_rgb (cr, .8, .9, 0.2);
		if (i == 0) {
			snprintf (buf,256, "0\u00b0");
		} else {
			snprintf (buf,256, "%d\u00b0", (i * 45));
		}
		cairo_text_extents (cr, buf, &t_ext);
		cairo_move_to (cr, _analysis_width - t_ext.width - t_ext.x_bearing - 2.0, y - extents.descent);
		cairo_show_text (cr, buf);

		if (i == 0) {
			continue;
		}

		y = roundf (y) - .5;

		cairo_set_source_rgba (cr, .8, .9, .2, 0.4);
		cairo_move_to (cr, 0.0,             y);
		cairo_line_to (cr, _analysis_width, y);
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);

		y = _analysis_height / 2.0 + (float)i * (_analysis_height / 8.0) * PHASE_PROPORTION;

		// label
		snprintf (buf,256, "-%d\u00b0", (i * 45));
		cairo_set_source_rgb (cr, .8, .9, 0.2);
		cairo_text_extents (cr, buf, &t_ext);
		cairo_move_to (cr, _analysis_width - t_ext.width - t_ext.x_bearing - 2.0, y - extents.descent);
		cairo_show_text (cr, buf);

		y = roundf (y) - .5;
		// line
		cairo_set_source_rgba (cr, .8, .9, .2, 0.4);
		cairo_move_to (cr, 0.0,             y);
		cairo_line_to (cr, _analysis_width, y);

		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);
	}
}

void
PluginEqGui::plot_impulse_phase (Gtk::Widget *w, cairo_t *cr)
{
	float x,y;

	int prevX = 0;
	float avgY = 0.0;
	int avgNum = 0;

	// float width  = w->get_width();
	float height = w->get_height ();
	float analysis_height_2 = _analysis_height / 2.f;

	cairo_set_source_rgba (cr, 0.95, 0.3, 0.2, 1.0);
	for (uint32_t i = 0; i < _impulse_fft->bins() - 1; ++i) {
		// x coordinate of bin i
		x  = log10f (1.0 + (float)i / (float)_impulse_fft->bins() * _log_coeff) / _log_max;
		x *= _analysis_width;
		y  = analysis_height_2 - (_impulse_fft->phase_at_bin (i) / M_PI) * analysis_height_2 * PHASE_PROPORTION;

		if (i == 0) {
			cairo_move_to (cr, x, y);
			avgY = 0;
			avgNum = 0;
		} else if (rint (x) > prevX || i == _impulse_fft->bins() - 1) {
			avgY = avgY / (float)avgNum;
			if (avgY > (height * 10.0)) {
				avgY = height * 10.0;
			}
			if (avgY < (-height * 10.0)) {
				avgY = -height * 10.0;
			}

			cairo_line_to (cr, prevX, avgY);

			avgY = 0;
			avgNum = 0;
		}

		prevX = rint (x);
		avgY += y;
		avgNum++;
	}

	cairo_set_line_width (cr, 2.0);
	cairo_stroke (cr);
}

void
PluginEqGui::draw_scales_power (Gtk::Widget */*w*/, cairo_t *cr)
{
	if (_impulse_fft == 0) {
		return;
	}

	static float scales[] = { 30.0, 70.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 15000.0, 20000.0, -1.0 };
	float divisor = _samplerate / 2.0 / _impulse_fft->bins();
	float x;

	cairo_set_line_width (cr, 1.5);
	cairo_set_font_size (cr, 9);

	cairo_font_extents_t extents;
	cairo_font_extents (cr, &extents);
	// float fontXOffset = extents.descent + 1.0;

	char buf[256];

	for (uint32_t i = 0; scales[i] != -1.0; ++i) {
		float bin = scales[i] / divisor;

		x  = log10f (1.0 + bin / (float)_impulse_fft->bins() * _log_coeff) / _log_max;
		x *= _analysis_width;

		if (scales[i] < 1000.0) {
			snprintf (buf, 256, "%0.0f", scales[i]);
		} else {
			snprintf (buf, 256, "%0.0fk", scales[i]/1000.0);
		}

		cairo_set_source_rgb (cr, 0.4, 0.4, 0.4);

		cairo_move_to (cr, x - extents.height, 3.0);

		cairo_rotate (cr, M_PI / 2.0);
		cairo_show_text (cr, buf);
		cairo_rotate (cr, -M_PI / 2.0);
		cairo_stroke (cr);

		cairo_set_source_rgb (cr, 0.3, 0.3, 0.3);
		cairo_move_to (cr, x, _analysis_height);
		cairo_line_to (cr, x, 0.0);
		cairo_stroke (cr);
	}

	float y;

	//double dashes[] = { 1.0, 3.0, 4.5, 3.0 };
	double dashes[] = { 3.0, 5.0 };

	for (float dB = 0.0; dB < _max_dB; dB += _step_dB) {
		snprintf (buf, 256, "+%0.0f", dB);

		y  = (_max_dB - dB) / (_max_dB - _min_dB);
		//std::cerr << " y = " << y << std::endl;
		y *= _analysis_height;

		if (dB != 0.0) {
			cairo_set_source_rgb (cr, 0.4, 0.4, 0.4);
			cairo_move_to (cr, 1.0,     y + extents.height + 1.0);
			cairo_show_text (cr, buf);
			cairo_stroke (cr);
		}

		cairo_set_source_rgb (cr, 0.2, 0.2, 0.2);
		cairo_move_to (cr, 0,               y);
		cairo_line_to (cr, _analysis_width, y);
		cairo_stroke (cr);

		if (dB == 0.0) {
			cairo_set_dash (cr, dashes, 2, 0.0);
		}
	}

	for (float dB = - _step_dB; dB > _min_dB; dB -= _step_dB) {
		snprintf (buf, 256, "%0.0f", dB);

		y  = (_max_dB - dB) / (_max_dB - _min_dB);
		y *= _analysis_height;

		cairo_set_source_rgb (cr, 0.4, 0.4, 0.4);
		cairo_move_to (cr, 1.0, y - extents.descent - 1.0);
		cairo_show_text (cr, buf);
		cairo_stroke (cr);

		cairo_set_source_rgb (cr, 0.2, 0.2, 0.2);
		cairo_move_to (cr, 0,               y);
		cairo_line_to (cr, _analysis_width, y);
		cairo_stroke (cr);
	}

	cairo_set_dash (cr, 0, 0, 0.0);
}

void
PluginEqGui::plot_impulse_amplitude (Gtk::Widget *w, cairo_t *cr)
{
	float x,y;
	int prevX = 0;
	float avgY = 0.0;
	int avgNum = 0;

	// float width  = w->get_width();
	float height = w->get_height ();

	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	cairo_set_line_width (cr, 2.5);

	for (uint32_t i = 0; i < _impulse_fft->bins() - 1; ++i) {
		// x coordinate of bin i
		x  = log10f (1.0 + (float)i / (float)_impulse_fft->bins() * _log_coeff) / _log_max;
		x *= _analysis_width;

		float yCoeff = (power_to_dB (_impulse_fft->power_at_bin (i)) - _min_dB) / (_max_dB - _min_dB);

		y = _analysis_height - _analysis_height * yCoeff;

		if (i == 0) {
			cairo_move_to (cr, x, y);
			avgY = 0;
			avgNum = 0;
		} else if (rint (x) > prevX || i == _impulse_fft->bins() - 1) {
			avgY = avgY / (float)avgNum;
			if (avgY > (height * 10.0)) {
				avgY = height * 10.0;
			}
			if (avgY < (-height * 10.0)) {
				avgY = -height * 10.0;
			}
			cairo_line_to (cr, prevX, avgY);

			avgY = 0;
			avgNum = 0;
		}

		prevX = rint (x);
		avgY += y;
		avgNum++;
	}

	cairo_stroke (cr);
}

void
PluginEqGui::plot_signal_amplitude_difference (Gtk::Widget *w, cairo_t *cr)
{
	float x,y;

	int prevX = 0;
	float avgY = 0.0;
	int avgNum = 0;

	float height = w->get_height();

	cairo_set_source_rgb (cr, 0.0, 1.0, 0.0);
	cairo_set_line_width (cr, 1.5);

	for (uint32_t i = 0; i < _signal_input_fft->bins() - 1; ++i) {
		// x coordinate of bin i
		x  = log10f (1.0 + (float)i / (float)_signal_input_fft->bins() * _log_coeff) / _log_max;
		x *= _analysis_width;

		float power_out = _signal_output_fft->power_at_bin (i) + 1e-30;
		float power_in  = _signal_input_fft ->power_at_bin (i) + 1e-30;
		float power;
		switch (_live_signal_combo->get_active_row_number()) {
			case 2:
				power = power_to_dB (power_in);
				break;
			case 3:
				power = power_to_dB (power_out);
				break;
			case 4:
				power = power_to_dB (power_in) + 40;
				break;
			case 5:
				power = power_to_dB (power_out) + 40;
				break;
			default:
				power = power_to_dB (power_out / power_in);
				break;
		}

		assert (!ISINF(power));
		assert (!ISNAN(power));

		float yCoeff = (power - _min_dB) / (_max_dB - _min_dB);

		y = _analysis_height - _analysis_height*yCoeff;

		if (i == 0) {
			cairo_move_to (cr, x, y);

			avgY = 0;
			avgNum = 0;
		} else if (rint (x) > prevX || i == _impulse_fft->bins() - 1) {
			avgY = avgY / (float)avgNum;
			if (avgY > (height * 10.0)) {
				avgY = height * 10.0;
			}
			if (avgY < (-height * 10.0)) {
				avgY = -height * 10.0;
			}
			cairo_line_to (cr, prevX, avgY);

			avgY = 0;
			avgNum = 0;

		}

		prevX = rint (x);
		avgY += y;
		avgNum++;
	}

	cairo_stroke (cr);
}
