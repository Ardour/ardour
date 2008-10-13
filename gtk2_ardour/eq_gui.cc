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

#include "eq_gui.h"
#include "fft.h"

#include "ardour_ui.h"
#include <ardour/audio_buffer.h>
#include <ardour/data_type.h>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>

#include <iostream>


PluginEqGui::PluginEqGui(boost::shared_ptr<ARDOUR::PluginInsert> pluginInsert)
	: _mindB(-12.0),
	  _maxdB(+12.0),
	  _dBStep(3.0),
	  _impulseFft(0)
{
	_samplerate = ARDOUR_UI::instance()->the_session()->frame_rate();

	_plugin = pluginInsert->get_impulse_analysis_plugin();
	_plugin->activate();

	setBufferSize(4096);

	_logCoeff = (1.0 - 2.0 * (1000.0/(_samplerate/2.0))) / powf(1000.0/(_samplerate/2.0), 2.0); 
	_logMax = log10f(1 + _logCoeff);


	// Setup analysis drawing area
	_analysisScaleSurface = 0;

	_analysisArea = new Gtk::DrawingArea();
	_analysisWidth = 500.0;
	_analysisHeight = 500.0;
	_analysisArea->set_size_request(_analysisWidth, _analysisHeight);

	_analysisArea->signal_expose_event().connect( sigc::mem_fun (*this, &PluginEqGui::exposeAnalysisArea));
	_analysisArea->signal_size_allocate().connect( sigc::mem_fun (*this, &PluginEqGui::resizeAnalysisArea));
	

	// dB selection
	dBScaleModel = Gtk::ListStore::create(dBColumns);

	dBScaleCombo = new Gtk::ComboBox(dBScaleModel);
	dBScaleCombo -> set_title("dB scale");

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

#undef ADD_DB_ROW

	dBScaleCombo -> pack_start(dBColumns.name);
	dBScaleCombo -> set_active(1);

	dBScaleCombo -> signal_changed().connect( sigc::mem_fun(*this, &PluginEqGui::dBScaleChanged) );

	Gtk::Label *dBComboLabel = new Gtk::Label("dB scale");	

	Gtk::HBox *dBSelectBin = new Gtk::HBox(false, 5);
	dBSelectBin->add( *manage(dBComboLabel));
	dBSelectBin->add( *manage(dBScaleCombo));
	
	// Phase checkbutton
	phaseSelect = new Gtk::CheckButton("Show phase");
	phaseSelect->set_active(true);
	phaseSelect->signal_toggled().connect( sigc::mem_fun(*this, &PluginEqGui::redrawScales));

	// Update button
	Gtk::Button *btn = new Gtk::Button("Update");
	btn->signal_clicked().connect( sigc::mem_fun(*this, &PluginEqGui::runAnalysis));

	// populate table
	attach( *manage(_analysisArea), 1, 4, 1, 2);
	attach( *manage(dBSelectBin), 	1, 2, 2, 3, Gtk::SHRINK, Gtk::SHRINK);
	attach( *manage(phaseSelect),	2, 3, 2, 3, Gtk::SHRINK, Gtk::SHRINK);
	attach( *manage(btn),           3, 4, 2, 3, Gtk::SHRINK, Gtk::SHRINK);


	// Timeout 
	//_updateConn = Glib::signal_timeout().connect( sigc::mem_fun(this, &PluginEqGui::timeoutCallback), 250);
}

PluginEqGui::~PluginEqGui()
{
	std::cerr << "Destroying PluginEqGui for " << _plugin->name() << std::endl;
	if (_analysisScaleSurface) {
		cairo_surface_destroy (_analysisScaleSurface);
	}

	delete _impulseFft;
	_plugin->deactivate();
	
	// all gui objects are *manage'd by the inherited Table object
}

void
PluginEqGui::on_hide()
{
	Gtk::Table::on_hide();
	_updateConn.disconnect();
}

void
PluginEqGui::on_show()
{
	Gtk::Table::on_show();
	_updateConn = Glib::signal_timeout().connect( sigc::mem_fun(this, &PluginEqGui::timeoutCallback), 250);
}

