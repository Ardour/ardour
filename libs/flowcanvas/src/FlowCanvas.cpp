/* This file is part of FlowCanvas.  Copyright (C) 2005 Dave Robillard.
 * 
 * FlowCanvas is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * FlowCanvas is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "flowcanvas/FlowCanvas.h"
#include <cassert>
#include <map>
#include <iostream>
#include <cmath>
#include "flowcanvas/Port.h"
#include "flowcanvas/Module.h"

using std::cerr; using std::cout; using std::endl;

namespace LibFlowCanvas {
	

FlowCanvas::FlowCanvas(double width, double height)
: m_selected_port(NULL),
  m_connect_port(NULL),
  m_zoom(1.0),
  m_width(width),
  m_height(height),
  m_drag_state(NOT_DRAGGING),
  m_base_rect(*root(), 0, 0, width, height),
  m_select_rect(NULL),
  m_select_dash(NULL)
{
	set_scroll_region(0.0, 0.0, width, height);
	set_center_scroll_region(true);
	
	m_base_rect.property_fill_color_rgba() = 0x000000FF;
	m_base_rect.show();
	//m_base_rect.signal_event().connect(sigc::mem_fun(this, &FlowCanvas::scroll_drag_handler));
	m_base_rect.signal_event().connect(sigc::mem_fun(this, &FlowCanvas::select_drag_handler));
	m_base_rect.signal_event().connect(sigc::mem_fun(this, &FlowCanvas::connection_drag_handler));
	m_base_rect.signal_event().connect(sigc::mem_fun(this, &FlowCanvas::canvas_event));
	
	set_dither(Gdk::RGB_DITHER_NORMAL); // NONE or NORMAL or MAX
	
	// Dash style for selected modules and selection box
	m_select_dash = new ArtVpathDash();
	m_select_dash->n_dash = 2;
	m_select_dash->dash = art_new(double, 2);
	m_select_dash->dash[0] = 5;
	m_select_dash->dash[1] = 5;
		
	Glib::signal_timeout().connect(
		sigc::mem_fun(this, &FlowCanvas::animate_selected), 150);
}


FlowCanvas::~FlowCanvas()
{
	destroy();
}


void
FlowCanvas::zoom(float pix_per_unit)
{
	// Round to .25
	m_zoom = static_cast<int>(pix_per_unit*4) / 4.0;
	if (m_zoom < 0.25)
		m_zoom = 0.25;
	
	set_pixels_per_unit(m_zoom);

	for (ModuleMap::iterator m = m_modules.begin(); m != m_modules.end(); ++m)
		(*m).second->zoom(m_zoom);
}


void
FlowCanvas::clear_selection()
{
	for (list<Module*>::iterator m = m_selected_modules.begin(); m != m_selected_modules.end(); ++m)
		(*m)->selected(false);
	
	for (list<Connection*>::iterator c = m_selected_connections.begin(); c != m_selected_connections.end(); ++c)
		(*c)->selected(false);

	m_selected_modules.clear();
	m_selected_connections.clear();
}

	
/** Add a module to the current selection, and automagically select any connections
 * between selected modules */
void
FlowCanvas::select_module(Module* m)
{
	assert(! m->selected());
	
	m_selected_modules.push_back(m);

	Connection* c;
	for (ConnectionList::iterator i = m_connections.begin(); i != m_connections.end(); ++i) {
		c = (*i);
		if ( !c->selected()) {
			if (c->source_port()->module() == m && c->dest_port()->module()->selected()) {
				c->selected(true);
				m_selected_connections.push_back(c);
			} else if (c->dest_port()->module() == m && c->source_port()->module()->selected()) {
				c->selected(true);
				m_selected_connections.push_back(c);
			} 
		}
	}
				
	m->selected(true);
}


void
FlowCanvas::unselect_module(Module* m)
{
	assert(m->selected());
	
	// Remove any connections that aren't selected anymore because this module isn't
	Connection* c;
	for (ConnectionList::iterator i = m_selected_connections.begin(); i != m_selected_connections.end();) {
		c = (*i);
		if (c->selected()
			&& ((c->source_port()->module() == m && c->dest_port()->module()->selected())
				|| c->dest_port()->module() == m && c->source_port()->module()->selected()))
			{
				c->selected(false);
				i = m_selected_connections.erase(i);
		} else {
			++i;
		}
	}

	// Remove the module
	for (list<Module*>::iterator i = m_selected_modules.begin(); i != m_selected_modules.end(); ++i) {
		if ((*i) == m) {
			m_selected_modules.erase(i);
			break;
		}
	}
	
	m->selected(false);
}


