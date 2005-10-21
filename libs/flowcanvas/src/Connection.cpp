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

#include "flowcanvas/Connection.h"
#include <cassert>
#include <math.h>
#include <libgnomecanvasmm/libgnomecanvasmm.h>
#include "flowcanvas/FlowCanvas.h"

// FIXME: remove
#include <iostream>
using std::cerr; using std::endl;

namespace LibFlowCanvas {
	

Connection::Connection(FlowCanvas* patch_bay, Port* source_port, Port* dest_port)
: Gnome::Canvas::Bpath(*patch_bay->root()),
  m_patch_bay(patch_bay),
  m_source_port(source_port),
  m_dest_port(dest_port),
  m_selected(false),
//  m_path(Gnome::Canvas::PathDef::create())
  m_path(gnome_canvas_path_def_new())
{
	assert(m_source_port->is_output());
	assert(m_dest_port->is_input());
	
	m_colour = m_source_port->colour() + 0x44444400;
	property_width_units() = 1.0;
	property_outline_color_rgba() = m_colour;
	property_cap_style() = (Gdk::CapStyle)GDK_CAP_ROUND;

	update_location();	
}


Connection::~Connection()
{
	if (m_selected) {
		for (list<Connection*>::iterator c = m_patch_bay->selected_connections().begin();
				c != m_patch_bay->selected_connections().end(); ++c)
		{
			if ((*c) == this) {
				m_patch_bay->selected_connections().erase(c);
				break;
			}
		}
	}
}


#undef MIN
#define MIN(x,y) (((x) < (y)) ? (x) : (y))

#undef MAX
#define MAX(x,y) (((x) > (y)) ? (x) : (y))

#if 0									   
/** Updates the connection's location to match it's source/dest ports.
 *
 * This is used when modules are dragged, to keep the connections attached
 * to their ports.
 *
 * You are not expected to understand this.
 */
void
Connection::update_location()
{
	const double src_x = m_source_port->connection_coords().get_x();
	const double src_y = m_source_port->connection_coords().get_y();
	const double dst_x = m_dest_port->connection_coords().get_x();
	const double dst_y = m_dest_port->connection_coords().get_y();
	
	const double src_mod_x = m_source_port->module()->property_x();
	const double src_mod_y = m_source_port->module()->property_y();
	const double src_mod_w = m_source_port->module()->width();
	const double src_mod_h = m_source_port->module()->height();
	const double dst_mod_x = m_dest_port->module()->property_x();
	const double dst_mod_y = m_dest_port->module()->property_y();
	const double dst_mod_w = m_dest_port->module()->width();
	const double dst_mod_h = m_dest_port->module()->height();

	// Y Modifier (-1 if src module is below dst module)
	double y_mod = (src_y < dst_y) ? 1.0 : -1.0;

	// Added in various places to keep things parallel
	/*double src_port_offset = src_y - src_mod_y - src_title_h;
	double dst_port_offset = dst_y - dst_mod_y - dst_title_h;
	if (y_mod < 0.0) {
		src_port_offset = src_mod_y + src_mod_h - src_y;
		dst_port_offset = dst_mod_y + dst_mod_h - dst_y;
	}*/
	double src_port_offset = m_source_port->module()->port_connection_point_offset(m_source_port);
	double src_offset_range = m_source_port->module()->port_connection_points_range();
	double dst_port_offset = m_dest_port->module()->port_connection_point_offset(m_dest_port);
	double dst_offset_range = m_dest_port->module()->port_connection_points_range();
	
	/*
	double smallest_offset = (src_port_offset < dst_port_offset)
		? src_port_offset : dst_port_offset;

	double smallest_offset_range = (src_port_offset < dst_port_offset)
		? m_source_port->module()->port_connection_points_range()
		: m_dest_port->module()->port_connection_points_range();
	*/
	double smallest_offset = (src_offset_range < dst_offset_range)
			? src_port_offset : dst_port_offset;

	double smallest_offset_range = (src_offset_range < dst_offset_range)
			? m_source_port->module()->port_connection_points_range()
	  		: m_dest_port->module()->port_connection_points_range();

	//double largest_offset_range = (src_offset_range > dst_offset_range)
	//		? m_source_port->module()->port_connection_points_range()
	 //		: m_dest_port->module()->port_connection_points_range();
	
	double x_dist = fabs(dst_x - src_x);
	double y_dist = fabs(dst_y - src_y);

	// Vertical distance between modules
	double y_mod_dist = dst_mod_y - src_mod_y - src_mod_h;
	if (dst_y < src_y)
		y_mod_dist = src_mod_y - dst_mod_y - dst_mod_h;
	if (y_mod_dist < 1.0)
		y_mod_dist = 1.0;

	// Horizontal distance between modules
	double x_mod_dist = dst_mod_x - src_mod_x - src_mod_w;
	if (src_x > dst_x + src_mod_w)
		x_mod_dist = src_mod_x - dst_mod_x - dst_mod_w;
	if (x_mod_dist < 1.0)
		x_mod_dist = 1.0;

	double tallest_mod_h = m_source_port->module()->height();
	if (m_dest_port->module()->height() > tallest_mod_h)
		tallest_mod_h = m_dest_port->module()->height();
	
	double src_x1, src_y1, src_x2, src_y2, join_x, join_y; // Path 1
	double dst_x2, dst_y2, dst_x1, dst_y1;                 // Path 2

	src_x1 = src_y1 = src_x2 = src_y2 = join_x = join_y = dst_x2 = dst_y2 = dst_x1 = dst_y1 = 0.0;

	static const double join_range = 15.0;
	
	double src_offset = (src_y < dst_y)
		? src_port_offset : src_offset_range - src_port_offset;
	double dst_offset = (src_y < dst_y)
		? dst_port_offset : dst_offset_range - dst_port_offset;

	
	// Wrap around connections
	if (dst_x < src_x && y_mod_dist < join_range*3.0) {
		
		static const double module_padding = 20.0;
		
		// FIXME: completely different meanings in this case than the normal case
		// (this one is better though)
		smallest_offset = (src_offset_range < dst_offset_range)
			? src_offset : dst_offset;

		// Limit straight out distance
		if (x_dist > 60.0)
			x_dist = 60.0;
		if (x_dist < 80.0 && y_dist > 40.0)
			x_dist = 80.0;
		
		// Calculate join point
		join_x = dst_mod_x + dst_mod_w + x_mod_dist/2.0;
		join_y = (src_y < dst_y)
			? MIN(src_mod_y, dst_mod_y)
			: MAX(src_mod_y + src_mod_h, dst_mod_y + dst_mod_h);
		join_y -= (smallest_offset/smallest_offset_range*join_range + module_padding) * y_mod;

		if (join_x > src_mod_x)
			join_x = src_mod_x;
	
		// Path 1 (src_x, src_y) -> (join_x, join_y)
		src_x1 = src_x + x_dist/5.0 + src_offset/src_offset_range*join_range;
		src_y1 = src_y - (x_dist/3.0 + src_offset) * y_mod;
		src_x2 = src_x + x_dist/3.0 + src_offset/src_offset_range*join_range;
		src_y2 = join_y;
	
		// Path 2, (join_x, join_y) -> (dst_x, dst_y)
		dst_x1 = MIN(dst_x, src_mod_x) - x_dist/5.0 - dst_offset/dst_offset_range*join_range;
		dst_y1 = MIN(dst_y, src_mod_y + src_mod_h) - (x_dist/3.0 + dst_offset) * y_mod;
		dst_x2 = MIN(dst_x, src_mod_x) - x_dist/3.0 - dst_offset/dst_offset_range*join_range;
		dst_y2 = join_y;

	
	// Curve through connections and normal (left->right) connections
	// (These two cases are continuous)
	} else {
		/* The trick with this one is to define each curve's points exclusively in terms
		 * of the join point (ie nothing about the other module), and choose the join point
		 * cleverly */
		
		// Calculate join point
		double ratio = (x_dist - y_dist) / (y_dist + x_dist);
		join_x = (src_x + dst_x)/2.0;
		join_y = (src_y + dst_y)/2.0;
	
		// Vertical centre point between the modules
		join_y = (src_y < dst_y)
			? (dst_mod_y - (dst_mod_y - (src_mod_y + src_mod_h)) / 2.0)
			: (src_mod_y - (src_mod_y - (dst_mod_y + dst_mod_h)) / 2.0);
		
		join_y -= (smallest_offset / smallest_offset_range * join_range) - join_range/2.0;
		
		// Interpolate between (src_x < dst_x) case and (src_y == dst_y) case
		if (src_x < dst_x && x_dist > y_dist) {
				join_y *= (1.0-ratio);
				join_y += (src_y + dst_y)/2.0 * ratio;
		}
			
		if (src_x < dst_x) {
			join_y += ((smallest_offset/smallest_offset_range)) * join_range * (1.0-fabs(ratio));
			join_x -= ((smallest_offset/smallest_offset_range)) * join_range * (1.0-fabs(ratio)) * y_mod;
		}
		
		//cerr << "ratio: " << ratio << endl;
			
		// Curve through connections
		if (dst_x < src_x) {
			double src_x_offset = fabs(src_x - join_x)/8.0 + src_offset_range - src_offset/src_offset_range*join_range;
			double dst_x_offset = fabs(dst_x - join_x)/8.0 + dst_offset_range + dst_offset/dst_offset_range*join_range;
			double src_y_offset = fabs(src_y - join_y)/4.0 + src_offset/src_offset_range*(src_offset_range+join_range)/2.0;
			double dst_y_offset = fabs(dst_y - join_y)/2.0 + (dst_offset_range-dst_offset)/dst_offset_range*(dst_offset_range+join_range)/2.0;

			// Path 1 (src_x, src_y) -> (join_x, join_y)
			src_x1 = src_x + src_x_offset;
			src_y1 = join_y - src_y_offset * y_mod;
			src_x2 = src_x + src_x_offset;
			src_y2 = join_y;
		
			// Path 2, (join_x, join_y) -> (dst_x, dst_y)
			dst_x1 = dst_x - dst_x_offset;
			dst_y1 = join_y + dst_y_offset * y_mod;
			dst_x2 = dst_x - dst_x_offset;
			dst_y2 = join_y;
			
		// Normal connections
		} else {
			double src_x_offset = fabs(src_x - join_x)/8.0 + src_offset_range - src_offset/src_offset_range*join_range;
			double dst_x_offset = fabs(dst_x - join_x)/8.0 + dst_offset_range + dst_offset/dst_offset_range*join_range;
			double src_y_offset = fabs(src_y - join_y)/4.0 + src_offset/src_offset_range*(src_offset_range+join_range)/2.0;
			double dst_y_offset = fabs(dst_y - join_y)/2.0 + (dst_offset_range-dst_offset)/dst_offset_range*(dst_offset_range+join_range)/2.0;
			
			// Path 1 (src_x, src_y) -> (join_x, join_y)
			src_x1 = src_x + src_x_offset;
			// Interpolate from curve through case
			if (x_dist < y_dist) {
				// Smooth transition from (src_y) to (join_y - src_y_offset * y_mod)
				src_y1 = (src_y * (1.0-fabs(ratio))) + ((join_y - src_y_offset * y_mod) * -ratio);
				// Smooth transition from (join_x + src_offset_range/4.0 * -ratio) to (src_x + src_x_offset)
				src_x2 = (join_x + src_offset_range/4.0 * -ratio)*(1.0-fabs(ratio)) + (src_x + src_x_offset)*-ratio;
			} else {
				src_y1 = src_y;
				src_x2 = join_x + src_offset_range/4.0 * -ratio;
			}
			src_y2 = join_y - src_y_offset*(1.0-fabs(ratio)) * y_mod;
			
			// Path 2, (join_x, join_y) -> (dst_x, dst_y)
			dst_x1 = dst_x - dst_x_offset;
			dst_y1 = dst_y;
			// Interpolate from curve through case
			if (x_dist < y_dist) {
				// Smooth transition from (dst_y) to (join_y - dst_y_offset * y_mod)
				dst_y1 = (dst_y * (1.0-fabs(ratio))) + ((join_y + dst_y_offset * y_mod) * -ratio);
				// Smooth transition from (join_x + dst_offset_range/4.0 * -ratio) to (dst_x + dst_x_offset)
				dst_x2 = (join_x - dst_offset_range/4.0 * -ratio)*(1.0-fabs(ratio)) + (dst_x - dst_x_offset)*-ratio;
			} else {
				dst_y1 = dst_y;
				dst_x2 = join_x - dst_offset_range/4.0 * -ratio;
			}
			dst_y2 = join_y + dst_y_offset*(1.0-fabs(ratio)) * y_mod;
		}
		/*
		} else {
			double src_x_offset = fabs(src_x - join_x)/4.0 + src_offset_range*2.0 - src_offset;
			double dst_x_offset = fabs(dst_x - join_x)/4.0 + dst_offset + dst_offset_range;
			double y_offset = fabs(join_y - src_y)/2.0;
			
			// Path 1 (src_x, src_y) -> (join_x, join_y)
			src_x1 = src_x + src_x_offset/2.0;
			src_y1 = src_y;
			if (x_dist < y_dist)
				src_x2 = join_x + src_x_offset * -ratio;
			else
				src_x2 = join_x + src_offset_range/4.0 * -ratio;
			src_y2 = join_y - y_offset*(1.0-fabs(ratio)) * y_mod;
			
			// Path 2, (join_x, join_y) -> (dst_x, dst_y)
			dst_x1 = dst_x - dst_x_offset/2.0;
			dst_y1 = dst_y;
			if (x_dist < y_dist)
				dst_x2 = join_x - dst_x_offset * -ratio;
			else
				dst_x2 = join_x - dst_x_offset/4.0 * -ratio;
			dst_y2 = join_y + y_offset*(1.0-fabs(ratio)) * y_mod;
		}*/
	}
	
	/*
	cerr << "src_x1: " << src_x1 << endl;
	cerr << "src_y1: " << src_y1 << endl;
	cerr << "src_x2: " << src_x2 << endl;
	cerr << "src_y2: " << src_x2 << endl;
	cerr << "join_x: " << join_x << endl;
	cerr << "join_y: " << join_y << endl;
	cerr << "dst_x1: " << dst_x1 << endl;
	cerr << "dst_y1: " << dst_y1 << endl;
	cerr << "dst_x2: " << dst_x2 << endl;
	cerr << "dst_y2: " << dst_x2 << endl << endl;
	*/

	m_path->reset();
	
	//m_path->moveto(0,0);
	
	
	m_path->moveto(src_x, src_y);
	//m_path->lineto(join_x, join_y);
	m_path->curveto(src_x1, src_y1, src_x2, src_y2, join_x, join_y);
	m_path->curveto(dst_x2, dst_y2, dst_x1, dst_y1, dst_x, dst_y);
	set_bpath(m_path);
}
#endif


/** Updates the path of the connection to match it's ports if they've moved.
 */
void
Connection::update_location()
{
	const double src_x = m_source_port->connection_coords().get_x();
	const double src_y = m_source_port->connection_coords().get_y();
	const double dst_x = m_dest_port->connection_coords().get_x();
	const double dst_y = m_dest_port->connection_coords().get_y();
	
	const double src_mod_x = m_source_port->module()->property_x();
	const double src_mod_y = m_source_port->module()->property_y();
	const double src_mod_w = m_source_port->module()->width();
	const double src_mod_h = m_source_port->module()->height();
	const double dst_mod_x = m_dest_port->module()->property_x();
	const double dst_mod_y = m_dest_port->module()->property_y();
	const double dst_mod_w = m_dest_port->module()->width();
	const double dst_mod_h = m_dest_port->module()->height();
	
	// Vertical distance between modules
	double y_mod_dist = dst_mod_y - src_mod_y - src_mod_h;
	if (dst_y < src_y)
		y_mod_dist = src_mod_y - dst_mod_y - dst_mod_h;
	if (y_mod_dist < 1.0)
		y_mod_dist = 1.0;

	// Horizontal distance between modules
	double x_mod_dist = dst_mod_x - src_mod_x - src_mod_w;
	if (src_x > dst_x + src_mod_w)
		x_mod_dist = src_mod_x - dst_mod_x - dst_mod_w;
	if (x_mod_dist < 1.0)
		x_mod_dist = 1.0;

	// Y Modifier (-1 if src module is below dst module)
	double y_mod = (src_y < dst_y) ? 1.0 : -1.0;

	double x_dist = fabsl(src_x - dst_x);
	double y_dist = fabsl(src_y - dst_y);
	
	double src_port_offset = m_source_port->module()->port_connection_point_offset(m_source_port);
	double src_offset_range = m_source_port->module()->port_connection_points_range();
	double dst_port_offset = m_dest_port->module()->port_connection_point_offset(m_dest_port);
	double dst_offset_range = m_dest_port->module()->port_connection_points_range();

	double smallest_offset = (src_offset_range < dst_offset_range)
			? src_port_offset : dst_port_offset;

	double smallest_offset_range = (src_offset_range < dst_offset_range)
			? m_source_port->module()->port_connection_points_range()
	  		: m_dest_port->module()->port_connection_points_range();

	double tallest_module_height = m_source_port->module()->height();
	if (m_dest_port->module()->height() > tallest_module_height)
		tallest_module_height = m_dest_port->module()->height();
	
	double src_x1, src_y1, src_x2, src_y2, join_x, join_y; // Path 1
	double dst_x2, dst_y2, dst_x1, dst_y1;                 // Path 2
	src_x1 = src_y1 = src_x2 = src_y2 = join_x = join_y = dst_x2 = dst_y2 = dst_x1 = dst_y1 = 0.0;
	
	double join_range = 20.0;

	double src_offset = (src_y < dst_y)
		? src_port_offset : src_offset_range - src_port_offset;
	double dst_offset = (src_y < dst_y)
		? dst_port_offset : dst_offset_range - dst_port_offset;
		
	// Wrap around connections
	if ((src_x > dst_x)
		   && y_mod_dist < join_range*3.0
		   && ((dst_x > src_mod_x+src_mod_w+join_range*2.0) || src_x > dst_x)
		   && (! ((src_mod_y + src_mod_h < dst_mod_y - join_range*3.0) || (dst_mod_y + dst_mod_h < src_mod_h - join_range*3.0)))) {
		//|| (dst_x < src_x)) {
		
		static const double module_padding = 20.0;

		// FIXME: completely different meanings in this case than the normal case
		// (this one is better though)
		smallest_offset = (src_offset_range < dst_offset_range)
			? src_offset : dst_offset;

		// Limit straight out distance
		if (x_dist > 60.0)
			x_dist = 60.0;
		if (x_dist < 80.0 && y_dist > 40.0)
			x_dist = 80.0;
		
		// Calculate join point
		join_x = dst_mod_x + dst_mod_w + x_mod_dist/2.0;
		join_y = (src_y < dst_y)
			? MIN(src_mod_y, dst_mod_y)
			: MAX(src_mod_y + src_mod_h, dst_mod_y + dst_mod_h);
		join_y -= (smallest_offset/smallest_offset_range*join_range + module_padding) * y_mod;

		if (join_x > src_mod_x)
			join_x = src_mod_x;
	
		// Path 1 (src_x, src_y) -> (join_x, join_y)
		src_x1 = src_x + x_dist/5.0 + src_offset/src_offset_range*join_range;
		src_y1 = src_y - (x_dist/3.0 + src_offset) * y_mod;
		src_x2 = src_x + x_dist/3.0 + src_offset/src_offset_range*join_range;
		src_y2 = join_y;
	
		// Path 2, (join_x, join_y) -> (dst_x, dst_y)
		dst_x1 = MIN(dst_x, src_mod_x) - x_dist/5.0 - dst_offset/dst_offset_range*join_range;
		dst_y1 = MIN(dst_y, src_mod_y + src_mod_h) - (x_dist/3.0 + dst_offset) * y_mod;
		dst_x2 = MIN(dst_x, src_mod_x) - x_dist/3.0 - dst_offset/dst_offset_range*join_range;
		dst_y2 = join_y;


	// Curve through connections
	} else if (dst_x < src_x) {
		
		join_range = MIN(join_range, smallest_offset_range);

		// Calculate join point
		double ratio = (x_dist - y_dist) / (y_dist + x_dist);
		join_x = (src_x + dst_x)/2.0;
		join_y = (src_y + dst_y)/2.0;
	
		// Vertical centre point between the modules
		join_y = (src_y < dst_y)
			? (dst_mod_y - (dst_mod_y - (src_mod_y + src_mod_h)) / 2.0)
			: (src_mod_y - (src_mod_y - (dst_mod_y + dst_mod_h)) / 2.0);
		
		join_y -= (smallest_offset / smallest_offset_range * join_range) - join_range/2.0;
		
		// Interpolate between (src_x < dst_x) case and (src_y == dst_y) case
		if (src_x < dst_x && x_dist > y_dist) {
				join_y *= (1.0-ratio);
				join_y += (src_y + dst_y)/2.0 * ratio;
		}
			
		if (src_x < dst_x) {
			join_y += ((smallest_offset/smallest_offset_range)) * join_range * (1.0-fabs(ratio));
			join_x -= ((smallest_offset/smallest_offset_range)) * join_range * (1.0-fabs(ratio)) * y_mod;
		}
		
		//cerr << "ratio: " << ratio << endl;
			
		double src_x_offset = fabs(src_x - join_x)/8.0 + MAX(src_offset_range,join_range) - src_offset/src_offset_range*join_range;
		double dst_x_offset = fabs(dst_x - join_x)/8.0 + MAX(dst_offset_range,join_range) + dst_offset/dst_offset_range*join_range;
		double src_y_offset = fabs(src_y - join_y)/4.0 + src_offset/src_offset_range*(src_offset_range+join_range)/2.0;
		double dst_y_offset = fabs(dst_y - join_y)/4.0 + (dst_offset_range-dst_offset)/dst_offset_range*(dst_offset_range+join_range)/2.0;

		// Path 1 (src_x, src_y) -> (join_x, join_y)
		src_x1 = src_x + src_x_offset;
		src_y1 = join_y - src_y_offset * y_mod;
		src_x2 = src_x + src_x_offset;
		src_y2 = join_y;
	
		// Path 2, (join_x, join_y) -> (dst_x, dst_y)
		dst_x1 = dst_x - dst_x_offset;
		dst_y1 = join_y + dst_y_offset * y_mod;
		dst_x2 = dst_x - dst_x_offset;
		dst_y2 = join_y;


	// In between curve through and normal connections
/*	} else if (x_dist < y_dist && src_mod_y + src_mod_h < dst_mod_y) {
		// Calculate join point
		double ratio = (x_dist - y_dist) / (y_dist + x_dist);
		join_x = (src_x + dst_x)/2.0;
		join_y = (src_y + dst_y)/2.0;
	
		// Vertical centre point between the modules
		join_y = (src_y < dst_y)
			? (dst_mod_y - (dst_mod_y - (src_mod_y + src_mod_h)) / 2.0)
			: (src_mod_y - (src_mod_y - (dst_mod_y + dst_mod_h)) / 2.0);
		
		join_y -= (smallest_offset / smallest_offset_range * join_range) - join_range/2.0;
		
		// Interpolate between (src_x < dst_x) case and (src_y == dst_y) case
		if (src_x < dst_x && x_dist > y_dist) {
				join_y *= (1.0-ratio);
				join_y += (src_y + dst_y)/2.0 * ratio;
		}
			
		if (src_x < dst_x) {
			join_y += ((smallest_offset/smallest_offset_range)) * join_range * (1.0-fabs(ratio));
			join_x -= ((smallest_offset/smallest_offset_range)) * join_range * (1.0-fabs(ratio)) * y_mod;
		}
		double src_x_offset = fabs(src_x - join_x)/8.0 + src_offset_range - src_offset/src_offset_range*join_range;
		double dst_x_offset = fabs(dst_x - join_x)/8.0 + dst_offset_range + dst_offset/dst_offset_range*join_range;
		double src_y_offset = fabs(src_y - join_y)/4.0 + src_offset/src_offset_range*(src_offset_range+join_range)/2.0;
		double dst_y_offset = fabs(dst_y - join_y)/2.0 + (dst_offset_range-dst_offset)/dst_offset_range*(dst_offset_range+join_range)/2.0;
		
		// Path 1 (src_x, src_y) -> (join_x, join_y)
		src_x1 = src_x + src_x_offset;
		// Interpolate from curve through case
		if (x_dist < y_dist) {
			// Smooth transition from (src_y) to (join_y - src_y_offset * y_mod)
			src_y1 = (src_y * (1.0-fabs(ratio))) + ((join_y - src_y_offset * y_mod) * -ratio);
			// Smooth transition from (join_x + src_offset_range/4.0 * -ratio) to (src_x + src_x_offset)
			src_x2 = (join_x + src_offset_range/4.0 * -ratio)*(1.0-fabs(ratio)) + (src_x + src_x_offset)*-ratio;
		} else {
			src_y1 = src_y;
			src_x2 = join_x + src_offset_range/4.0 * -ratio;
		}
		src_y2 = join_y - src_y_offset*(1.0-fabs(ratio)) * y_mod;
		
		// Path 2, (join_x, join_y) -> (dst_x, dst_y)
		dst_x1 = dst_x - dst_x_offset;
		dst_y1 = dst_y;
		// Interpolate from curve through case
		if (x_dist < y_dist) {
			// Smooth transition from (dst_y) to (join_y - dst_y_offset * y_mod)
			dst_y1 = (dst_y * (1.0-fabs(ratio))) + ((join_y + dst_y_offset * y_mod) * -ratio);
			// Smooth transition from (join_x + dst_offset_range/4.0 * -ratio) to (dst_x + dst_x_offset)
			dst_x2 = (join_x - dst_offset_range/4.0 * -ratio)*(1.0-fabs(ratio)) + (dst_x - dst_x_offset)*-ratio;
		} else {
			dst_y1 = dst_y;
			dst_x2 = join_x - dst_offset_range/4.0 * -ratio;
		}
		dst_y2 = join_y + dst_y_offset*(1.0-fabs(ratio)) * y_mod;
*/
	
	// "Normal" connections
	} else {

		join_x = (src_x + dst_x)/2.0;
		join_y = (src_y + dst_y)/2.0;
#if 0
		join_range = MIN(join_range, x_dist/2.0);

		
		
		/************ Find join point **************/

		//const double join_range = 15.0;
		//const double join_range = MIN(smallest_offset_range, x_dist/2.0);
		//const double join_range = MIN(30.0, x_dist/2.0);

		// Calculate join point
		double ratio = (x_dist - y_dist) / (y_dist + x_dist);
	
		cerr << "ratio: " << ratio << endl;
		
	/*	if (MAX(x_dist, y_dist) > smallest_offset_range * 2.0) {
			// Vertical centre point between the modules
			join_y = (src_y < dst_y)
				? (dst_mod_y - (dst_mod_y - (src_mod_y + src_mod_h)) / 2.0)
			 	: (src_mod_y - (src_mod_y - (dst_mod_y + dst_mod_h)) / 2.0);
			
			join_y -= (smallest_offset / smallest_offset_range * join_range) - join_range/2.0;
			
			// Interpolate between (src_x < dst_x) case and (src_y == dst_y) case
			if (src_x < dst_x && x_dist > y_dist) {
				join_y *= (1.0-ratio);
				join_y += (src_y + dst_y)/2.0 * ratio;
			}
		}*/
			
		if (src_x < dst_x) {
		//	join_y += ((smallest_offset/smallest_offset_range)) * join_range * (1.0-fabs(ratio));
			join_x -= ((smallest_offset/smallest_offset_range)) * join_range * (1.0-fabs(ratio)) * y_mod;
			join_x += join_range/2.0 * (1.0-fabs(ratio)); // center
		}
#endif

		/*************************************************/

		
		// Path 1 (src_x, src_y) -> (join_x, join_y)
		// Control point 1
		src_x1 = src_x + fabs(join_x - src_x) / 2.0;
		src_y1 = src_y;
		// Control point 2
		src_x2 = join_x - fabs(join_x - src_x) / 4.0;
		src_y2 = join_y - fabs(join_y - src_y) / 2.0 * y_mod;
		
		// Path 2, (join_x, join_y) -> (dst_x, dst_y)
		// Control point 1
		dst_x1 = dst_x - fabs(join_x - dst_x) / 2.0;
		dst_y1 = dst_y;
		// Control point 2
		dst_x2 = join_x + fabs(join_x - dst_x) / 4.0;
		dst_y2 = join_y + fabs(join_y - dst_y) / 2.0 * y_mod;
	}
	
	// This was broken in libgnomecanvasmm with GTK 2.8.  Nice work, guys.  
	/*
	m_path->reset();
	m_path->moveto(src_x, src_y);
	m_path->curveto(src_x1, src_y1, src_x2, src_y2, join_x, join_y);
	m_path->curveto(dst_x2, dst_y2, dst_x1, dst_y1, dst_x, dst_y);
	set_bpath(m_path);
	*/
	
	// Work around it w/ the C API
	gnome_canvas_path_def_reset(m_path);
	gnome_canvas_path_def_moveto(m_path, src_x, src_y);
	gnome_canvas_path_def_curveto(m_path, src_x1, src_y1, src_x2, src_y2, join_x, join_y);
	gnome_canvas_path_def_curveto(m_path, dst_x2, dst_y2, dst_x1, dst_y1, dst_x, dst_y);
	
	GnomeCanvasBpath* c_obj = gobj();
	gnome_canvas_item_set(GNOME_CANVAS_ITEM(c_obj), "bpath", m_path, NULL);
}


/** Removes the reference to this connection contained in the ports.
 *
 * Must be called before destroying a connection.
 */
void
Connection::disconnect()
{
	m_source_port->remove_connection(this);
	m_dest_port->remove_connection(this);
	m_source_port = NULL;
	m_dest_port = NULL;
}


void
Connection::hilite(bool b)
{
	if (b)
		property_outline_color_rgba() = 0xFF0000FF;
	else
		property_outline_color_rgba() = m_colour;
}


void
Connection::selected(bool selected)
{
	m_selected = selected;
	if (selected)
		property_dash() = m_patch_bay->select_dash();
	else
		property_dash() = NULL;
}


} // namespace LibFlowCanvas

