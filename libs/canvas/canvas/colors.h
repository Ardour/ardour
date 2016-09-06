/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#ifndef __ardour_canvas_colors_h__
#define __ardour_canvas_colors_h__

#include <cairomm/context.h>

#include "canvas/visibility.h"
#include "canvas/types.h"

namespace ArdourCanvas
{

struct LIBCANVAS_API HSV;
struct LIBCANVAS_API HSVA;

extern LIBCANVAS_API Color change_alpha (Color, double alpha);

extern LIBCANVAS_API Color hsva_to_color (double h, double s, double v, double a = 1.0);
extern LIBCANVAS_API void  color_to_hsva (Color color, double& h, double& s, double& v, double& a);
extern LIBCANVAS_API Color color_at_alpha (Color, double a);
extern LIBCANVAS_API void  color_to_hsv (Color color, double& h, double& s, double& v);
extern LIBCANVAS_API void  color_to_rgba (Color, double& r, double& g, double& b, double& a);
extern LIBCANVAS_API Color rgba_to_color (double r, double g, double b, double a);

uint32_t LIBCANVAS_API contrasting_text_color (uint32_t c);

struct LIBCANVAS_API HSV;

class LIBCANVAS_API SVAModifier
{
  public:
	enum Type {
		Add,
		Multiply,
		Assign
	};

	SVAModifier (std::string const &);
	SVAModifier (Type t, double ss, double vv, double aa) : type (t), _s (ss) , _v (vv) , _a (aa) {}
	SVAModifier () : type (Add), _s (0), _v (0), _a (0) {} /* no-op modifier */

	double s() const { return _s; }
	double v() const { return _v; }
	double a() const { return _a; }

	HSV operator () (HSV& hsv) const;
	std::string to_string () const;
	void from_string (std::string const &);

  private:
	Type type;
	double _s;
	double _v;
	double _a;
};

struct LIBCANVAS_API HSV
{
	HSV ();
	HSV (double h, double s, double v, double a = 1.0);
	HSV (Color);

	double h;
	double s;
	double v;
	double a;

	std::string to_string() const;
	bool is_gray() const;

	Color color() const { return hsva_to_color (h,s, v, a); }
	operator Color() const { return color(); }

	HSV mod (SVAModifier const & svam);

	HSV operator+ (const HSV&) const;
	HSV operator- (const HSV&) const;

	HSV& operator=(Color);
	HSV& operator=(const std::string&);

	bool operator== (const HSV& other);

	double distance (const HSV& other) const;
	HSV delta (const HSV& other) const;

	HSV darker (double factor = 1.3) const { return shade (factor); }
	HSV lighter (double factor = 0.7) const { return shade (factor); }

	HSV shade (double factor) const;
	HSV mix (const HSV& other, double amt) const;

	HSV opposite() const;
	HSV complement() const { return opposite(); }

	HSV bw_text () const;
	HSV text() const;
	HSV selected () const;
	HSV outline() const;

	void print (std::ostream&) const;

  protected:
	void clamp ();
};


}

std::ostream& operator<<(std::ostream& o, const ArdourCanvas::HSV& hsv);

#endif /* __ardour_canvas_colors_h__ */