/** Removes all ports and connections and modules.
 */
void
FlowCanvas::destroy()
{
	for (ModuleMap::iterator m = m_modules.begin(); m != m_modules.end(); ++m)
		delete (*m).second;
	for (ConnectionList::iterator c = m_connections.begin(); c != m_connections.end(); ++c)
		delete (*c);

	m_modules.clear();
	m_connections.clear();

	m_selected_port = NULL;
	m_connect_port = NULL;
}


void
FlowCanvas::selected_port(Port* p)
{
	if (m_selected_port != NULL)
		m_selected_port->rect()->property_fill_color_rgba() = m_selected_port->colour(); // "turn off" the old one
	
	m_selected_port = p;
	
	if (p != NULL)
		m_selected_port->rect()->property_fill_color() = "red";
}


Module*
FlowCanvas::find_module(const string& name)
{
	ModuleMap::iterator m = m_modules.find(name);

	if (m != m_modules.end())
		return (*m).second;
	else
		return NULL;
}


/** Sets the passed module's location to a reasonable default.
 */
void
FlowCanvas::set_default_placement(Module* m)
{
	assert(m != NULL);
	
	// Simple cascade.  This will get more clever in the future.
	double x = ((m_width / 2.0) + (m_modules.size() * 25));
	double y = ((m_height / 2.0) + (m_modules.size() * 25));

	m->move_to(x, y);
}


void
FlowCanvas::add_module(Module* m)
{
	assert(m != NULL);
	std::pair<string, Module*> p(m->name(), m);
	m_modules.insert(p);
}


void
FlowCanvas::remove_module(const string& name)
{
	ModuleMap::iterator m = m_modules.find(name);

	if (m != m_modules.end()) {
		delete (*m).second;
		m_modules.erase(m);
	} else {
		cerr << "[FlowCanvas::remove_module] Unable to find module!" << endl;
	}
}


Port*
FlowCanvas::find_port(const string& node_name, const string& port_name)
{
	Module* module = NULL;
	Port*   port   = NULL;
	
	for (ModuleMap::iterator i = m_modules.begin(); i != m_modules.end(); ++i) {
		module = (*i).second;
		port = module->port(port_name);
		if (module->name() == node_name && port != NULL)
			return port;
	}
	
	cerr << "[FlowCanvas::find_port] Failed to find port " <<
		node_name << ":" << port_name << endl;

	return NULL;
}


void
FlowCanvas::rename_module(const string& old_name, const string& new_name)
{
	Module* module = NULL;
	
	for (ModuleMap::iterator i = m_modules.begin(); i != m_modules.end(); ++i) {
		module = (*i).second;
		assert(module != NULL);
		if (module->name() == old_name) {
			m_modules.erase(i);
			module->name(new_name);
			add_module(module);
			return;
		}
	}
	
	cerr << "[FlowCanvas::rename_module] Failed to find module " <<
		old_name << endl;
}


/** Add a connection.
 */
void
FlowCanvas::add_connection(const string& node1_name, const string& port1_name,
                             const string& node2_name, const string& port2_name)
{
	Port* port1 = find_port(node1_name, port1_name);
	Port* port2 = find_port(node2_name, port2_name);

	if (port1 == NULL) {
		cerr << "Unable to find port " << node1_name << ":" << port1_name
			<< " to make connection." << endl;
	} else if (port2 == NULL) {
		cerr << "Unable to find port " << node2_name << ":" << port2_name
			<< " to make connection." << endl;
	} else {
		add_connection(port1, port2);
	}
}


bool
FlowCanvas::remove_connection(Port* port1, Port* port2)
{
	assert(port1 != NULL);
	assert(port2 != NULL);
	
	Connection* c = get_connection(port1, port2);
	if (c == NULL) {
		cerr << "Couldn't find connection.\n";
		return false;
	} else {
		remove_connection(c);
		return true;
	}
}


/** Remove a connection.
 * 
 * Returns whether or not the connection was found (and removed).
 */
