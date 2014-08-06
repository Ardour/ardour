/*
    Copyright (C) 2014 Waves Audio Ltd.

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
*/

#ifndef __gtk2_waves_grid_h__
#define __gtk2_waves_grid_h__

#include <gtkmm/fixed.h>

class WavesGrid : public Gtk::Fixed
{
  public:
	WavesGrid ();
	virtual ~WavesGrid ();
	void pack (Gtk::Widget& widget);

  protected:
	void on_size_allocate (Gtk::Allocation&);
};

#endif /* __gtk2_waves_grid_h__ */
