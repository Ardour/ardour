// -*- c++ -*-
/* $Id$ */

/* properties.cc
 *
 * Copyright (C) 2002 The Free Software Foundation
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

#include <libgnomecanvasmm/properties.h>

namespace Gnome
{

namespace Canvas
{

namespace Properties
{

PropertyBase::PropertyBase(const char* name)
: name_(name)
{}

PropertyBase::~PropertyBase()
{}

const char* PropertyBase::get_name() const
{
  return name_;
}

/////////////////////////////
// Property<Gdk::Color>

Property<Gdk::Color>::Property(const char* name, const Gdk::Color& value) :
  PropertyBase(name),
  value_(value),
  value_gobj_used_(true),
  value_string_used_(false),
  value_rgba_(0)
{}

Property<Gdk::Color>::Property(const char* name, const Glib::ustring& color) :
  PropertyBase(name),
  value_gobj_used_(false),
  value_string_(color),
  value_string_used_ (true),
  value_rgba_(0)
{}

Property<Gdk::Color>::Property(const char* name, const guint& rgba_color) :
  PropertyBase(name),
  value_gobj_used_(false),
  value_string_used_(false),
  value_rgba_(rgba_color)
{}

void Property<Gdk::Color>::set_value_in_object(Glib::Object& object) const
{
  //Set the appropriate property name with the appropriately-typed value:
  if(value_string_used_) {
    
    Glib::PropertyProxy<Glib::ustring> proxy(&object, get_name());
    if (value_string_ == "")
      proxy.reset_value ();
    else
      proxy.set_value(value_string_);

  } else if(value_gobj_used_) {
    
    Glib::PropertyProxy<Gdk::Color> proxy(&object, get_name());
    proxy.set_value(value_);

  } else {
    
    Glib::PropertyProxy<guint> proxy(&object, get_name());
    proxy.set_value(value_rgba_);
  }
}

/////////////////////////////
// Property<Pango::FontDescription>
Property<Pango::FontDescription>::Property(const char* name, const Pango::FontDescription& value) :
  PropertyBase(name),
  value_(value)
{}

Property<Pango::FontDescription>::Property(const char* name, const Glib::ustring& font) :
  PropertyBase(name),
  value_(0),
  value_string_(font)
{}

void Property<Pango::FontDescription>::set_value_in_object(Glib::Object& object) const
{
  if(value_string_.size())
  {
    Glib::PropertyProxy<Glib::ustring> proxy(&object, get_name());
    proxy.set_value(value_string_);
  }
  else
  {
    Glib::PropertyProxy<Pango::FontDescription> proxy(&object, get_name());
    proxy.set_value(value_);
  }
}


/////////////////////////////
// Property< Glib::RefPtr<Gdk::Bitmap> >
Property< Glib::RefPtr<Gdk::Bitmap> >::Property(const char* name, const Glib::RefPtr<Gdk::Bitmap>& value)
  : PropertyBase(name), 
    value_(value)
{}

void Property< Glib::RefPtr<Gdk::Bitmap> >::set_value_in_object(Glib::Object& object) const
{
  Glib::PropertyProxy< Glib::RefPtr<Gdk::Bitmap> > proxy(&object, get_name());
  proxy.set_value(value_);
}


font::font(const Pango::FontDescription& v)
  : Property<Pango::FontDescription>("font-desc", v)
{}

font::font(const Glib::ustring& v)
  : Property<Pango::FontDescription>("font", v)
{}

fill_color::fill_color(const Gdk::Color& v)
  : Property<Gdk::Color>("fill_color_gdk",v)
{}

fill_color::fill_color(const Glib::ustring& v)
  : Property<Gdk::Color>("fill_color",v)
{}

outline_color::outline_color(const Gdk::Color& v)
  : Property<Gdk::Color>("outline_color_gdk", v)
{}

outline_color::outline_color(const Glib::ustring& v)
  : Property<Gdk::Color>("outline_color", v)
{}

// GNOMEMM_PROPERTY_IMPL(C++ name, C property name, C++ type)
#define GNOMEMM_PROPERTY_IMPL(N,N2,T) \
N::N(const T& v) \
  : Property<T >(#N2, v) \
{}

// CanvasLine
GNOMEMM_PROPERTY_IMPL(arrow_shape_a,arrow_shape_a,double)
GNOMEMM_PROPERTY_IMPL(arrow_shape_b,arrow_shape_b,double)
GNOMEMM_PROPERTY_IMPL(arrow_shape_c,arrow_shape_c,double)
GNOMEMM_PROPERTY_IMPL(cap_style,cap_style,Gdk::CapStyle)
GNOMEMM_PROPERTY_IMPL(first_arrowhead,first_arrowhead,bool)
GNOMEMM_PROPERTY_IMPL(join_style,join_style,Gdk::JoinStyle)
GNOMEMM_PROPERTY_IMPL(last_arrowhead,last_arrowhead,bool)
GNOMEMM_PROPERTY_IMPL(line_style,line_style,Gdk::LineStyle)
GNOMEMM_PROPERTY_IMPL(smooth,smooth,bool)
GNOMEMM_PROPERTY_IMPL(spline_steps,spline_steps,guint)

// CanvasText
GNOMEMM_PROPERTY_IMPL(clip,clip,bool)
GNOMEMM_PROPERTY_IMPL(clip_height,clip_height,double)
GNOMEMM_PROPERTY_IMPL(clip_width,clip_width,double)
GNOMEMM_PROPERTY_IMPL(wrap_mode,wrap_mode,Gtk::WrapMode)
GNOMEMM_PROPERTY_IMPL(justification,justification,Gtk::Justification)
GNOMEMM_PROPERTY_IMPL(direction,direction,Gtk::DirectionType)
GNOMEMM_PROPERTY_IMPL(text_height,text_height,double)
GNOMEMM_PROPERTY_IMPL(text_width,text_width,double)
GNOMEMM_PROPERTY_IMPL(x_offset,x_offset,double)
GNOMEMM_PROPERTY_IMPL(y_offset,y_offset,double)
GNOMEMM_PROPERTY_IMPL(text,text,Glib::ustring)
GNOMEMM_PROPERTY_IMPL(markup,markup,Glib::ustring)
GNOMEMM_PROPERTY_IMPL(editable,editable,bool)
GNOMEMM_PROPERTY_IMPL(visible,visible,bool)
GNOMEMM_PROPERTY_IMPL(cursor_visible,cursor_visible,bool)
GNOMEMM_PROPERTY_IMPL(cursor_blink,cursor_blink,bool)
GNOMEMM_PROPERTY_IMPL(grow_height,grow_height,bool)
GNOMEMM_PROPERTY_IMPL(pixels_above_lines,pixels_above_lines,int)
GNOMEMM_PROPERTY_IMPL(pixels_below_lines,pixels_below_lines,int)
GNOMEMM_PROPERTY_IMPL(pixels_inside_wrap,pixels_inside_wrap,int)
GNOMEMM_PROPERTY_IMPL(left_margin,left_margin,int)
GNOMEMM_PROPERTY_IMPL(right_margin,right_margin,int)
GNOMEMM_PROPERTY_IMPL(indent,indent,int)

// CanvasWidget
GNOMEMM_PROPERTY_IMPL(size_pixels,size_pixels,bool)

// CanvasImage, CanvasWidget
GNOMEMM_PROPERTY_IMPL(height,height,double)
GNOMEMM_PROPERTY_IMPL(width,width,double)

// CanvasRect, CanvasEllipse
GNOMEMM_PROPERTY_IMPL(x1,x1,double)
GNOMEMM_PROPERTY_IMPL(x2,x2,double)
GNOMEMM_PROPERTY_IMPL(y1,y1,double)
GNOMEMM_PROPERTY_IMPL(y2,y2,double)

// CanvasImage, CanvasText, CanvasWidget
GNOMEMM_PROPERTY_IMPL(anchor,anchor,Gtk::AnchorType)

// CanvasPolygon, CanvasRect, CanvasEllipse
GNOMEMM_PROPERTY_IMPL(outline_stipple,outline_stipple,Glib::RefPtr<Gdk::Bitmap>)
GNOMEMM_PROPERTY_IMPL(wind,wind,guint)
GNOMEMM_PROPERTY_IMPL(miterlimit,miterlimit,double)

// CanvasLine, CanvasPolygon, CanvasRect, CanvasEllipse
GNOMEMM_PROPERTY_IMPL(width_pixels,width_pixels,guint)
GNOMEMM_PROPERTY_IMPL(width_units,width_units,double)

// CanvasGroup, CanvasImage, CanvasText, CanvasWidget
GNOMEMM_PROPERTY_IMPL(x,x,double)
GNOMEMM_PROPERTY_IMPL(y,y,double)

// CanvasLine, CanvasPolygon, CanvasRect, CanvasEllipse, CanvasText
GNOMEMM_PROPERTY_IMPL(fill_stipple,fill_stipple,Glib::RefPtr<Gdk::Bitmap>)

} /* namespace Properties */
} /* namespace Canvas */
} /* namespace Gnome */
