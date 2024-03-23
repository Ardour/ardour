#include "ardour/logmeter.h"
#include "ardour/audioengine.h"

#include "widgets/fastmeter.h"

#include "livetrax_meters.h"
#include "ui_config.h"

using namespace ARDOUR;
using namespace ArdourWidgets;

#define PX_SCALE(px) std::max ((float)px, rintf ((float)px* UIConfiguration::instance ().get_ui_scale ()))

LiveTraxMeters::LiveTraxMeters (size_t initial_cnt)
{
	set_policy (Gtk::POLICY_ALWAYS, Gtk::POLICY_NEVER);

	resize (initial_cnt);

	meter_box.set_spacing (PX_SCALE (10));
	add (meter_box);

	fast_screen_update_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &LiveTraxMeters::update_meters), 40, GDK_PRIORITY_REDRAW + 10);
}

LiveTraxMeters::~LiveTraxMeters ()
{
	fast_screen_update_connection.disconnect ();
}

void
LiveTraxMeters::resize (size_t sz)
{
	size_t old = meters.size();

	while (old > sz) {
		/* Widgets are all managed so this should delete them as they
		   are removed.
		*/
		meter_box.remove (*widgets[old - 1]);
		meters.pop_back ();
		old--;
	}

	if (old == sz) {
		return;
	}

	uint32_t c[10];
	uint32_t b[4];
	float stp[4];

	c[0] = UIConfiguration::instance().color ("meter color0");
	c[1] = UIConfiguration::instance().color ("meter color1");
	c[2] = UIConfiguration::instance().color ("meter color2");
	c[3] = UIConfiguration::instance().color ("meter color3");
	c[4] = UIConfiguration::instance().color ("meter color4");
	c[5] = UIConfiguration::instance().color ("meter color5");
	c[6] = UIConfiguration::instance().color ("meter color6");
	c[7] = UIConfiguration::instance().color ("meter color7");
	c[8] = UIConfiguration::instance().color ("meter color8");
	c[9] = UIConfiguration::instance().color ("meter color9");
	b[0] = UIConfiguration::instance().color ("meter background bottom");
	b[1] = UIConfiguration::instance().color ("meter background top");
	b[2] = 0x991122ff; // red highlight gradient Bot
	b[3] = 0x551111ff; // red highlight gradient Top

	stp[0] = 115.0 * log_meter0dB (-15);
	stp[1] = 115.0 * log_meter0dB (-9);
	stp[2] = 115.0 * log_meter0dB (-3);
	stp[3] = 115.0;


	// XXX config changed -> update meter style (and size)

	for (size_t i = old; i < sz; ++i) {

		meters.push_back (manage (new FastMeter (10 /* (uint32_t)floor (UIConfiguration::instance ().get_meter_hold ()) */, 
		                                          8, FastMeter::Vertical, PX_SCALE (64),
		                                          c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9],
		                                          b[0], b[1], b[2], b[3],
		                                          stp[0], stp[1], stp[2], stp[3],
		                                          (UIConfiguration::instance ().get_meter_style_led () ? 3 : 1))));

		Gtk::VBox* vb = manage (new Gtk::VBox);
		char buf[16];
		snprintf (buf, sizeof (buf), "%zu", i+1);
		Gtk::Label* l = manage (new Gtk::Label (buf));
		vb->pack_start (*l, false, false);
		vb->pack_start (*meters.back(), true, true);

		widgets.push_back (vb);

		meter_box.pack_start (*vb, false, false, 0);
	}

	meter_box.show_all ();
}

bool
LiveTraxMeters::update_meters ()
{
	PortManager::AudioInputPorts const aip (AudioEngine::instance ()->audio_input_ports ());

	size_t n = 0;

	for (auto const & p : aip) {
		if (n >= meters.size()) {
			break;
		}
		meters[n]->set (p.second.meter->level, p.second.meter->peak);
		++n;
	}

	return true;
}
