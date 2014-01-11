/*
    Copyright (C) 2008 Paul Davis
    Author: Sampo Savolainen

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
#include <cmath>

#ifdef COMPILER_MSVC
#include <float.h>
/* isinf() & isnan() are C99 standards, which older MSVC doesn't provide */
#define isinf(val) !((bool)_finite((double)val))
#define isnan(val) (bool)_isnan((double)val)
#endif

#ifdef __APPLE__
#define isinf(val) std::isinf((val))
#define isnan(val) std::isnan((val))
#endif

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>

#include "ardour/audio_buffer.h"
#include "ardour/data_type.h"
#include "ardour/chan_mapping.h"
#include "ardour/session.h"

#include "plugin_eq_gui.h"
#include "fft.h"
#include "ardour_ui.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;

PluginEqGui::PluginEqGui(boost::shared_ptr<ARDOUR::PluginInsert> pluginInsert)
	: _min_dB(-12.0)
	, _max_dB(+12.0)
	, _step_dB(3.0)
	, _impulse_fft(0)
	, _signal_input_fft(0)
	, _signal_output_fft(0)
	, _plugin_insert(pluginInsert)
{
	_signal_analysis_running = false;
	_samplerate = ARDOUR_UI::instance()->the_session()->frame_rate();

	_log_coeff = (1.0 - 2.0 * (1000.0/(_samplerate/2.0))) / powf(1000.0/(_samplerate/2.0), 2.0);
	_log_max = log10f(1 + _log_coeff);

	// Setup analysis drawing area
	_analysis_scale_surface = 0;

	_analysis_area = new Gtk::DrawingArea();
	_analysis_width = 256.0;
	_analysis_height = 256.0;
	_analysis_area->set_size_request(_analysis_width, _analysis_height);

	_analysis_area->signal_expose_event().connect( sigc::mem_fun (*this, &PluginEqGui::expose_analysis_area));
	_analysis_area->signal_size_allocate().connect( sigc::mem_fun (*this, &PluginEqGui::resize_analysis_area));

	// dB selection
	dBScaleModel = Gtk::ListStore::create(dBColumns);

	/* this grotty-looking cast allows compilation against gtkmm 2.24.0, which
	   added a new ComboBox constructor.
	*/
	dBScaleCombo = new Gtk::ComboBox ((Glib::RefPtr<Gtk::TreeModel> &) dBScaleModel);
	dBScaleCombo->set_title (_("dB scale"));

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

	dBScaleCombo -> signal_changed().connect( sigc::mem_fun(*this, &PluginEqGui::change_dB_scale) );

	Gtk::Label *dBComboLabel = new Gtk::Label (_("dB scale"));

	Gtk::HBox *dBSelectBin = new Gtk::HBox(false, 5);
	dBSelectBin->add( *manage(dBComboLabel));
	dBSelectBin->add( *manage(dBScaleCombo));

	// Phase checkbutton
	_phase_button = new Gtk::CheckButton (_("Show phase"));
	_phase_button->set_active(true);
	_phase_button->signal_toggled().connect( sigc::mem_fun(*this, &PluginEqGui::redraw_scales));

	// populate table
	attach( *manage(_analysis_area), 1, 3, 1, 2);
	attach( *manage(dBSelectBin),    1, 2, 2, 3, Gtk::SHRINK, Gtk::SHRINK);
	attach( *manage(_phase_button),	 2, 3, 2, 3, Gtk::SHRINK, Gtk::SHRINK);
}