void
PluginEqGui::dBScaleChanged()
{
	Gtk::TreeModel::iterator iter = dBScaleCombo -> get_active();

	Gtk::TreeModel::Row row;

	if(iter && (row = *iter)) {
		_mindB = row[dBColumns.dBMin];
		_maxdB = row[dBColumns.dBMax];
		_dBStep = row[dBColumns.dBStep];
		

		redrawScales();
	}
}

void
PluginEqGui::redrawScales()
{

	if (_analysisScaleSurface) {
		cairo_surface_destroy (_analysisScaleSurface);
		_analysisScaleSurface = 0;
	}

	_analysisArea->queue_draw();	
}

void
PluginEqGui::setBufferSize(uint32_t size)
{
	if (_bufferSize == size)
		return;

	_bufferSize = size;

	if (_impulseFft) {
		delete _impulseFft;
		_impulseFft = 0;
	}

	_impulseFft = new FFT(_bufferSize);

	uint32_t inputs  = _plugin->get_info()->n_inputs.n_audio();
	uint32_t outputs = _plugin->get_info()->n_outputs.n_audio();

	uint32_t n_chans = std::max(inputs, outputs);
	_bufferset.ensure_buffers(ARDOUR::DataType::AUDIO, n_chans, _bufferSize);

	ARDOUR::ChanCount chanCount(ARDOUR::DataType::AUDIO, n_chans);
	_bufferset.set_count(chanCount);

	/*

        const uint32_t nbufs = _bufferset.count().n_audio();
	std::cerr << "ensure_buffers(ARDOUR::DataType::Audio, " << n_chans << ", " << _bufferSize << "), _bufferset.count().n_audio() = " << nbufs << std::endl;
	*/
}

void 
PluginEqGui::resizeAnalysisArea(Gtk::Allocation& size)
{
	_analysisWidth  = (float)size.get_width();
	_analysisHeight = (float)size.get_height();

	if (_analysisScaleSurface) {
		cairo_surface_destroy (_analysisScaleSurface);
		_analysisScaleSurface = 0;
	}
}

bool
PluginEqGui::timeoutCallback()
{
	/*
	struct timeval tv;
	struct timezone tz;
	
	gettimeofday(&tv, &tz);
	std::cerr << " time = " << tv.tv_sec << ":" << tv.tv_usec << std::endl;
	*/
	runAnalysis();

	return true;
}

void
PluginEqGui::runAnalysis()
{
	uint32_t inputs  = _plugin->get_info()->n_inputs.n_audio();
	uint32_t outputs = _plugin->get_info()->n_outputs.n_audio();

        const uint32_t nbufs = _bufferset.count().n_audio();

	// Create the impulse, can't use silence() because consecutive calls won't work
	for (uint32_t i = 0; i < inputs; ++i) {
		ARDOUR::AudioBuffer &buf = _bufferset.get_audio(i);
		ARDOUR::Sample *d = buf.data(_bufferSize, 0);
		memset(d, 0, sizeof(ARDOUR::Sample)*_bufferSize);
		*d = 1.0;
	}
	uint32_t x,y;
	x=y=0;

	_plugin->connect_and_run(_bufferset, x, y, _bufferSize, (nframes_t)0);

	// Analyze all output buffers
	_impulseFft->reset();
	for (uint32_t i = 0; i < outputs; ++i) {
		_impulseFft->analyze(_bufferset.get_audio(i).data(_bufferSize, 0));
	}

	// normalize the output
	_impulseFft->calculate();

	// This signals calls exposeAnalysisArea()
	_analysisArea->queue_draw();	

}

bool
PluginEqGui::exposeAnalysisArea(GdkEventExpose *evt)
{
	redrawAnalysisArea();

	return false;
}

