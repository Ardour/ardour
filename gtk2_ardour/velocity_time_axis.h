/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_gtk_velocity_time_axis_h__
#define __ardour_gtk_velocity_time_axis_h__

#include "automation_time_axis.h"

class VelocityTimeAxisView : public AutomationTimeAxisView
{
public:
	VelocityTimeAxisView (ARDOUR::Session*,
	                        boost::shared_ptr<ARDOUR::Stripable>,
	                        boost::shared_ptr<ARDOUR::Automatable>,
	                        boost::shared_ptr<ARDOUR::AutomationControl>,
	                        PublicEditor&,
	                        TimeAxisView& parent,
	                        bool show_regions,
	                        ArdourCanvas::Canvas& canvas,
	                        const std::string & name = "", /* translatable */
	                        const std::string & plug_name = "");

	~VelocityTimeAxisView();

};

#endif /* __ardour_gtk_velocity_time_axis_h__ */
