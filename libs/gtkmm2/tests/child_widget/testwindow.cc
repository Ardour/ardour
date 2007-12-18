//$Id: testwindow.cc 2 2003-01-21 13:41:59Z murrayc $ -*- c++ -*-

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

#include "testwindow.h"

TestWindow::TestWindow()
: m_Button("test")
{
  add(m_Button);

  show_all_children();

  //This seems to be 2, and then 3 during destruction. Looks like unref-ing isn't enough - we'll have to gtk_object_destroy() it instead of just unrefing.
  g_warning("m_Button -> refcount = %d\n", G_OBJECT(m_Button.gobj())->ref_count);
}

TestWindow::~TestWindow()
{
}