bool
FlowCanvas::remove_connection(const string& mod1_name, const string& port1_name, const string& mod2_name, const string& port2_name)
{
	Connection* c = get_connection(find_port(mod1_name, port1_name),
	                               find_port(mod2_name, port2_name));
	if (c == NULL) {
		cerr << "Couldn't find connection.\n";
		return false;
	} else {
		remove_connection(c);
		return true;
	}
}


bool
FlowCanvas::are_connected(const Port* port1, const Port* port2)
{
	assert(port1 != NULL);
	assert(port2 != NULL);
	
	ConnectionList::const_iterator c;
	const Connection* connection;


	for (c = m_connections.begin(); c != m_connections.end(); ++c) {
		connection = *c;
		if (connection->source_port() == port1 && connection->dest_port() == port2)
			return true;
		if (connection->source_port() == port2 && connection->dest_port() == port1)
			return true;
	}

	return false;
}


Connection*
FlowCanvas::get_connection(const Port* port1, const Port* port2)
{
	assert(port1 != NULL);
	assert(port2 != NULL);
	
	for (ConnectionList::iterator i = m_connections.begin(); i != m_connections.end(); ++i) {
		if ( (*i)->source_port() == port1 && (*i)->dest_port() == port2 )
			return *i;
		else if ( (*i)->dest_port() == port1 && (*i)->source_port() == port2 )
			return *i;
	}
	
	return NULL;
}


void
FlowCanvas::add_connection(Port* port1, Port* port2)
{
	assert(port1->is_input() != port2->is_input());
	assert(port1->is_output() != port2->is_output());
	Port* src_port = NULL;
	Port* dst_port = NULL;
	if (port1->is_output() && port2->is_input()) {
		src_port = port1;
		dst_port = port2;
	} else {
		src_port = port2;
		dst_port = port1;
	}

	// Create (graphical) connection object
	if (get_connection(port1, port2) == NULL) {
		Connection* c = new Connection(this, src_port, dst_port);
		port1->add_connection(c);
		port2->add_connection(c);
		m_connections.push_back(c);
	}
}


void
FlowCanvas::remove_connection(Connection* connection)
{
	assert(connection != NULL);
	
	ConnectionList::iterator i = find(m_connections.begin(), m_connections.end(), connection);
	Connection* c = *i;
	
	c->disconnect();
	m_connections.erase(i);
	delete c;
}


/** Called when two ports are 'toggled' (connected or disconnected)
 */
void
FlowCanvas::ports_joined(Port* port1, Port* port2)
{
	assert(port1 != NULL);
	assert(port2 != NULL);

	port1->hilite(false);
	port2->hilite(false);

	string src_mod_name, dst_mod_name, src_port_name, dst_port_name;

	Port* src_port = NULL;
	Port* dst_port = NULL;
	
	if (port2->is_input() && ! port1->is_input()) {
		src_port = port1;
		dst_port = port2;
	} else if ( ! port2->is_input() && port1->is_input()) {
		src_port = port2;
		dst_port = port1;
	} else {
		return;
	}
	
	if (are_connected(src_port, dst_port))
		disconnect(src_port, dst_port);
	else
		connect(src_port, dst_port);
}


/** Event handler for ports.
 *
 * These events can't be handled in the Port class because they have to do with
 * connections etc. which deal with multiple ports (ie m_selected_port).  Ports
 * pass their events on to this function to get around this.
 */
bool
FlowCanvas::port_event(GdkEvent* event, Port* port)
{
	static bool port_dragging = false;
	bool handled = true;
	
	switch (event->type) {
	
	case GDK_BUTTON_PRESS:
		if (event->button.button == 1) {
			port_dragging = true;
		} else if (event->button.button == 3) {
			m_selected_port = port;
			port->popup_menu(event->button.button, event->button.time);
		} else {
			handled = false;
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (port_dragging) {
			if (m_connect_port == NULL) {
				selected_port(port);
				m_connect_port = port;
			} else {
				ports_joined(port, m_connect_port);
				m_connect_port = NULL;
				selected_port(NULL);
			}
			port_dragging = false;
		} else {
			handled = false;
		}
		break;

	case GDK_ENTER_NOTIFY:
		if (port != m_selected_port)
			port->hilite(true);
		break;

	case GDK_LEAVE_NOTIFY:
		if (port_dragging) {
			m_drag_state = CONNECTION;
			m_connect_port = port;
			
			m_base_rect.grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
				Gdk::Cursor(Gdk::CROSSHAIR), event->button.time);

			port_dragging = false;
		} else {
			if (port != m_selected_port)
				port->hilite(false);
		}
		break;

	default:
		handled = false;
	}
	
	return handled;
}


