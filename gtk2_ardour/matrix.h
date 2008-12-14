#ifndef __gtk_ardour_matrix_h__
#define __gtk_ardour_matrix_h__

#include <list>
#include <vector>
#include <string>
#include <stdint.h>

#include <gtkmm/eventbox.h>
#include <gtkmm/widget.h>

#include "port_group.h"

class OtherPort {
public:
    OtherPort (const std::string& n, PortGroup& g)
	    : _name (n), _group (g) {}

    std::string name() const { return _name; }
    PortGroup& group() const { return _group; }
    bool visible() const { return _group.visible; }

public:
    std::string _name;
    PortGroup& _group;
};

class MatrixNode {
  public:
    MatrixNode (std::string a, OtherPort o, int32_t x, int32_t y)
	    : _name (a), them (o), _connected (random()%3), _x(x), _y(y) {}
    ~MatrixNode() {}

    PortGroup& get_group() const { return them.group(); }

    std::string our_name() const { return _name; }
    std::string their_name() const { return them.name(); }

    bool connected() const { return _connected; }
    void set_connected (bool yn) { _connected = yn; }
    int32_t x() const { return _x; }
    int32_t y() const { return _y; }

  private:
    std::string _name;
    OtherPort them;
    bool _connected;
    int32_t _x;
    int32_t _y;
};

class Matrix : public Gtk::EventBox
{
  public: 
    Matrix();

    void set_ports (const std::list<std::string>&);
    void add_group (PortGroup&);
    void remove_group (PortGroup&);
    void hide_group (PortGroup&);
    void show_group (PortGroup&);
    void clear ();

    int row_spacing () const { return xstep; }

  protected:
    bool on_button_press_event (GdkEventButton* ev);
    bool on_expose_event (GdkEventExpose* ev);
    void on_size_allocate (Gtk::Allocation&);
    void on_size_request (Gtk::Requisition*);
    void on_realize ();
    bool on_motion_notify_event (GdkEventMotion*);
    bool on_leave_notify_event (GdkEventCrossing*);

    MatrixNode* get_node (int32_t x, int32_t y);

private: 
    int height;
    int width;
    int alloc_width;
    int alloc_height;
    bool drawn;
    int labels_y_shift;
    int labels_x_shift;
    float angle_radians;
    int border;
    int ystep;
    int xstep;
    uint32_t line_height;
    uint32_t line_width;
    int arc_radius;
    int32_t motion_x;
    int32_t motion_y;

    std::list<std::string> ours;
    std::list<OtherPort> others;
    std::vector<MatrixNode*> nodes;

    void reset_size ();
    void redraw (GdkDrawable*, GdkRectangle*);
    void alloc_pixmap ();
    void setup_nodes ();

    GdkPixmap* pixmap;
};

#endif /* __gtk_ardour_matrix_h__ */
