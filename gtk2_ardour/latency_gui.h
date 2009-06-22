#ifndef __gtk2_ardour_latency_gui_h__
#define __gtk2_ardour_latency_gui_h__

#include <vector>
#include <string>

#include <gtkmm/dialog.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/adjustment.h>

#include <gtkmm2ext/barcontroller.h>
#include "pbd/controllable.h"

#include "ardour/types.h"

#include "ardour_dialog.h"

namespace ARDOUR {
	class Latent;
}

class LatencyGUI;

class LatencyBarController : public Gtkmm2ext::BarController
{
public:
	LatencyBarController (Gtk::Adjustment& adj, LatencyGUI* g)
		: BarController (adj, boost::shared_ptr<PBD::IgnorableControllable> (new PBD::IgnorableControllable ())),
		  _latency_gui (g) {}
	
private:
	LatencyGUI* _latency_gui;
	
	std::string get_label (int&);
};	

class LatencyGUI : public Gtk::VBox
{
  public:
	LatencyGUI (ARDOUR::Latent&, nframes64_t sample_rate, nframes64_t period_size);
	~LatencyGUI() { }

	void finish ();
	void reset ();
	void refresh ();

  private:
	ARDOUR::Latent& _latent;
	nframes64_t initial_value;
	nframes64_t sample_rate;
	nframes64_t period_size;
	boost::shared_ptr<PBD::IgnorableControllable> ignored;

	Gtk::Adjustment adjustment;
	LatencyBarController bc;
	Gtk::HBox hbox1;
	Gtk::HBox hbox2;
	Gtk::HButtonBox hbbox;
	Gtk::Button minus_button;
	Gtk::Button plus_button;
	Gtk::Button reset_button;
	Gtk::ComboBoxText units_combo;

	void change_latency_from_button (int dir);

	friend class LatencyBarController;

	static std::vector<std::string> unit_strings;
};

class LatencyDialog : public ArdourDialog
{
  public:
	LatencyDialog (const Glib::ustring& title, ARDOUR::Latent&, nframes64_t sample_rate, nframes64_t period_size);
	~LatencyDialog() {}

  private:
	LatencyGUI lwidget;
};

#endif /* __gtk2_ardour_latency_gui_h__ */