/*
bool
FlowCanvas::canvas_event(GdkEvent* event)
{
	if (m_connection_dragging) {
		return connection_drag_handler(event);
	} else if (m_scroll_dragging) {
		return scroll_drag_handler(event);
	} else if (event->type == GDK_BUTTON_PRESS && event->button.button == 2) {
		get_scroll_offsets(m_scroll_offset_x, m_scroll_offset_y);
		//double x, y;
		//window_to_world(event->button.x, event->button.y, x, y);
		//w2c(x, y, m_scroll_origin_x, m_scroll_origin_y);
		m_scroll_origin_x = event->button.x;
		m_scroll_origin_y = event->button.y;
		//root()->w2i(m_scroll_origin_x, m_scroll_origin_y);
		//window_to_world(event->button.x, event->button.y, x, y);
		//w2c(x, y, m_scroll_origin_x, m_scroll_origin_y);
		m_scroll_dragging = true;
	}
	return false;
}
*/


/* I can not get this to work for the life of me.
 * Man I hate gnomecanvas.
bool
FlowCanvas::scroll_drag_handler(GdkEvent* event)
{
	
	bool handled = true;
	
	static int    original_scroll_x = 0;
	static int    original_scroll_y = 0;
	static double origin_x = 0;
	static double origin_y = 0;
	static double x_offset = 0;
	static double y_offset = 0;
	static double scroll_offset_x = 0;
	static double scroll_offset_y = 0;
	static double last_x = 0;
	static double last_y = 0;

	bool first_motion = true;
	
	if (event->type == GDK_BUTTON_PRESS && event->button.button == 2) {
		m_base_rect.grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK,
			Gdk::Cursor(Gdk::FLEUR), event->button.time);
		get_scroll_offsets(original_scroll_x, original_scroll_y);
		scroll_offset_x = original_scroll_x;
		scroll_offset_y = original_scroll_y;
		origin_x = event->button.x;
		origin_y = event->button.y;
		last_x = origin_x;
		last_y = origin_y;
		first_motion = true;
		m_scroll_dragging = true;
		
	} else if (event->type == GDK_MOTION_NOTIFY && m_scroll_dragging) {
		// These are world-relative coordinates
		double x = event->motion.x_root;
		double y = event->motion.y_root;
		
		//c2w(x, y, x, y);
		//world_to_window(x, y, x, y);
		//window_to_world(event->button.x, event->button.y, x, y);
		//w2c(x, y, x, y);

		x_offset += last_x - x;//x + original_scroll_x;
		y_offset += last_y - y;// + original_scroll_y;
		
		//cerr << "Coord: (" << x << "," << y << ")\n";
		//cerr << "Offset: (" << x_offset << "," << y_offset << ")\n";

		int temp_x;
		int temp_y;
		w2c(lrint(x_offset), lrint(y_offset),
			temp_x, temp_y);
		scroll_offset_x += temp_x;
		scroll_offset_y += temp_y;
		scroll_to(scroll_offset_x,
		          scroll_offset_y);
		last_x = x;
		last_y = y;
	} else if (event->type == GDK_BUTTON_RELEASE && m_scroll_dragging) {
		m_base_rect.ungrab(event->button.time);
		m_scroll_dragging = false;
	} else {
		handled = false;
	}

	return handled;
	return false;
}
*/


