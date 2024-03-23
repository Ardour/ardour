#ifndef __ardour_gtk_livetrax_meters_h__
#define __ardour_gtk_livetrax_meters_h__

#include <unistd.h>

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>

namespace Gtk {
	class Label;
}

namespace ArdourWidgets {
	class FastMeter;
}

class LiveTraxMeters : public Gtk::ScrolledWindow
{
  public:
	LiveTraxMeters (size_t initial_cnt);
	~LiveTraxMeters ();

	void resize (size_t);

  private:
	Gtk::HBox meter_box;
	Gtk::HBox global_hbox;
	std::vector<Gtk::Widget*> widgets;
	std::vector<ArdourWidgets::FastMeter*> meters;
	sigc::connection fast_screen_update_connection;

	bool update_meters ();
};

#endif /* __ardour_gtk_livetrax_meters_h__ */
