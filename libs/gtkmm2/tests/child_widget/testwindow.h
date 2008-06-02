//$Id: testwindow.h 2 2003-01-21 13:41:59Z murrayc $ -*- c++ -*-

/* gtkmm example Copyright (C) 2002 gtkmm development team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef GTKMM_TESTWINDOW_H
#define GTKMM_TESTWINDOW_H

#include <gtkmm/window.h>
#include <gtkmm/button.h>

class TestWindow : public Gtk::Window
{
public:
  TestWindow();
  virtual ~TestWindow();

protected:

  //Child widgets:

  //For the test, we allocate it dynamically, so that we can delete it whenever we want, and see the results
  Gtk::Button m_Button;
};

#endif //GTKMM_TESTWINDOW_H
