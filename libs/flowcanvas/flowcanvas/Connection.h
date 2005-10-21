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


#ifndef CONNECTION_H
#define CONNECTION_H

#include <libgnomecanvasmm.h>
#include <libgnomecanvasmm/bpath.h>
#include <libgnomecanvasmm/path-def.h>
#include <list>
#include "Port.h"

using std::list;

namespace LibFlowCanvas {

class FlowCanvas;


/** A connection between two ports.
 *
 * \ingroup FlowCanvas
 */
class Connection : public Gnome::Canvas::Bpath
{
public:
	Connection(FlowCanvas* patch_bay, Port* source_port, Port* dest_port);
	virtual ~Connection();
	
	void update_location();
	void disconnect();
	void hilite(bool b);
	
	bool selected()        { return m_selected; }
	void selected(bool b);
	
	void        source_port(Port* p) { m_source_port = p; }
	const Port* source_port() const  { return m_source_port; }
	void        dest_port(Port* p)   { m_dest_port = p; }
	const Port* dest_port() const    { return m_dest_port; }

private:
	FlowCanvas* m_patch_bay;
	Port*         m_source_port;
	Port*         m_dest_port;
	int           m_colour;
	bool          m_selected;

	//Glib::RefPtr<Gnome::Canvas::PathDef> m_path;
	GnomeCanvasPathDef* m_path;
};

typedef list<Connection*> ConnectionList;


} // namespace LibFlowCanvas

#endif // CONNECTION_H
