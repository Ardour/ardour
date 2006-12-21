#ifndef __gtk_ardour_panner_h__
#define __gtk_ardour_panner_h__

#include <gtkmm2ext/barcontroller.h>

class PannerBar : public Gtkmm2ext::BarController
{
  public:
	PannerBar (Gtk::Adjustment& adj, PBD::Controllable&);
	~PannerBar ();

	void on_size_request (Gtk::Requisition*);

  protected:
	bool expose (GdkEventExpose*);
	bool button_press (GdkEventButton*);
	bool button_release (GdkEventButton*);
};

#endif /* __gtk_ardour_panner_h__ */
