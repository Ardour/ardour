// -*- c++ -*-
#ifndef _LIBGNOMECANVASMM_WAVEVIEW_H
#define _LIBGNOMECANVASMM_WAVEVIEW_H

#include <glibmm.h>


/* waveview.h
 *
 * Copyright (C) 1998 EMC Capital Management Inc.
 * Developed by Havoc Pennington <hp@pobox.com>
 *
 * Copyright (C) 1999 The Gtk-- Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <libgnomecanvasmm/item.h>
#include "canvas-waveview.h"
#include <libgnomecanvas/gnome-canvas-util.h>
#include <libgnomecanvas/libgnomecanvas.h>
#include <vector>


#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef struct _GnomeCanvasWaveView GnomeCanvasWaveView;
typedef struct _GnomeCanvasWaveViewClass GnomeCanvasWaveViewClass;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


namespace Gnome
{

namespace Canvas
{ class WaveView_Class; } // namespace Canvas

} // namespace Gnome
namespace Gnome
{

namespace Canvas
{

class GnomeGroup;

class WaveView : public Item
{
  public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef WaveView CppObjectType;
  typedef WaveView_Class CppClassType;
  typedef GnomeCanvasWaveView BaseObjectType;
  typedef GnomeCanvasWaveViewClass BaseClassType;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  virtual ~WaveView();

#ifndef DOXYGEN_SHOULD_SKIP_THIS

private:
  friend class WaveView_Class;
  static CppClassType waveview_class_;

  // noncopyable
  WaveView(const WaveView&);
  WaveView& operator=(const WaveView&);

protected:
  explicit WaveView(const Glib::ConstructParams& construct_params);
  explicit WaveView(GnomeCanvasWaveView* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  static GType get_type()      G_GNUC_CONST;
  static GType get_base_type() G_GNUC_CONST;
#endif

  ///Provides access to the underlying C GtkObject.
  GnomeCanvasWaveView*       gobj()       { return reinterpret_cast<GnomeCanvasWaveView*>(gobject_); }

  ///Provides access to the underlying C GtkObject.
  const GnomeCanvasWaveView* gobj() const { return reinterpret_cast<GnomeCanvasWaveView*>(gobject_); }


public:
  //C++ methods used to invoke GTK+ virtual functions:

protected:
  //GTK+ Virtual Functions (override these to change behaviour):

  //Default Signal Handlers::


private:

public:
  WaveView(Group& parent);

  static GnomeCanvasWaveViewCache* create_cache();

  Glib::PropertyProxy<void*> property_data_src();
  Glib::PropertyProxy_ReadOnly<void*> property_data_src() const;
  Glib::PropertyProxy<uint32_t> property_channel();
  Glib::PropertyProxy_ReadOnly<uint32_t> property_channel() const;
  Glib::PropertyProxy<void*> property_length_function();
  Glib::PropertyProxy_ReadOnly<void*> property_length_function() const;
  Glib::PropertyProxy<void*> property_sourcefile_length_function();
  Glib::PropertyProxy_ReadOnly<void*> property_sourcefile_length_function() const;
  Glib::PropertyProxy<void*> property_peak_function();
  Glib::PropertyProxy_ReadOnly<void*> property_peak_function() const;
  Glib::PropertyProxy<void*> property_gain_function();
  Glib::PropertyProxy_ReadOnly<void*> property_gain_function() const;
  Glib::PropertyProxy<void*> property_gain_src();
  Glib::PropertyProxy_ReadOnly<void*> property_gain_src() const;
  Glib::PropertyProxy<void*> property_cache();
  Glib::PropertyProxy_ReadOnly<void*> property_cache() const;
  Glib::PropertyProxy<bool> property_cache_updater();
  Glib::PropertyProxy_ReadOnly<bool> property_cache_updater() const;
  Glib::PropertyProxy<double> property_samples_per_unit();
  Glib::PropertyProxy_ReadOnly<double> property_samples_per_unit() const;
  Glib::PropertyProxy<double> property_amplitude_above_axis();
  Glib::PropertyProxy_ReadOnly<double> property_amplitude_above_axis() const;
  Glib::PropertyProxy<double> property_x();
  Glib::PropertyProxy_ReadOnly<double> property_x() const;
  Glib::PropertyProxy<double> property_y();
  Glib::PropertyProxy_ReadOnly<double> property_y() const;
  Glib::PropertyProxy<double> property_height();
  Glib::PropertyProxy_ReadOnly<double> property_height() const;
  Glib::PropertyProxy<guint> property_wave_color();
  Glib::PropertyProxy_ReadOnly<guint> property_wave_color() const;
  Glib::PropertyProxy<guint> property_clip_color();
  Glib::PropertyProxy_ReadOnly<guint> property_clip_color() const;
  Glib::PropertyProxy<guint> property_fill_color();
  Glib::PropertyProxy_ReadOnly<guint> property_fill_color() const;
  Glib::PropertyProxy<gint> property_filled();
  Glib::PropertyProxy_ReadOnly<gint> property_filled() const;
  Glib::PropertyProxy<gint> property_zero_line();
  Glib::PropertyProxy_ReadOnly<gint> property_zero_line() const;
  Glib::PropertyProxy<guint> property_zero_color();
  Glib::PropertyProxy_ReadOnly<guint> property_zero_color() const;
  Glib::PropertyProxy<gint> property_rectified();
  Glib::PropertyProxy_ReadOnly<gint> property_rectified() const;
  Glib::PropertyProxy<guint> property_region_start();
  Glib::PropertyProxy_ReadOnly<guint> property_region_start() const;
  Glib::PropertyProxy<gint> property_logscaled();
  Glib::PropertyProxy_ReadOnly<gint> property_logscaled() const;
};

} /* namespace Canvas */
} /* namespace Gnome */


namespace Glib
{
  /** @relates Gnome::Canvas::WaveView
   * @param object The C instance
   * @param take_copy False if the result should take ownership of the C instance. True if it should take a new copy or ref.
   * @result A C++ instance that wraps this C instance.
   */
  Gnome::Canvas::WaveView* wrap(GnomeCanvasWaveView* object, bool take_copy = false);
}
#endif /* _LIBGNOMECANVASMM_WAVEVIEW_H */

