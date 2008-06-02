#ifndef _LIBGNOMECANVASMM_PROPERTIES_H_
#define _LIBGNOMECANVASMM_PROPERTIES_H_

// -*- c++ -*-
/* $Id$ */

/* properties.h
 *
 * Copyright (C) 1999-2002 The Free Software Foundation
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

//#include <gtk/gtkpacker.h> //For GtkAnchorType.


#include <glibmm/propertyproxy.h>
#include <gdkmm/color.h>
#include <gdkmm/bitmap.h>
#include <pangomm/fontdescription.h>
#include <gtkmm/enums.h>

namespace Gnome
{

namespace Canvas
{

namespace Properties
{

class PropertyBase
{
public:
  PropertyBase(const char* name);
  ~PropertyBase();

  const char* get_name() const;

protected:
  const char* name_;
};

template <class T_Value>
class Property : public PropertyBase
{
public:
  Property(const char* name, const T_Value& value)
  : PropertyBase(name), value_(value)
  {}

  void set_value_in_object(Glib::Object& object) const
  {
    Glib::PropertyProxy<T_Value> proxy(&object, get_name());
    proxy.set_value(value_);
  }

protected:
  T_Value value_;
};


/** Allow use of << operator on objects:
  * For instance:
  * canvasgroup << Gnome::Canvas::CanvasHelpers::x(2);
  */
template <class O, class T>
O& operator << (O& object, const Property<T>& property)
{
  property.set_value_in_object(object);
  return object;
}

/********* specializations *********/

//Colors can be specified with a string or a Gdk::Color, or an rgba guint.
template<>
class Property<Gdk::Color> : public PropertyBase
{
public:
  Property(const char* name, const Gdk::Color& value);
  Property(const char* name, const Glib::ustring& color);  
  Property(const char* name, const guint& rgba_color);

  void set_value_in_object(Glib::Object& object) const;

protected:
  Gdk::Color value_;
  bool value_gobj_used_; //Whether the Gdk::Value was intialised in the constructor.
  Glib::ustring value_string_;
  bool value_string_used_;
  guint value_rgba_;
};

//Font can be specified with a string or a Pango::FontDescription.
template<>
class Property<Pango::FontDescription> : public PropertyBase
{
public:
  Property(const char* name, const Pango::FontDescription& value);
  Property(const char* name, const Glib::ustring& font);

  void set_value_in_object(Glib::Object& object) const;

protected:
  Pango::FontDescription value_;
  Glib::ustring value_string_;
};


//We now define some specific properties.
//Some of these are unusual, so we define them manually.
//Others are regular so we define them with a macro:


class font : public Property<Pango::FontDescription>  //Used by CanvasText.
{
public:
  font(const Pango::FontDescription& v);

  font(const Glib::ustring& v);
};

template<>
class Property< Glib::RefPtr<Gdk::Bitmap> > : public PropertyBase
{
public:
  Property(const char* name, const Glib::RefPtr<Gdk::Bitmap>& value);

  void set_value_in_object(Glib::Object& object) const;

protected:
  Glib::RefPtr<Gdk::Bitmap> value_;
};

class fill_color : public Property<Gdk::Color>
{
public:
  fill_color(const Gdk::Color& v);

  fill_color(const Glib::ustring& v);
};

class outline_color : public Property<Gdk::Color>
{
public:
  outline_color(const Gdk::Color& v);

  outline_color(const Glib::ustring& v);
};


// GNOMEMM_PROPERTY(C++ name, C property name, C++ type)
#define GNOMEMM_PROPERTY(N,N2,T) \
class N : public Property<T > \
{ \
public: \
  N(const T& v); \
};


