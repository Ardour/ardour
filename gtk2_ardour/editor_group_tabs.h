#include "cairo_widget.h"

class Editor;

class EditorGroupTabs : public CairoWidget
{
public:
	EditorGroupTabs (Editor *);

	void set_session (ARDOUR::Session *);

private:
	void on_size_request (Gtk::Requisition *);
	bool on_button_press_event (GdkEventButton *);
	void render (cairo_t *);
	void draw_group (cairo_t *, int32_t, int32_t, ARDOUR::RouteGroup* , Gdk::Color const &);
	
	Editor* _editor;
};
