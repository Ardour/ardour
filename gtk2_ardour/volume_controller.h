/*
    Copyright (C) 1998-2007 Paul Davis
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id: volume_controller.h,v 1.4 2000/05/03 15:54:21 pbd Exp $
*/

#ifndef __gtk_ardour_vol_controller_h__
#define __gtk_ardour_vol_controller_h__

#include <gtkmm/adjustment.h>

#include "gtkmm2ext/motionfeedback.h"

class VolumeController : public Gtkmm2ext::MotionFeedback
{
  public:
	VolumeController (Glib::RefPtr<Gdk::Pixbuf>,
			  boost::shared_ptr<PBD::Controllable>,
			  double def,
			  double step,
			  double page,
			  bool with_numeric = true,
                          int image_width = 40,
                          int image_height = 40,
			  bool linear = true);

        virtual ~VolumeController () {}

	static void _dB_printer (char buf[32], const boost::shared_ptr<PBD::Controllable>& adj, void* arg);

  protected:
	double to_control_value (double);
	double to_display_value (double);
	double adjust (double nominal_delta);

	double display_value () const;
	double control_value () const;

  private:
	bool _linear;

	void dB_printer (char buf[32], const boost::shared_ptr<PBD::Controllable>& adj);
};

#endif // __gtk_ardour_vol_controller_h__


