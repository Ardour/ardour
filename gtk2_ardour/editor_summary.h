#ifndef __gtk_ardour_editor_summary_h__
#define __gtk_ardour_editor_summary_h__

#include <gtkmm/eventbox.h>

namespace ARDOUR {
	class Session;
}

class Editor;

/** Class to provide a visual summary of the contents of an editor window; represents
 *  the whole session as a set of lines, one per region view.
 */
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

	Editor* _editor; ///< our editor
	ARDOUR::Session* _session; ///< our session
	GdkPixmap* _pixmap; ///< pixmap containing a rendering of the region views, or 0
	bool _regions_dirty; ///< true if _pixmap requires re-rendering, otherwise false
	int _width; ///< pixmap width
	int _height; ///< pixmap height
	double _pixels_per_frame; ///< pixels per frame for the x axis of the pixmap
};

#endif
