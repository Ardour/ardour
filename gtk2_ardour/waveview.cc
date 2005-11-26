#include "waveview.h"
#include "waveview_p.h"

/* $Id$ */

/* waveview.cc
 *
 * Copyright (C) 1998 EMC Capital Management Inc.
 * Developed by Havoc Pennington <hp@pobox.com>
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

//#include <libgnomecanvasmm/group.h>

namespace Gnome
{

namespace Canvas
{

WaveView::WaveView(Group& parentx)
	: Item(GNOME_CANVAS_ITEM(g_object_new(get_type(),0)))
{
	item_construct(parentx);
}

} /* namespace Canvas */
} /* namespace Gnome */


namespace Glib
{

Gnome::Canvas::WaveView* wrap(GnomeCanvasWaveView* object, bool take_copy)
{
  return dynamic_cast<Gnome::Canvas::WaveView *> (Glib::wrap_auto ((GObject*)(object), take_copy));
}

} /* namespace Glib */

namespace Gnome
{

namespace Canvas
{


/* The *_Class implementation: */

const Glib::Class& WaveView_Class::init()
{
  if(!gtype_) // create the GType if necessary
  {
    // Glib::Class has to know the class init function to clone custom types.
    class_init_func_ = &WaveView_Class::class_init_function;

    // This is actually just optimized away, apparently with no harm.
    // Make sure that the parent type has been created.
    //CppClassParent::CppObjectType::get_type();

    // Create the wrapper type, with the same class/instance size as the base type.
    register_derived_type(gnome_canvas_waveview_get_type());

    // Add derived versions of interfaces, if the C type implements any interfaces:
  }

  return *this;
}

void WaveView_Class::class_init_function(void* g_class, void* class_data)
{
  BaseClassType *const klass = static_cast<BaseClassType*>(g_class);
  CppClassParent::class_init_function(klass, class_data);

}


Glib::ObjectBase* WaveView_Class::wrap_new(GObject* o)
{
  return manage(new WaveView((GnomeCanvasWaveView*)(o)));

}


/* The implementation: */

WaveView::WaveView(const Glib::ConstructParams& construct_params)
:
  Item(construct_params)
{
  }

WaveView::WaveView(GnomeCanvasWaveView* castitem)
:
  Item((GnomeCanvasItem*)(castitem))
{
  }

WaveView::~WaveView()
{
  destroy_();
}

WaveView::CppClassType WaveView::waveview_class_; // initialize static member

GType WaveView::get_type()
{
  return waveview_class_.init().get_type();
}

GType WaveView::get_base_type()
{
  return gnome_canvas_waveview_get_type();
}

GnomeCanvasWaveViewCache*
WaveView::create_cache ()
{
	return gnome_canvas_waveview_cache_new ();
}

Glib::PropertyProxy<void*> WaveView::property_data_src()
{
	return Glib::PropertyProxy<void*> (this, "data_src");
}
Glib::PropertyProxy_ReadOnly<void*> WaveView::property_data_src() const
{
	return Glib::PropertyProxy_ReadOnly<void*> (this, "data_src");
}
Glib::PropertyProxy<uint32_t> WaveView::property_channel() 
{
	return Glib::PropertyProxy<uint32_t> (this, "channel");
}
Glib::PropertyProxy_ReadOnly<uint32_t> WaveView::property_channel()  const
{
	return Glib::PropertyProxy_ReadOnly<uint32_t> (this, "channel");
}
Glib::PropertyProxy<void*> WaveView::property_length_function()
{
	return Glib::PropertyProxy<void*> (this, "length_function");
}
Glib::PropertyProxy_ReadOnly<void*> WaveView::property_length_function() const
{
	return Glib::PropertyProxy_ReadOnly<void*> (this, "length_function");
}
Glib::PropertyProxy<void*> WaveView::property_sourcefile_length_function()
{
	return Glib::PropertyProxy<void*> (this, "sourcefile_length_function");
}
Glib::PropertyProxy_ReadOnly<void*> WaveView::property_sourcefile_length_function() const
{
	return Glib::PropertyProxy_ReadOnly<void*> (this, "sourcefile_length_function");
}
Glib::PropertyProxy<void*> WaveView::property_peak_function()
{
	return Glib::PropertyProxy<void*> (this, "peak_function");
}
Glib::PropertyProxy_ReadOnly<void*> WaveView::property_peak_function() const
{
	return Glib::PropertyProxy_ReadOnly<void*> (this, "peak_function");
}
Glib::PropertyProxy<void*> WaveView::property_gain_function()
{
	return Glib::PropertyProxy<void*> (this, "gain_function");
}
Glib::PropertyProxy_ReadOnly<void*> WaveView::property_gain_function() const
{
	return Glib::PropertyProxy_ReadOnly<void*> (this, "gain_function");
}
Glib::PropertyProxy<void*> WaveView::property_gain_src()
{
	return Glib::PropertyProxy<void*> (this, "gain_src");
}
Glib::PropertyProxy_ReadOnly<void*> WaveView::property_gain_src() const
{
	return Glib::PropertyProxy_ReadOnly<void*> (this, "gain_src");
}
Glib::PropertyProxy<void*> WaveView::property_cache()
{
	return Glib::PropertyProxy<void*> (this, "cache");
}
Glib::PropertyProxy_ReadOnly<void*> WaveView::property_cache() const
{
	return Glib::PropertyProxy_ReadOnly<void*> (this, "cache");
}
Glib::PropertyProxy<bool> WaveView::property_cache_updater()
{
	return Glib::PropertyProxy<bool> (this, "cache_updater");
}
Glib::PropertyProxy_ReadOnly<bool> WaveView::property_cache_updater() const
{
	return Glib::PropertyProxy_ReadOnly<bool> (this, "cache_updater");
}
Glib::PropertyProxy<double> WaveView::property_samples_per_unit()
{
	return Glib::PropertyProxy<double> (this, "samples_per_unit");
}
Glib::PropertyProxy_ReadOnly<double> WaveView::property_samples_per_unit() const
{
	return Glib::PropertyProxy_ReadOnly<double> (this, "samples_per_unit");
}
Glib::PropertyProxy<double> WaveView::property_amplitude_above_axis()
{
	return Glib::PropertyProxy<double> (this, "amplitude_above_axis");
}
Glib::PropertyProxy_ReadOnly<double> WaveView::property_amplitude_above_axis() const
{
	return Glib::PropertyProxy_ReadOnly<double> (this, "amplitude_above_axis");
}
Glib::PropertyProxy<double> WaveView::property_x()
{
	return Glib::PropertyProxy<double> (this, "x");
}
Glib::PropertyProxy_ReadOnly<double> WaveView::property_x() const
{
	return Glib::PropertyProxy_ReadOnly<double> (this, "x");
}
Glib::PropertyProxy<double> WaveView::property_y()
{
	return Glib::PropertyProxy<double> (this, "y");
}
Glib::PropertyProxy_ReadOnly<double> WaveView::property_y() const
{
	return Glib::PropertyProxy_ReadOnly<double> (this, "y");
}
Glib::PropertyProxy<double> WaveView::property_height()
{
	return Glib::PropertyProxy<double> (this, "height");
}
Glib::PropertyProxy_ReadOnly<double> WaveView::property_height() const
{
	return Glib::PropertyProxy_ReadOnly<double> (this, "height");
}
Glib::PropertyProxy<guint> WaveView::property_wave_color()
{
	return Glib::PropertyProxy<guint> (this, "wave_color");
}
Glib::PropertyProxy_ReadOnly<guint> WaveView::property_wave_color() const
{
	return Glib::PropertyProxy_ReadOnly<guint> (this, "wave_color");
}
Glib::PropertyProxy<gint> WaveView::property_rectified()
{
	return Glib::PropertyProxy<gint> (this, "rectified");
}
Glib::PropertyProxy_ReadOnly<gint> WaveView::property_rectified() const
{
	return Glib::PropertyProxy_ReadOnly<gint> (this, "rectified");
}
Glib::PropertyProxy<guint> WaveView::property_region_start()
{
	return Glib::PropertyProxy<guint> (this, "region_start");
}
Glib::PropertyProxy_ReadOnly<guint> WaveView::property_region_start() const
{
	return Glib::PropertyProxy_ReadOnly<guint> (this, "region_start");
}

} // namespace Canvas

} // namespace Gnome


