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


#ifndef FLOWCANVAS_H
#define FLOWCANVAS_H

#include <string>
#include <list>
#include <libgnomecanvasmm.h>
#include "Connection.h"
#include "Module.h"

using std::string;
using std::list;


/** FlowCanvas namespace, everything is defined under this.
 *
 * \ingroup FlowCanvas
 */
namespace LibFlowCanvas {
	
class Port;
class Module;


/** \defgroup FlowCanvas Canvas widget for dataflow systems.
 *
 * A generic dataflow widget using libgnomecanvas.  It's used by Om and
 * Patchage, but could be used by anyone.
 */


/** The canvas widget.
 *
 * Applications must override some virtual methods to make the widget actually
 * do anything (ie connect).
 *
 * \ingroup FlowCanvas
 */
#ifdef ANTI_ALIAS
class FlowCanvas : public Gnome::Canvas::CanvasAA
#else
class FlowCanvas : public Gnome::Canvas::Canvas
#endif
{
public:
	FlowCanvas(double width, double height);
	virtual	~FlowCanvas();

	void destroy();
	
	void add_module(Module* m);
	void remove_module(const string& name);
		
	void add_connection(const string& mod1_name, const string& port1_name, const string& mod2_name, const string& port2_name);
	bool remove_connection(const string& mod1_name, const string& port1_name, const string& mod2_name, const string& port2_name);

	void add_connection(Port* port1, Port* port2);
	bool remove_connection(Port* port1, Port* port2);
	
	Module* find_module(const string& name);
	Port*   find_port(const string& module_name, const string& port_name);
	
	void rename_module(const string& old_name, const string& new_name);
	
	void set_default_placement(Module* m);
	
	float zoom()                    { return m_zoom; }
	void  zoom(float pix_per_unit);
	
	double width() const  { return m_width; }
	double height() const { return m_height; }

	void clear_selection();
	void select_module(Module* m);
	void unselect_module(Module* m);

	ModuleMap&         modules()              { return m_modules; }
	list<Module*>&     selected_modules()     { return m_selected_modules; }
	list<Connection*>& selected_connections() { return m_selected_connections; }
	
	/** Dash applied to selected items.
	 * Always animating, set a rect's property_dash() to this and it
	 * will automagically do the rubber band thing. */
	ArtVpathDash* const select_dash() { return m_select_dash; }

	virtual bool port_event(GdkEvent* event, Port* port);
	
	/** Make a connection.  Should be overridden by an implementation to do something. */
	virtual void connect(const Port* const port1, const Port* const port2) = 0;
	
	/** Disconnect two ports.  Should be overridden by an implementation to do something */
	virtual void disconnect(const Port* const port1, const Port* const port2) = 0;

protected:
	ModuleMap		  m_modules;              ///< All modules on this canvas
	ConnectionList	  m_connections;          ///< All connections on this canvas
	list<Module*>     m_selected_modules;     ///< All currently selected modules
	list<Connection*> m_selected_connections; ///< All currently selected connections

	virtual bool canvas_event(GdkEvent* event) { return false; } 
	
private:
	Connection* get_connection(const Port* port1, const Port* port2);
	void        remove_connection(Connection* c);
	void        selected_port(Port* p);
	Port*       selected_port() { return m_selected_port; }
	bool        are_connected(const Port* port1, const Port* port2);
	Port*       get_port_at(double x, double y);

	//bool scroll_drag_handler(GdkEvent* event);
	virtual bool select_drag_handler(GdkEvent* event);
	virtual bool connection_drag_handler(GdkEvent* event);
	
	void ports_joined(Port* port1, Port* port2);
	bool animate_selected();

	Port*  m_selected_port; ///< Selected port (hilited red from clicking once)
	Port*  m_connect_port;  ///< Port for which a connection is being made (if applicable)
	
	float  m_zoom;   ///< Current zoom level
	double m_width;  
	double m_height; 

	enum DragState { NOT_DRAGGING, CONNECTION, SCROLL, SELECT };
	DragState      m_drag_state;
	
	Gnome::Canvas::Rect  m_base_rect; ///< Background

	Gnome::Canvas::Rect* m_select_rect;          ///< Rectangle for drag selection
	ArtVpathDash*        m_select_dash;          ///< Animated selection dash style
};


} // namespace LibFlowCanvas

#endif // FLOWCANVAS_H
