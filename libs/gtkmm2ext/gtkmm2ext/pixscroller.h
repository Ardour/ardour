#ifndef __gtkmm2ext_pixscroller_h__ 
#define __gtkmm2ext_pixscroller_h__

#include <gtkmm/drawingarea.h>
#include <gtkmm/adjustment.h>
#include <gdkmm.h>

namespace Gtkmm2ext {

class PixScroller : public Gtk::DrawingArea
{
  public:
	PixScroller(Gtk::Adjustment& adjustment, 
		    Glib::RefPtr<Gdk::Pixbuf> slider,
		    Glib::RefPtr<Gdk::Pixbuf> rail);

	bool on_expose_event (GdkEventExpose*);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	void on_size_request (GtkRequisition*);

  protected:
	Gtk::Adjustment& adj;

  private:
	Glib::RefPtr<Gdk::Pixbuf> rail;
	Glib::RefPtr<Gdk::Pixbuf> slider;
	Gdk::Rectangle sliderrect;
	Gdk::Rectangle railrect;
	GdkWindow* grab_window;
	double grab_y;
	double grab_start;
	int overall_height;
	bool dragging;
	
	float default_value;

	void adjustment_changed ();
};

} // namespace

#endif /* __gtkmm2ext_pixscroller_h__ */
