#include "canvas-program-change.h"
#include <iostream>

using namespace ArdourCanvas;
using namespace std;

CanvasProgramChange::CanvasProgramChange(
		MidiRegionView&                       region,
		Group&                                parent,
		boost::shared_ptr<MIDI::Event>        event,
		double                                height,
		double                                x,
		double                                y)
	: Group(parent, x, y),
	  _region(region),
	  _event(event),
	  _text(0),
	  _line(0),
	  _rect(0),
	  _widget(0)
{
	_text = new Text(*this);
	ostringstream pgm(ios::ate);
	pgm << int(event->pgm_number());
	_text->property_text() = pgm.str();
	_text->property_justification() = Gtk::JUSTIFY_CENTER;
	_text->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiProgramChangeOutline.get();
	double flagwidth  = _text->property_text_width()  + 10.0;
	double flagheight = _text->property_text_height() + 3.0;
	_text->property_x() = flagwidth / 2.0;
	_text->property_y() = flagheight / 2.0;
	_text->show();
	_line = new SimpleLine(*this, 0.0, 0.0, 0.0, height);
	_line->property_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiProgramChangeOutline.get();
	_rect = new SimpleRect(*this, 0.0, 0.0, flagwidth, flagheight);
	_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiProgramChangeOutline.get();
	_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MidiProgramChangeFill.get();
	_text->lower_to_bottom();
	_text->raise(2);
	assert(_widget == 0);
	assert(_text != 0);
	assert(_line != 0);
	assert(_rect != 0);
}

CanvasProgramChange::~CanvasProgramChange()
{
	if(_line)
		delete _line;
	if(_rect)
		delete _rect;
	if(_text)
		delete _text;
	if(_widget)
		delete _widget;
}