void
PluginEqGui::generateAnalysisScale(cairo_t *ref_cr)
{
	// TODO: check whether we need rounding
	_analysisScaleSurface = cairo_surface_create_similar(cairo_get_target(ref_cr), 
							     CAIRO_CONTENT_COLOR, 
							     _analysisWidth,
							     _analysisHeight);

	cairo_t *cr = cairo_create (_analysisScaleSurface);

        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_rectangle(cr, 0.0, 0.0, _analysisWidth, _analysisHeight);
        cairo_fill(cr);


	drawPowerScale(_analysisArea, cr);
	if (phaseSelect->get_active()) {
		drawPhaseScale(_analysisArea, cr);
	}
	
        cairo_destroy(cr);
	
}

void
PluginEqGui::redrawAnalysisArea()
{
	cairo_t *cr;

        cr = gdk_cairo_create(GDK_DRAWABLE(_analysisArea->get_window()->gobj()));

	if (_analysisScaleSurface == 0) {
		generateAnalysisScale(cr);
	}
	

	cairo_copy_page(cr);

	cairo_set_source_surface(cr, _analysisScaleSurface, 0.0, 0.0);
	cairo_paint(cr);

	if (phaseSelect->get_active()) {
		drawPhase(_analysisArea, cr);
	}
	drawPower(_analysisArea, cr);


        cairo_destroy(cr);


}

#define PHASE_PROPORTION 0.6

void 
PluginEqGui::drawPhaseScale(Gtk::Widget *w, cairo_t *cr)
{
	float y;
	cairo_font_extents_t extents;
	cairo_font_extents(cr, &extents);

	char buf[256];
	cairo_text_extents_t t_ext;

	for (uint32_t i = 0; i < 3; i++) {

		y = _analysisHeight/2.0 - (float)i*(_analysisHeight/8.0)*PHASE_PROPORTION;

        	cairo_set_source_rgb(cr, .8, .9, 0.2);
		if (i == 0) {
			snprintf(buf,256, "0\u00b0");
		} else {
			snprintf(buf,256, "%d\u00b0", (i * 45));
		}
		cairo_text_extents(cr, buf, &t_ext);
		cairo_move_to(cr, _analysisWidth - t_ext.width - t_ext.x_bearing - 2.0, y - extents.descent);
		cairo_show_text(cr, buf);
		
		if (i == 0)
			continue;
		

        	cairo_set_source_rgba(cr, .8, .9, 0.2, 0.6/(float)i);
		cairo_move_to(cr, 0.0,            y);
		cairo_line_to(cr, _analysisWidth, y);

		
		y = _analysisHeight/2.0 + (float)i*(_analysisHeight/8.0)*PHASE_PROPORTION;

		// label
		snprintf(buf,256, "-%d\u00b0", (i * 45));
        	cairo_set_source_rgb(cr, .8, .9, 0.2);
		cairo_text_extents(cr, buf, &t_ext);
		cairo_move_to(cr, _analysisWidth - t_ext.width - t_ext.x_bearing - 2.0, y - extents.descent);
		cairo_show_text(cr, buf);

		// line
        	cairo_set_source_rgba(cr, .8, .9, 0.2, 0.6/(float)i);
		cairo_move_to(cr, 0.0,            y);
		cairo_line_to(cr, _analysisWidth, y);

		cairo_set_line_width (cr, 0.25 + 1.0/(float)(i+1));
		cairo_stroke(cr);
	}
}

