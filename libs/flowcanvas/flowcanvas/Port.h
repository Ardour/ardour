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


#ifndef PORT_H
#define PORT_H

#include <string>
#include <list>
#include <libgnomecanvasmm.h>

using std::string; using std::list;

namespace LibFlowCanvas {
	
class Connection;
class Module;


static const int PORT_LABEL_SIZE = 8000; // in thousandths of a point


/** A port on a module on the canvas.
 *
 * This is a group that contains both the label and rectangle for a port.
 *
 * \ingroup FlowCanvas
 */
class Port : public Gnome::Canvas::Group
{
public:
	Port(Module* module, const string& name, bool is_input, int colour);
	virtual ~Port() {};
	
	void add_connection(Connection* c) { m_connections.push_back(c); }
	void remove_connection(Connection* c);
	void move_connections();
	void raise_connections();
	void disconnect_all();
	
	Gnome::Art::Point connection_coords();
	
	void hilite(bool b);
	void zoom(float z);

	void popup_menu(guint button, guint32 activate_time) {
		m_menu.popup(button, activate_time);
	}
	
	Module*              module() const         { return m_module; }
	list<Connection*>&   connections()          { return m_connections; }
	Gnome::Canvas::Rect* rect()                 { return &m_rect; }
	Gnome::Canvas::Text* label()                { return &m_label; }
	bool                 is_input() const       { return m_is_input; }
	bool                 is_output() const      { return !m_is_input; }
	int                  colour() const         { return m_colour; }
	double               border_width() const   { return m_border_width; }
	void                 border_width(double w);

	const string& name() const           { return m_name; }
	virtual void  name(const string& n);
	
	double width() const    { return m_width; }
	void   width(double w);
	
	double height() const { return m_height; }

protected:
	Module* m_module;
	string  m_name;
	bool    m_is_input;
	double  m_width;
	double  m_height;
	double  m_border_width;
	int     m_colour;
	
	list<Connection*> m_connections; // needed for dragging
	
	Gnome::Canvas::Text m_label;
	Gnome::Canvas::Rect m_rect;
	Gtk::Menu           m_menu;
};


typedef list<Port*> PortList;

} // namespace LibFlowCanvas

#endif // PORT_H
