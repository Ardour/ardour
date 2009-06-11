#ifndef __gtk_ardour_editor_summary_h__
#define __gtk_ardour_editor_summary_h__

#include <gtkmm/eventbox.h>

namespace ARDOUR {
	class Session;
}

class Editor;

class EditorSummary : public Gtk::EventBox
{
public:
	EditorSummary (Editor *);
	~EditorSummary ();

	void set_session (ARDOUR::Session *);
	void set_dirty ();
	void set_bounds_dirty ();

private:
	bool on_expose_event (GdkEventExpose *);
	void on_size_request (Gtk::Requisition *);
	void on_size_allocate (Gtk::Allocation &);
	bool on_button_press_event (GdkEventButton *);

	void render (cairo_t *);
	GdkPixmap* get_pixmap (GdkDrawable *);
	void render_region (RegionView*, cairo_t*, nframes_t, double) const;

	Editor* _editor;
	ARDOUR::Session* _session;
	GdkPixmap* _pixmap;
	bool _regions_dirty;
	int _width;
	int _height;
	double _pixels_per_frame;
};

#endif
