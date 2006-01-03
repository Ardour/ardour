#ifndef __gtk_ardour_xfade_edit_h__
#define __gtk_ardour_xfade_edit_h__

#include <list>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/radiobutton.h>

#include <ardour/curve.h>
#include "ardour_dialog.h"
#include "canvas.h"

namespace ARDOUR
{
	class Session;
	class Curve;
	class Crossfade;
}

class CrossfadeEditor : public ArdourDialog
{
  public:
    CrossfadeEditor (ARDOUR::Session&, ARDOUR::Crossfade&, double miny, double maxy);
    ~CrossfadeEditor ();
    
    void apply ();

    static const double canvas_border;
    
    /* these are public so that a caller/subclass can make them do the right thing.
     */
    
    Gtk::Button* cancel_button;
    Gtk::Button* ok_button;

    struct PresetPoint {
	double x;
	double y;

	PresetPoint (double a, double b) 
		: x (a), y (b) {}
    };

    struct Preset : public list<PresetPoint> {
	const gchar** xpm;
	
	Preset (const gchar** x)
		: xpm (x) {}
    };

    typedef list<Preset*> Presets;

    static Presets* fade_in_presets;
    static Presets* fade_out_presets;

  private:
    ARDOUR::Crossfade& xfade;
    ARDOUR::Session& session;

    Gtk::VBox vpacker;

    struct Point {
	~Point();

	ArdourCanvas::SimpleRect* box;
	ArdourCanvas::Line* curve;
	double x;
	double y;

	static const int32_t size;

	void move_to (double x, double y, double xfract, double yfract);
    };

    struct PointSorter 
    {
	bool operator() (const CrossfadeEditor::Point* a, const CrossfadeEditor::Point *b) {
		return a->x < b->x;
	}
    };

    ArdourCanvas::SimpleRect*   toplevel;
    ArdourCanvas::Canvas* canvas;

    struct Half {
	ArdourCanvas::Line*     line;
	ArdourCanvas::Polygon*  shading;
	list<Point*>            points;
	ARDOUR::Curve           normative_curve; /* 0 - 1.0, linear */
	ARDOUR::Curve           gain_curve;      /* 0 - 2.0, gain mapping */
	vector<ArdourCanvas::WaveView*>  waves;

	Half();
    };

    enum WhichFade {
	    In = 0,
	    Out = 1
    };

    Half fade[2];
    WhichFade current;

    bool point_grabbed;
    vector<Gtk::Button*> fade_out_buttons;
    vector<Gtk::Button*> fade_in_buttons;

    Gtk::VBox vpacker2;

    Gtk::Button clear_button;
    Gtk::Button revert_button;

    Gtk::ToggleButton audition_both_button;
    Gtk::ToggleButton audition_left_dry_button;
    Gtk::ToggleButton audition_left_button;
    Gtk::ToggleButton audition_right_dry_button;
    Gtk::ToggleButton audition_right_button;

    Gtk::ToggleButton preroll_button;
    Gtk::ToggleButton postroll_button;

    Gtk::HBox roll_box;

    gint event_handler (GdkEvent*);

    bool canvas_event (GdkEvent* event);
    bool point_event (GdkEvent* event, Point*);
    bool curve_event (GdkEvent* event);

    void canvas_allocation (Gtk::Allocation&);
    void add_control_point (double x, double y);
    Point* make_point ();
    void redraw ();
    
    double effective_width () const { return canvas->get_allocation().get_width() - (2.0 * canvas_border); }
    double effective_height () const { return canvas->get_allocation().get_height() - (2.0 * canvas_border); }

    void clear ();
    void reset ();

    double miny;
    double maxy;

    Gtk::Table fade_in_table;
    Gtk::Table fade_out_table;

    void build_presets ();
    void apply_preset (Preset*);
    
    Gtk::RadioButton select_in_button;
    Gtk::RadioButton select_out_button;
    Gtk::HBox   curve_button_box;
    Gtk::HBox   audition_box;

    void curve_select_clicked (WhichFade);

    double x_coordinate (double& xfract) const;
    double y_coordinate (double& yfract) const;
    
    void set (const ARDOUR::Curve& alist, WhichFade);

    void make_waves (ARDOUR::AudioRegion&, WhichFade);
    void peaks_ready (ARDOUR::AudioRegion* r, WhichFade);
    
    void _apply_to (ARDOUR::Crossfade* xf);
    void setup (ARDOUR::Crossfade*);
    void cancel_audition ();
    void audition_state_changed (bool);

    void audition_toggled ();
    void audition_right_toggled ();
    void audition_right_dry_toggled ();
    void audition_left_toggled ();
    void audition_left_dry_toggled ();

    void audition_both ();
    void audition_left_dry ();
    void audition_left ();
    void audition_right_dry ();
    void audition_right ();

    void xfade_changed (ARDOUR::Change);

    void dump ();
};

#endif /* __gtk_ardour_xfade_edit_h__ */