PluginEqGui::~PluginEqGui()
{
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

void
PluginEqGui::start_listening ()
{
	if (!_plugin) {
		_plugin = _plugin_insert->get_impulse_analysis_plugin();
	}

	_plugin->activate();
	set_buffer_size(4096, 16384);
	// Connect the realtime signal collection callback
	_plugin_insert->AnalysisDataGathered.connect (analysis_connection, invalidator (*this), boost::bind (&PluginEqGui::signal_collect_callback, this, _1, _2), gui_context());
}

void
PluginEqGui::stop_listening ()
{
	analysis_connection.disconnect ();
	_plugin->deactivate ();
}

void
PluginEqGui::on_hide()
{
	stop_updating();
	Gtk::Table::on_hide();
}

void
PluginEqGui::stop_updating()
{
	if (_update_connection.connected()) {
		_update_connection.disconnect();
	}
}

void
PluginEqGui::start_updating()
{
	if (!_update_connection.connected() && is_visible()) {
		_update_connection = Glib::signal_timeout().connect( sigc::mem_fun(this, &PluginEqGui::timeout_callback), 250);
	}
}

void
PluginEqGui::on_show()
{
	Gtk::Table::on_show();

	start_updating();

	Gtk::Widget *toplevel = get_toplevel();
	if (toplevel) {
		if (!_window_unmap_connection.connected()) {
			_window_unmap_connection = toplevel->signal_unmap().connect( sigc::mem_fun(this, &PluginEqGui::stop_updating));
		}

		if (!_window_map_connection.connected()) {
			_window_map_connection = toplevel->signal_map().connect( sigc::mem_fun(this, &PluginEqGui::start_updating));
		}
	}
}

void
PluginEqGui::change_dB_scale()
{
	Gtk::TreeModel::iterator iter = dBScaleCombo -> get_active();

	Gtk::TreeModel::Row row;

	if(iter && (row = *iter)) {
		_min_dB = row[dBColumns.dBMin];
		_max_dB = row[dBColumns.dBMax];
		_step_dB = row[dBColumns.dBStep];


		redraw_scales();
	}
}

void
PluginEqGui::redraw_scales()
{

	if (_analysis_scale_surface) {
		cairo_surface_destroy (_analysis_scale_surface);
		_analysis_scale_surface = 0;
	}

	_analysis_area->queue_draw();

	// TODO: Add graph legend!
}

void
PluginEqGui::set_buffer_size(uint32_t size, uint32_t signal_size)
{
	if (_buffer_size == size && _signal_buffer_size == signal_size) {
		return;
	}

        GTKArdour::FFT *tmp1 = _impulse_fft;
        GTKArdour::FFT *tmp2 = _signal_input_fft;
        GTKArdour::FFT *tmp3 = _signal_output_fft;

	try {
		_impulse_fft       = new GTKArdour::FFT(size);
		_signal_input_fft  = new GTKArdour::FFT(signal_size);
		_signal_output_fft = new GTKArdour::FFT(signal_size);
	} catch( ... ) {
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

	ARDOUR::ChanCount count = ARDOUR::ChanCount::max (_plugin->get_info()->n_inputs, _plugin->get_info()->n_outputs);

	for (ARDOUR::DataType::iterator i = ARDOUR::DataType::begin(); i != ARDOUR::DataType::end(); ++i) {
		_bufferset.ensure_buffers (*i, count.get (*i), _buffer_size);
		_collect_bufferset.ensure_buffers (*i, count.get (*i), _buffer_size);
	}

	_bufferset.set_count (count);
	_collect_bufferset.set_count (count);
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
PluginEqGui::timeout_callback()
{
	if (!_signal_analysis_running) {
		_signal_analysis_running = true;
		_plugin_insert -> collect_signal_for_analysis(_signal_buffer_size);
	}
	run_impulse_analysis();

	return true;
}

void
PluginEqGui::signal_collect_callback(ARDOUR::BufferSet *in, ARDOUR::BufferSet *out)
{
	ENSURE_GUI_THREAD (*this, &PluginEqGui::signal_collect_callback, in, out)

	_signal_input_fft ->reset();
	_signal_output_fft->reset();

	for (uint32_t i = 0; i < _plugin_insert->input_streams().n_audio(); ++i) {
		_signal_input_fft ->analyze(in ->get_audio(i).data(), GTKArdour::FFT::HANN);
	}

	for (uint32_t i = 0; i < _plugin_insert->output_streams().n_audio(); ++i) {
		_signal_output_fft->analyze(out->get_audio(i).data(), GTKArdour::FFT::HANN);
	}

	_signal_input_fft ->calculate();
	_signal_output_fft->calculate();

	_signal_analysis_running = false;

	// This signals calls expose_analysis_area()
	_analysis_area->queue_draw();
}

void
PluginEqGui::run_impulse_analysis()
{
	/* Allocate some thread-local buffers so that Plugin::connect_and_run can use them */
	ARDOUR_UI::instance()->get_process_buffers ();
	
	uint32_t inputs  = _plugin->get_info()->n_inputs.n_audio();
	uint32_t outputs = _plugin->get_info()->n_outputs.n_audio();

	// Create the impulse, can't use silence() because consecutive calls won't work
	for (uint32_t i = 0; i < inputs; ++i) {
		ARDOUR::AudioBuffer& buf = _bufferset.get_audio(i);
		ARDOUR::Sample* d = buf.data();
		memset(d, 0, sizeof(ARDOUR::Sample)*_buffer_size);
		*d = 1.0;
	}

	ARDOUR::ChanMapping in_map(_plugin->get_info()->n_inputs);
	ARDOUR::ChanMapping out_map(_plugin->get_info()->n_outputs);

	_plugin->connect_and_run(_bufferset, in_map, out_map, _buffer_size, 0);
	framecnt_t f = _plugin->signal_latency ();
	// Adding user_latency() could be interesting

	// Gather all output, taking latency into account.
	_impulse_fft->reset();

	// Silence collect buffers to copy data to, can't use silence() because consecutive calls won't work
	for (uint32_t i = 0; i < outputs; ++i) {
		ARDOUR::AudioBuffer &buf = _collect_bufferset.get_audio(i);
		ARDOUR::Sample *d = buf.data();
		memset(d, 0, sizeof(ARDOUR::Sample)*_buffer_size);
	}

	if (f == 0) {
		//std::cerr << "0: no latency, copying full buffer, trivial.." << std::endl;
		for (uint32_t i = 0; i < outputs; ++i) {
			memcpy(_collect_bufferset.get_audio(i).data(),
			       _bufferset.get_audio(i).data(), _buffer_size * sizeof(float));
		}
	} else {
		//int C = 0;
		//std::cerr << (++C) << ": latency is " << f << " frames, doing split processing.." << std::endl;
		framecnt_t target_offset = 0;
		framecnt_t frames_left = _buffer_size; // refaktoroi
		do {
			if (f >= _buffer_size) {
				//std::cerr << (++C) << ": f (=" << f << ") is larger than buffer_size, still trying to reach the actual output" << std::endl;
				// there is no data in this buffer regarding to the input!
				f -= _buffer_size;
			} else {
				// this buffer contains either the first, last or a whole bu the output of the impulse
				// first part: offset is 0, so we copy to the start of _collect_bufferset
				//             we start at output offset "f"
				//             .. and copy "buffer size" - "f" - "offset" frames

				framecnt_t length = _buffer_size - f - target_offset;

				//std::cerr << (++C) << ": copying " << length << " frames to _collect_bufferset.get_audio(i)+" << target_offset << " from bufferset at offset " << f << std::endl;
				for (uint32_t i = 0; i < outputs; ++i) {
					memcpy(_collect_bufferset.get_audio(i).data(target_offset),
                                       		_bufferset.get_audio(i).data() + f,
                                       		length * sizeof(float));
				}

				target_offset += length;
				frames_left   -= length;
				f = 0;
			}
			if (frames_left > 0) {
				// Silence the buffers
				for (uint32_t i = 0; i < inputs; ++i) {
					ARDOUR::AudioBuffer &buf = _bufferset.get_audio(i);
					ARDOUR::Sample *d = buf.data();
					memset(d, 0, sizeof(ARDOUR::Sample)*_buffer_size);
				}

				in_map  = ARDOUR::ChanMapping(_plugin->get_info()->n_inputs);
				out_map = ARDOUR::ChanMapping(_plugin->get_info()->n_outputs);
				_plugin->connect_and_run(_bufferset, in_map, out_map, _buffer_size, 0);
			}
		} while ( frames_left > 0);

	}


	for (uint32_t i = 0; i < outputs; ++i) {
		_impulse_fft->analyze(_collect_bufferset.get_audio(i).data());
	}

	// normalize the output
	_impulse_fft->calculate();

	// This signals calls expose_analysis_area()
	_analysis_area->queue_draw();

	ARDOUR_UI::instance()->drop_process_buffers ();
}

bool
PluginEqGui::expose_analysis_area(GdkEventExpose *)
{
	redraw_analysis_area();
	return true;
}

void
PluginEqGui::draw_analysis_scales(cairo_t *ref_cr)
{
	// TODO: check whether we need rounding
	_analysis_scale_surface = cairo_surface_create_similar(cairo_get_target(ref_cr),
							     CAIRO_CONTENT_COLOR,
							     _analysis_width,
							     _analysis_height);

	cairo_t *cr = cairo_create (_analysis_scale_surface);

        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_rectangle(cr, 0.0, 0.0, _analysis_width, _analysis_height);
        cairo_fill(cr);


	draw_scales_power(_analysis_area, cr);
	if (_phase_button->get_active()) {
		draw_scales_phase(_analysis_area, cr);
	}

        cairo_destroy(cr);

}

void
PluginEqGui::redraw_analysis_area()
{
	cairo_t *cr;

        cr = gdk_cairo_create(GDK_DRAWABLE(_analysis_area->get_window()->gobj()));

	if (_analysis_scale_surface == 0) {
		draw_analysis_scales(cr);
	}


	cairo_copy_page(cr);

	cairo_set_source_surface(cr, _analysis_scale_surface, 0.0, 0.0);
	cairo_paint(cr);

	if (_phase_button->get_active()) {
		plot_impulse_phase(_analysis_area, cr);
	}
	plot_impulse_amplitude(_analysis_area, cr);

	// TODO: make this optional
	plot_signal_amplitude_difference(_analysis_area, cr);

        cairo_destroy(cr);


}

#define PHASE_PROPORTION 0.5

void
PluginEqGui::draw_scales_phase(Gtk::Widget */*w*/, cairo_t *cr)
{
	float y;
	cairo_font_extents_t extents;
	cairo_font_extents(cr, &extents);

	char buf[256];
	cairo_text_extents_t t_ext;

	for (uint32_t i = 0; i < 3; i++) {

		y = _analysis_height/2.0 - (float)i*(_analysis_height/8.0)*PHASE_PROPORTION;

        	cairo_set_source_rgb(cr, .8, .9, 0.2);
		if (i == 0) {
			snprintf(buf,256, "0\u00b0");
		} else {
			snprintf(buf,256, "%d\u00b0", (i * 45));
		}
		cairo_text_extents(cr, buf, &t_ext);
		cairo_move_to(cr, _analysis_width - t_ext.width - t_ext.x_bearing - 2.0, y - extents.descent);
		cairo_show_text(cr, buf);

		if (i == 0)
			continue;


        	cairo_set_source_rgba(cr, .8, .9, 0.2, 0.6/(float)i);
		cairo_move_to(cr, 0.0,            y);
		cairo_line_to(cr, _analysis_width, y);


		y = _analysis_height/2.0 + (float)i*(_analysis_height/8.0)*PHASE_PROPORTION;

		// label
		snprintf(buf,256, "-%d\u00b0", (i * 45));
        	cairo_set_source_rgb(cr, .8, .9, 0.2);
		cairo_text_extents(cr, buf, &t_ext);
		cairo_move_to(cr, _analysis_width - t_ext.width - t_ext.x_bearing - 2.0, y - extents.descent);
		cairo_show_text(cr, buf);

		// line
        	cairo_set_source_rgba(cr, .8, .9, 0.2, 0.6/(float)i);
		cairo_move_to(cr, 0.0,            y);
		cairo_line_to(cr, _analysis_width, y);

		cairo_set_line_width (cr, 0.25 + 1.0/(float)(i+1));
		cairo_stroke(cr);
	}
}

void
PluginEqGui::plot_impulse_phase(Gtk::Widget *w, cairo_t *cr)
{
	float x,y;

	int prevX = 0;
	float avgY = 0.0;
	int avgNum = 0;

	// float width  = w->get_width();
	float height = w->get_height();

        cairo_set_source_rgba(cr, 0.95, 0.3, 0.2, 1.0);
	for (uint32_t i = 0; i < _impulse_fft->bins()-1; i++) {
		// x coordinate of bin i
		x  = log10f(1.0 + (float)i / (float)_impulse_fft->bins() * _log_coeff) / _log_max;
		x *= _analysis_width;

		y  = _analysis_height/2.0 - (_impulse_fft->phase_at_bin(i)/M_PI)*(_analysis_height/2.0)*PHASE_PROPORTION;

		if ( i == 0 ) {
			cairo_move_to(cr, x, y);

			avgY = 0;
			avgNum = 0;
		} else if (rint(x) > prevX || i == _impulse_fft->bins()-1 ) {
			avgY = avgY/(float)avgNum;
			if (avgY > (height * 10.0) ) avgY = height * 10.0;
			if (avgY < (-height * 10.0) ) avgY = -height * 10.0;
			cairo_line_to(cr, prevX, avgY);
			//cairo_line_to(cr, prevX, avgY/(float)avgNum);

			avgY = 0;
			avgNum = 0;

		}

		prevX = rint(x);
		avgY += y;
		avgNum++;
	}

	cairo_set_line_width (cr, 2.0);
	cairo_stroke(cr);
}

void
PluginEqGui::draw_scales_power(Gtk::Widget */*w*/, cairo_t *cr)
{
	if (_impulse_fft == 0) {
		return;
	}

	static float scales[] = { 30.0, 70.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 15000.0, 20000.0, -1.0 };
	float divisor = _samplerate / 2.0 / _impulse_fft->bins();
	float x;

	cairo_set_line_width (cr, 1.5);
	cairo_set_font_size(cr, 9);

	cairo_font_extents_t extents;
	cairo_font_extents(cr, &extents);
	// float fontXOffset = extents.descent + 1.0;

	char buf[256];

	for (uint32_t i = 0; scales[i] != -1.0; ++i) {
		float bin = scales[i] / divisor;

		x  = log10f(1.0 + bin / (float)_impulse_fft->bins() * _log_coeff) / _log_max;
		x *= _analysis_width;

		if (scales[i] < 1000.0) {
			snprintf(buf, 256, "%0.0f", scales[i]);
		} else {
			snprintf(buf, 256, "%0.0fk", scales[i]/1000.0);
		}

		cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);

		//cairo_move_to(cr, x + fontXOffset, 3.0);
		cairo_move_to(cr, x - extents.height, 3.0);

		cairo_rotate(cr, M_PI / 2.0);
		cairo_show_text(cr, buf);
		cairo_rotate(cr, -M_PI / 2.0);
		cairo_stroke(cr);

		cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
		cairo_move_to(cr, x, _analysis_height);
		cairo_line_to(cr, x, 0.0);
		cairo_stroke(cr);
	}

	float y;

	//double dashes[] = { 1.0, 3.0, 4.5, 3.0 };
	double dashes[] = { 3.0, 5.0 };

	for (float dB = 0.0; dB < _max_dB; dB += _step_dB ) {
		snprintf(buf, 256, "+%0.0f", dB );

		y  = ( _max_dB - dB) / ( _max_dB - _min_dB );
		//std::cerr << " y = " << y << std::endl;
		y *= _analysis_height;

		if (dB != 0.0) {
			cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
			cairo_move_to(cr, 1.0,     y + extents.height + 1.0);
			cairo_show_text(cr, buf);
			cairo_stroke(cr);
		}

		cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
		cairo_move_to(cr, 0,     y);
		cairo_line_to(cr, _analysis_width, y);
		cairo_stroke(cr);

		if (dB == 0.0) {
			cairo_set_dash(cr, dashes, 2, 0.0);
		}
	}



	for (float dB = - _step_dB; dB > _min_dB; dB -= _step_dB ) {
		snprintf(buf, 256, "%0.0f", dB );

		y  = ( _max_dB - dB) / ( _max_dB - _min_dB );
		y *= _analysis_height;

		cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
		cairo_move_to(cr, 1.0,     y - extents.descent - 1.0);
		cairo_show_text(cr, buf);
		cairo_stroke(cr);

		cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
		cairo_move_to(cr, 0,     y);
		cairo_line_to(cr, _analysis_width, y);
		cairo_stroke(cr);
	}

	cairo_set_dash(cr, 0, 0, 0.0);

}

inline float
power_to_dB(float a)
{
	return 10.0 * log10f(a);
}

void
PluginEqGui::plot_impulse_amplitude(Gtk::Widget *w, cairo_t *cr)
{
	float x,y;
	int prevX = 0;
	float avgY = 0.0;
	int avgNum = 0;

	// float width  = w->get_width();
	float height = w->get_height();

        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_set_line_width (cr, 2.5);

	for (uint32_t i = 0; i < _impulse_fft->bins()-1; i++) {
		// x coordinate of bin i
		x  = log10f(1.0 + (float)i / (float)_impulse_fft->bins() * _log_coeff) / _log_max;
		x *= _analysis_width;

		float yCoeff = ( power_to_dB(_impulse_fft->power_at_bin(i)) - _min_dB) / (_max_dB - _min_dB);

		y = _analysis_height - _analysis_height*yCoeff;

		if ( i == 0 ) {
			cairo_move_to(cr, x, y);

			avgY = 0;
			avgNum = 0;
		} else if (rint(x) > prevX || i == _impulse_fft->bins()-1 ) {
			avgY = avgY/(float)avgNum;
			if (avgY > (height * 10.0) ) avgY = height * 10.0;
			if (avgY < (-height * 10.0) ) avgY = -height * 10.0;
			cairo_line_to(cr, prevX, avgY);
			//cairo_line_to(cr, prevX, avgY/(float)avgNum);

			avgY = 0;
			avgNum = 0;

		}

		prevX = rint(x);
		avgY += y;
		avgNum++;
	}

	cairo_stroke(cr);
}

void
PluginEqGui::plot_signal_amplitude_difference(Gtk::Widget *w, cairo_t *cr)
{
	float x,y;

	int prevX = 0;
	float avgY = 0.0;
	int avgNum = 0;

	// float width  = w->get_width();
	float height = w->get_height();

        cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
	cairo_set_line_width (cr, 2.5);

	for (uint32_t i = 0; i < _signal_input_fft->bins()-1; i++) {
		// x coordinate of bin i
		x  = log10f(1.0 + (float)i / (float)_signal_input_fft->bins() * _log_coeff) / _log_max;
		x *= _analysis_width;

		float power_out = power_to_dB(_signal_output_fft->power_at_bin(i));
		float power_in  = power_to_dB(_signal_input_fft ->power_at_bin(i));
		float power = power_out - power_in;

		// for SaBer
		/*
		double p = 10.0 * log10( 1.0 + (double)_signal_output_fft->power_at_bin(i) - (double)
 - _signal_input_fft ->power_at_bin(i));
		//p *= 1000000.0;
		float power = (float)p;

		if ( (i % 1000) == 0) {
			std::cerr << i << ": " << power << std::endl;
		}
		*/

		if (isinf(power)) {
			if (power < 0) {
				power = _min_dB - 1.0;
			} else {
				power = _max_dB - 1.0;
			}
		} else if (isnan(power)) {
			power = _min_dB - 1.0;
		}

		float yCoeff = ( power - _min_dB) / (_max_dB - _min_dB);

		y = _analysis_height - _analysis_height*yCoeff;

		if ( i == 0 ) {
			cairo_move_to(cr, x, y);

			avgY = 0;
			avgNum = 0;
		} else if (rint(x) > prevX || i == _impulse_fft->bins()-1 ) {
			avgY = avgY/(float)avgNum;
			if (avgY > (height * 10.0) ) avgY = height * 10.0;
			if (avgY < (-height * 10.0) ) avgY = -height * 10.0;
			cairo_line_to(cr, prevX, avgY);

			avgY = 0;
			avgNum = 0;

		}

		prevX = rint(x);
		avgY += y;
		avgNum++;
	}

	cairo_stroke(cr);


}
