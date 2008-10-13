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

#ifndef __ardour_eq_gui_h
#define __ardour_eq_gui_h

#include <ardour/buffer_set.h>
#include <ardour/plugin_insert.h>
#include <ardour/plugin.h>

#include <gtkmm/table.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/combobox.h>
#include <gtkmm/liststore.h>

class Plugin;
class FFT;

class PluginEqGui : public Gtk::Table
{
	public:
		PluginEqGui(boost::shared_ptr<ARDOUR::PluginInsert>);
		~PluginEqGui();
		
	

	private:
		// Setup
		void setBufferSize(uint32_t);
		void dBScaleChanged();

		// Analysis
		void runAnalysis();

		// Drawing
		virtual void on_hide();
		virtual void on_show();

		void resizeAnalysisArea(Gtk::Allocation&);

		void redrawAnalysisArea();
		void generateAnalysisScale(cairo_t *);
		bool exposeAnalysisArea(GdkEventExpose *);

		void drawPowerScale(Gtk::Widget *, cairo_t *);
		void drawPower(Gtk::Widget *,cairo_t *);

		void drawPhaseScale(Gtk::Widget *,cairo_t *);
		void drawPhase(Gtk::Widget *,cairo_t *);

		// Helpers
		bool timeoutCallback();
		void redrawScales();


		// Fields:

		// analysis parameters
		float _samplerate;

		float _mindB;
		float _maxdB;
		float _dBStep;


		float _logCoeff;
		float _logMax;

		nframes_t _bufferSize;

		// buffers		
		ARDOUR::BufferSet _bufferset;


		// dimensions
		float _analysisWidth;
		float _analysisHeight;

		// My objects
		FFT *_impulseFft;
		boost::shared_ptr<ARDOUR::Plugin> _plugin;

		// gui objects
		Gtk::DrawingArea *_analysisArea;
		cairo_surface_t *_analysisScaleSurface;


		// dB scale selection:
		class dBSelectionColumns : public Gtk::TreeModel::ColumnRecord
		{
		  public:
			dBSelectionColumns()
				{ add(dBMin); add(dBMax); add(dBStep); add(name); }

			Gtk::TreeModelColumn<float> dBMin;
			Gtk::TreeModelColumn<float> dBMax;
			Gtk::TreeModelColumn<float> dBStep;
			Gtk::TreeModelColumn<Glib::ustring> name;
		};

		dBSelectionColumns dBColumns;

		Gtk::ComboBox *dBScaleCombo;
		Glib::RefPtr<Gtk::ListStore> dBScaleModel;

		Gtk::CheckButton *phaseSelect;

		// signals and connections
		sigc::connection _updateConn;
};

#endif