bool
FlowCanvas::select_drag_handler(GdkEvent* event)
{
	Module* module = NULL;
	
	if (event->type == GDK_BUTTON_PRESS && event->button.button == 1) {
		assert(m_select_rect == NULL);
		m_drag_state = SELECT;
		if ( !(event->button.state & GDK_CONTROL_MASK))
			clear_selection();
		m_select_rect = new Gnome::Canvas::Rect(*root(),
			event->button.x, event->button.y, event->button.x, event->button.y);
		m_select_rect->property_fill_color_rgba() = 0x273344FF;
		m_select_rect->property_outline_color_rgba() = 0xEEEEFFFF;
		m_select_rect->lower_to_bottom();
		m_base_rect.lower_to_bottom();
		return true;
	} else if (event->type == GDK_MOTION_NOTIFY && m_drag_state == SELECT) {
		assert(m_select_rect != NULL);
		m_select_rect->property_x2() = event->button.x;
		m_select_rect->property_y2() = event->button.y;
		return true;
	} else if (event->type == GDK_BUTTON_RELEASE && m_drag_state == SELECT) {
		// Select all modules within rect
		for (ModuleMap::iterator i = m_modules.begin(); i != m_modules.end(); ++i) {
			module = (*i).second;
			if (module->is_within(m_select_rect)) {
				if (module->selected())
					unselect_module(module);
				else
					select_module(module);
			}
		}
		
		delete m_select_rect;
		m_select_rect = NULL;
		m_drag_state = NOT_DRAGGING;
		return true;
	}
	return false;
}


/** Updates m_select_dash for rotation effect, and updates any
  * selected item's borders (and the selection rectangle).
  */
bool
FlowCanvas::animate_selected()
{
	static int i = 10;

	if (--i == 0)
		i = 10;

	m_select_dash->offset = i;
	
	if (m_select_rect != NULL)
		m_select_rect->property_dash() = m_select_dash;
	
	for (list<Module*>::iterator m = m_selected_modules.begin(); m != m_selected_modules.end(); ++m)
		(*m)->rect()->property_dash() = m_select_dash;
	
	for (list<Connection*>::iterator c = m_selected_connections.begin(); c != m_selected_connections.end(); ++c)
		(*c)->property_dash() = m_select_dash;
	
	return true;
}