// CanvasLine
GNOMEMM_PROPERTY(arrow_shape_a,arrow_shape_a,double)
GNOMEMM_PROPERTY(arrow_shape_b,arrow_shape_b,double)
GNOMEMM_PROPERTY(arrow_shape_c,arrow_shape_c,double)
GNOMEMM_PROPERTY(cap_style,cap_style,Gdk::CapStyle)
GNOMEMM_PROPERTY(first_arrowhead,first_arrowhead,bool)
GNOMEMM_PROPERTY(join_style,join_style,Gdk::JoinStyle)
GNOMEMM_PROPERTY(last_arrowhead,last_arrowhead,bool)
GNOMEMM_PROPERTY(line_style,line_style,Gdk::LineStyle)
GNOMEMM_PROPERTY(smooth,smooth,bool)
GNOMEMM_PROPERTY(spline_steps,spline_steps,guint)

// CanvasText
GNOMEMM_PROPERTY(clip,clip,bool)
GNOMEMM_PROPERTY(clip_height,clip_height,double)
GNOMEMM_PROPERTY(clip_width,clip_width,double)
GNOMEMM_PROPERTY(justification,justification,Gtk::Justification)
GNOMEMM_PROPERTY(direction,direction,Gtk::DirectionType)
GNOMEMM_PROPERTY(wrap_mode,wrap_mode,Gtk::WrapMode)
GNOMEMM_PROPERTY(text_height,text_height,double)
GNOMEMM_PROPERTY(text_width,text_width,double)
GNOMEMM_PROPERTY(x_offset,x_offset,double)
GNOMEMM_PROPERTY(y_offset,y_offset,double)
GNOMEMM_PROPERTY(text,text,Glib::ustring)
GNOMEMM_PROPERTY(markup,markup,Glib::ustring)
GNOMEMM_PROPERTY(editable,editable,bool)
GNOMEMM_PROPERTY(visible,visible,bool)
GNOMEMM_PROPERTY(cursor_visible,cursor_visible,bool)
GNOMEMM_PROPERTY(cursor_blink,cursor_blink,bool)
GNOMEMM_PROPERTY(grow_height,grow_height,bool)
GNOMEMM_PROPERTY(pixels_above_lines,pixels_above_lines,int)
GNOMEMM_PROPERTY(pixels_below_lines,pixels_below_lines,int)
GNOMEMM_PROPERTY(pixels_inside_wrap,pixels_inside_wrap,int)
GNOMEMM_PROPERTY(left_margin,left_margin,int)
GNOMEMM_PROPERTY(right_margin,right_margin,int)
GNOMEMM_PROPERTY(indent,indent,int)

// CanvasWidget
GNOMEMM_PROPERTY(size_pixels,size_pixels,bool)

// CanvasImage, CanvasWidget
GNOMEMM_PROPERTY(height,height,double)
GNOMEMM_PROPERTY(width,width,double)

// CanvasRect, CanvasEllipse
GNOMEMM_PROPERTY(x1,x1,double)
GNOMEMM_PROPERTY(x2,x2,double)
GNOMEMM_PROPERTY(y1,y1,double)
GNOMEMM_PROPERTY(y2,y2,double)

// CanvasImage, CanvasText, CanvasWidget
GNOMEMM_PROPERTY(anchor,anchor,Gtk::AnchorType)

// CanvasPolygon, CanvasRect, CanvasEllipse
GNOMEMM_PROPERTY(outline_stipple,outline_stipple,Glib::RefPtr<Gdk::Bitmap>)
GNOMEMM_PROPERTY(wind,wind,guint)
GNOMEMM_PROPERTY(miterlimit,miterlimit,double)

// CanvasLine, CanvasPolygon, CanvasRect, CanvasEllipse
GNOMEMM_PROPERTY(width_pixels,width_pixels,guint)
GNOMEMM_PROPERTY(width_units,width_units,double)

// CanvasGroup, CanvasImage, CanvasText, CanvasWidget
GNOMEMM_PROPERTY(x,x,double)
GNOMEMM_PROPERTY(y,y,double)

// CanvasLine, CanvasPolygon, CanvasRect, CanvasEllipse, CanvasText
GNOMEMM_PROPERTY(fill_stipple,fill_stipple,Glib::RefPtr<Gdk::Bitmap>)

} /* namespace Properties */
} /* namespace Canvas */
} /* namespace Gnome */

#endif /* _LIBGNOMECANVASMM_PROPERTIES_H_ */

