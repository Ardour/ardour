#include "bb.h"
#include "gui.h"

BBGUI::BBGUI (int* argc, char** argv[], jack_client_t* j, BeatBox* bb)
	: jack (j)
	, bbox (bb)
	, main (argc, argv)
	, quantize_off (quantize_group, "None")
	, quantize_32nd (quantize_group, "ThirtySecond")
	, quantize_16th (quantize_group, "Sixteenth")
	, quantize_8th (quantize_group, "Eighth")
	, quantize_quarter (quantize_group, "Quarter")
	, quantize_half (quantize_group, "Half")
	, quantize_whole (quantize_group, "Whole")
	, play_button ("Run")
	, clear_button ("Clear")
	, tempo_adjustment (bb->tempo(), 1, 300, 1, 10)
	, tempo_spinner (tempo_adjustment)
{
	quantize_off.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 0));
	quantize_32nd.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 32));
	quantize_16th.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 16));
	quantize_8th.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 8));
	quantize_quarter.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 4));
	quantize_half.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 2));
	quantize_whole.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 1));

	quantize_button_box.pack_start (quantize_off);
	quantize_button_box.pack_start (quantize_32nd);
	quantize_button_box.pack_start (quantize_16th);
	quantize_button_box.pack_start (quantize_8th);
	quantize_button_box.pack_start (quantize_quarter);
	quantize_button_box.pack_start (quantize_half);
	quantize_button_box.pack_start (quantize_whole);

	play_button.signal_toggled().connect (sigc::mem_fun (*this, &BBGUI::toggle_play));
	clear_button.signal_clicked().connect (sigc::mem_fun (*this, &BBGUI::clear));

	misc_button_box.pack_start (play_button);
	misc_button_box.pack_start (clear_button);

	tempo_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &BBGUI::tempo_changed));

	misc_button_box.pack_start (tempo_spinner);

	global_vbox.pack_start (misc_button_box);
	global_vbox.pack_start (quantize_button_box, true, true);
	window.add (global_vbox);
	window.show_all ();
}

BBGUI::~BBGUI ()
{
}

void
BBGUI::run ()
{
	window.show ();
	main.run ();
}

void
BBGUI::tempo_changed ()
{
	float t = tempo_adjustment.get_value();
	bbox->set_tempo (t);
}

void
BBGUI::set_quantize (int divisor)
{
	bbox->set_quantize (divisor);
}

void
BBGUI::clear ()
{
	bbox->clear ();
}

void
BBGUI::toggle_play ()
{
	if (bbox->running()) {
		bbox->stop ();
	} else {
		bbox->start ();
	}
}
