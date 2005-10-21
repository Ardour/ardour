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


#ifndef MODULE_H
#define MODULE_H

#include <string>
#include <map>
#include <libgnomecanvasmm.h>
#include "Port.h"

using std::string; using std::multimap;

namespace LibFlowCanvas {
	
class FlowCanvas;


/** A module on the canvas.
 *
 * \ingroup FlowCanvas
 */
class Module : public Gnome::Canvas::Group
{
public:
	Module(FlowCanvas* patch_bay, const string& name, double x=0, double y=0);
	virtual ~Module();
	
	inline Port* const port(const string& port_name) const;

	void add_port(Port* port, bool resize=true);
	void remove_port(const string& port_name, bool resize = true);
	//virtual void add_port(const string& port_name, bool is_input, int colour, bool resize = true);

	void zoom(float z);
	void resize();
	
	void         move(double dx, double dy);
	virtual void move_to(double x, double y);
	
	bool is_within(const Gnome::Canvas::Rect* const rect);

	virtual void load_location()            {}
	virtual void store_location()           {}
	virtual void on_double_click()          {}
	virtual void show_menu(GdkEventButton*) {}

	// For connection drawing
	double port_connection_point_offset(Port* port);
	double port_connection_points_range();
	
	double width()           { return m_width; }
	void   width(double w);
	double height()          { return m_height; }
	void   height(double h);

	void hilite(bool b);
	void selected(bool b);
	bool selected() const  { return m_selected; }
	
	virtual void  name(const string& n);
	const string& name() const            { return m_name; }
	
	FlowCanvas*   patch_bay()  const      { return m_patch_bay; }
	int           num_ports()  const      { return m_ports.size(); }
	int           base_colour() const     { return 0x1F2A3CFF; }
	PortList&     ports()                 { return m_ports; }
	double        border_width() const    { return m_border_width; }
	void          border_width(double w);
	
	Gnome::Canvas::Rect* rect()  { return &m_module_box; }
	Gnome::Canvas::Text* title() { return &m_canvas_title; }

protected:

	bool module_event(GdkEvent* event);
	
	double m_border_width;
	double m_width;
	double m_height;
	string m_name;
	bool   m_selected;

	FlowCanvas* m_patch_bay;
	PortList    m_ports;

	Gnome::Canvas::Rect m_module_box;
	Gnome::Canvas::Text m_canvas_title;
};


typedef multimap<string,Module*> ModuleMap;



/** Find a port on this module.
 *
 * Profiling has shown this to be performance critical, hence the inlining.
 * Making this faster would be a very good idea - better data structure?
 */
inline Port* const
Module::port(const string& port_name) const
{
	for (PortList::const_iterator i = m_ports.begin(); i != m_ports.end(); ++i)
		if ((*i)->name() == port_name)
			return (*i);
	return NULL;
}



} // namespace LibFlowCanvas

#endif // MODULE_H