bool
FlowCanvas::connection_drag_handler(GdkEvent* event)
{
	bool handled = true;
	
	// These are invisible, just used for making connections (while dragging)
	static Module*      drag_module = NULL;
	static Port*        drag_port = NULL;
	
	static Connection*  drag_connection = NULL;
	static Port*        snapped_port = NULL;

	static bool snapped = false;
	
	if (event->type == GDK_BUTTON_PRESS && event->button.button == 2) {
		m_drag_state = SCROLL;
	} else if (event->type == GDK_MOTION_NOTIFY && m_drag_state == CONNECTION) {
		double x = event->button.x, y = event->button.y;
		root()->w2i(x, y);

		if (drag_connection == NULL) { // Havn't created the connection yet
			assert(drag_port == NULL);
			assert(m_connect_port != NULL);
			
			drag_module = new Module(this, "");
			bool drag_port_is_input = true;
			if (m_connect_port->is_input())
				drag_port_is_input = false;
				
			drag_port = new Port(drag_module, "", drag_port_is_input, m_connect_port->colour());
			drag_module->add_port(drag_port);

			//drag_port->hide();
			drag_module->hide();

			drag_module->move_to(x, y);
			
			drag_port->property_x() = 0;
			drag_port->property_y() = 0;
			drag_port->rect()->property_x2() = 1;
			drag_port->rect()->property_y2() = 1;
			if (drag_port_is_input)
				drag_connection = new Connection(this, m_connect_port, drag_port);
			else
				drag_connection = new Connection(this, drag_port, m_connect_port);
				
			drag_connection->update_location();
			//drag_connection->property_line_style() = Gdk::LINE_DOUBLE_DASH;
			//drag_connection->property_last_arrowhead() = true;
		}

		if (snapped) {
			if (drag_connection != NULL) drag_connection->hide();
			Port* p = get_port_at(x, y);
			if (drag_connection != NULL) drag_connection->show();
			if (p != NULL) {
				if (p != m_selected_port) {
					if (snapped_port != NULL)
						snapped_port->hilite(false);
					p->hilite(true);
					snapped_port = p;
				}
				drag_module->property_x() = p->module()->property_x().get_value();
				drag_module->rect()->property_x2() = p->module()->rect()->property_x2().get_value();
				drag_module->property_y() = p->module()->property_y().get_value();
				drag_module->rect()->property_y2() = p->module()->rect()->property_y2().get_value();
				drag_port->property_x() = p->property_x().get_value();
				drag_port->property_y() = p->property_y().get_value();
			} else {  // off the port now, unsnap
				if (snapped_port != NULL)
					snapped_port->hilite(false);
				snapped_port = NULL;
				snapped = false;
				drag_module->property_x() = x;
				drag_module->property_y() = y;
				drag_port->property_x() = 0;
				drag_port->property_y() = 0;
				drag_port->rect()->property_x2() = 1;
				drag_port->rect()->property_y2() = 1;
			}
			drag_connection->update_location();
		} else { // not snapped to a port
			assert(drag_module != NULL);
			assert(drag_port != NULL);
			assert(m_connect_port != NULL);

			// "Snap" to port, if we're on a port and it's the right direction
			if (drag_connection != NULL) drag_connection->hide();
			Port* p = get_port_at(x, y);
			if (drag_connection != NULL) drag_connection->show();
			if (p != NULL && p->is_input() != m_connect_port->is_input()) {
				p->hilite(true);
				snapped_port = p;
				snapped = true;
				// Make drag module and port exactly the same size/loc as the snapped
				drag_module->move_to(p->module()->property_x().get_value(), p->module()->property_y().get_value());
				drag_module->width(p->module()->width());
				drag_module->height(p->module()->height());
				drag_port->property_x() = p->property_x().get_value();
				drag_port->property_y() = p->property_y().get_value();
				// Make the drag port as wide as the snapped port so the connection coords are the same
				drag_port->rect()->property_x2() = p->rect()->property_x2().get_value();
				drag_port->rect()->property_y2() = p->rect()->property_y2().get_value();
			} else {
				drag_module->property_x() = x;
				drag_module->property_y() = y;
			}
			drag_connection->update_location();
		}
	} else if (event->type == GDK_BUTTON_RELEASE && m_drag_state == CONNECTION) {
		m_base_rect.ungrab(event->button.time);
		
		double x = event->button.x;
		double y = event->button.y;
		m_base_rect.i2w(x, y);

		if (drag_connection != NULL) drag_connection->hide();
		Port* p = get_port_at(x, y);
		if (drag_connection != NULL) drag_connection->show();
	
		if (p != NULL) {
			if (p == m_connect_port) {   // drag ended on same port it started on
				if (m_selected_port == NULL) {  // no active port, just activate (hilite) it
					selected_port(m_connect_port);
				} else {  // there is already an active port, connect it with this one
					if (m_selected_port != m_connect_port)
						ports_joined(m_selected_port, m_connect_port);
					selected_port(NULL);
					m_connect_port = NULL;
					snapped_port = NULL;
				}
			} else {  // drag ended on different port
				//p->hilite(false);
				ports_joined(m_connect_port, p);
				selected_port(NULL);
				m_connect_port = NULL;
				snapped_port = NULL;
			}
		}
		
		// Clean up dragging stuff
		if (m_connect_port != NULL)
			m_connect_port->hilite(false);

		m_drag_state = NOT_DRAGGING;
		delete drag_connection;
		drag_connection = NULL;
		//delete drag_port;
		drag_port = NULL;
		delete drag_module; // deletes drag_port
		drag_module = NULL;
		snapped_port = NULL;
	} else {
		handled = false;
	}

	return handled;
}


Port*
FlowCanvas::get_port_at(double x, double y)
{
	Gnome::Canvas::Item* item = get_item_at(x, y);
	if (item == NULL) return NULL;
	
	Port* p = NULL;
	// Loop through every port and see if the item at these coordinates is that port
	// yes, this is disgusting ;)
	for (ModuleMap::iterator i = m_modules.begin(); i != m_modules.end(); ++i) {
		for (PortList::iterator j = (*i).second->ports().begin(); j != (*i).second->ports().end(); ++j) {
			p = (*j);
			
			if ((Gnome::Canvas::Item*)p == item
					|| (Gnome::Canvas::Item*)(p->rect()) == item
					|| (Gnome::Canvas::Item*)(p->label()) == item) {
				return p;
			}
		}
	}
	return NULL;
}

/*
void
FlowCanvas::port_menu_disconnect_all()
{
	Connection* c = NULL;
	list<Connection*> temp_list = m_selected_port->connections();
	for (list<Connection*>::iterator i = temp_list.begin(); i != temp_list.end(); ++i) {
		c = *i;
		disconnect(c->source_port(), c->dest_port());
	}
	
	selected_port(NULL);
}
*/

} // namespace LibFlowCanvas
