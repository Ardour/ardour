#ifndef __gtkmm2ext_pixscroller_h__ 
#define __gtkmm2ext_pixscroller_h__

#include <gtkmm/drawingarea.h>
#include <gtkmm/adjustment.h>
#include <gdkmm.h>

#include <gtkmm2ext/pix.h>

namespace Gtkmm2ext {

class PixScroller : public Gtk::DrawingArea
{
  public:
	PixScroller(Gtk::Adjustment& adjustment, Pix&);

	
	bool on_expose_event (GdkEventExpose*);
	bool on_motion_notify_event (GdkEventMotion*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	void on_size_request (GtkRequisition*);
	void on_realize ();

  protected:
	Gtk::Adjustment& adj;

  private:
	Pix& pix;

	Glib::RefPtr<Gdk::Pixmap> rail;
	Glib::RefPtr<Gdk::Pixmap> slider;
	Glib::RefPtr<Gdk::Bitmap> rail_mask;
	Glib::RefPtr<Gdk::Bitmap> slider_mask;
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