void 
PluginEqGui::drawPhase(Gtk::Widget *w, cairo_t *cr)
{
	float x,y;

	int prevX = 0;
	float avgY = 0.0;
	int avgNum = 0;

        cairo_set_source_rgba(cr, 0.95, 0.3, 0.2, 1.0);
	for (uint32_t i = 0; i < _impulseFft->bins()-1; i++) {
		// x coordinate of bin i
		x  = log10f(1.0 + (float)i / (float)_impulseFft->bins() * _logCoeff) / _logMax;
		x *= _analysisWidth;

		y  = _analysisHeight/2.0 - (_impulseFft->phaseAtBin(i)/M_PI)*(_analysisHeight/2.0)*PHASE_PROPORTION;
	
		if ( i == 0 ) {
			cairo_move_to(cr, x, y);

			avgY = 0;
			avgNum = 0;
		} else if (rint(x) > prevX || i == _impulseFft->bins()-1 ) {
			cairo_line_to(cr, prevX, avgY/(float)avgNum);

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
PluginEqGui::drawPowerScale(Gtk::Widget *w, cairo_t *cr)
{
	static float scales[] = { 30.0, 70.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 15000.0, 20000.0, -1.0 };
	
	float divisor = _samplerate / 2.0 / _impulseFft->bins();
	float x;

	cairo_set_line_width (cr, 1.5);
	cairo_set_font_size(cr, 9);

	cairo_font_extents_t extents;
	cairo_font_extents(cr, &extents);
	float fontXOffset = extents.descent + 1.0;

	char buf[256];

	for (uint32_t i = 0; scales[i] != -1.0; ++i) {
		float bin = scales[i] / divisor;

		x  = log10f(1.0 + bin / (float)_impulseFft->bins() * _logCoeff) / _logMax;
		x *= _analysisWidth;

		if (scales[i] < 1000.0) {
			snprintf(buf, 256, "%0.0f", scales[i]);
		} else {
			snprintf(buf, 256, "%0.0fk", scales[i]/1000.0);
		}

		cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);

		cairo_move_to(cr, x + fontXOffset, 3.0);

		cairo_rotate(cr, M_PI / 2.0);
		cairo_show_text(cr, buf);
		cairo_rotate(cr, -M_PI / 2.0);
		cairo_stroke(cr);

		cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
		cairo_move_to(cr, x, _analysisHeight);
		cairo_line_to(cr, x, 0.0);
		cairo_stroke(cr);
	}

	float y;

	//double dashes[] = { 1.0, 3.0, 4.5, 3.0 };
	double dashes[] = { 3.0, 5.0 };

	for (float dB = 0.0; dB < _maxdB; dB += _dBStep ) {
		snprintf(buf, 256, "+%0.0f", dB );

		y  = ( _maxdB - dB) / ( _maxdB - _mindB );
		//std::cerr << " y = " << y << std::endl;
		y *= _analysisHeight;

		if (dB != 0.0) {
			cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
			cairo_move_to(cr, 1.0,     y + extents.height + 1.0);
			cairo_show_text(cr, buf);
			cairo_stroke(cr);
		}

		cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
		cairo_move_to(cr, 0,     y);
		cairo_line_to(cr, _analysisWidth, y);
		cairo_stroke(cr);

		if (dB == 0.0) {
			cairo_set_dash(cr, dashes, 2, 0.0);
		}
	}

	
	
	for (float dB = - _dBStep; dB > _mindB; dB -= _dBStep ) {
		snprintf(buf, 256, "%0.0f", dB );

		y  = ( _maxdB - dB) / ( _maxdB - _mindB );
		y *= _analysisHeight;

		cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
		cairo_move_to(cr, 1.0,     y - extents.descent - 1.0);
		cairo_show_text(cr, buf);
		cairo_stroke(cr);

		cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
		cairo_move_to(cr, 0,     y);
		cairo_line_to(cr, _analysisWidth, y);
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
PluginEqGui::drawPower(Gtk::Widget *w, cairo_t *cr)
{
	float x,y;

	int prevX = 0;
	float avgY = 0.0;
	int avgNum = 0;

        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_set_line_width (cr, 2.5);

	for (uint32_t i = 0; i < _impulseFft->bins()-1; i++) {
		// x coordinate of bin i
		x  = log10f(1.0 + (float)i / (float)_impulseFft->bins() * _logCoeff) / _logMax;
		x *= _analysisWidth;

		float yCoeff = ( power_to_dB(_impulseFft->powerAtBin(i)) - _mindB) / (_maxdB - _mindB);

		y = _analysisHeight - _analysisHeight*yCoeff;

		if ( i == 0 ) {
			cairo_move_to(cr, x, y);

			avgY = 0;
			avgNum = 0;
		} else if (rint(x) > prevX || i == _impulseFft->bins()-1 ) {
			cairo_line_to(cr, prevX, avgY/(float)avgNum);

			avgY = 0;
			avgNum = 0;
				
		}

		prevX = rint(x);
		avgY += y;
		avgNum++;
	}

	cairo_stroke(cr);
}

