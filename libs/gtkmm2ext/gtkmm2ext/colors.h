/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef _GTKMM2EXT_COLORS_H_
#define _GTKMM2EXT_COLORS_H_

#include<stdint.h>

#include <cairomm/context.h>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext
{

typedef uint32_t Color;

extern LIBGTKMM2EXT_API Color random_color ();

/* conventient way to use Gtkmm2ext::Color with libcairo */
extern LIBGTKMM2EXT_API void set_source_rgba (Cairo::RefPtr<Cairo::Context>, Gtkmm2ext::Color);
extern LIBGTKMM2EXT_API void set_source_rgb_a (Cairo::RefPtr<Cairo::Context>, Gtkmm2ext::Color, float alpha);  //override the color's alpha

extern LIBGTKMM2EXT_API void set_source_rgba (cairo_t*, Gtkmm2ext::Color);
extern LIBGTKMM2EXT_API void set_source_rgb_a (cairo_t*, Gtkmm2ext::Color, float alpha);  //override the color's alpha


struct LIBGTKMM2EXT_API HSV;
struct LIBGTKMM2EXT_API HSVA;

extern LIBGTKMM2EXT_API Color change_alpha (Color, double alpha);

extern LIBGTKMM2EXT_API Color hsva_to_color (double h, double s, double v, double a = 1.0);
extern LIBGTKMM2EXT_API void  color_to_hsva (Color color, double& h, double& s, double& v, double& a);
extern LIBGTKMM2EXT_API Color color_at_alpha (Color, double a);
extern LIBGTKMM2EXT_API void  color_to_hsv (Color color, double& h, double& s, double& v);
extern LIBGTKMM2EXT_API void  color_to_rgba (Color, double& r, double& g, double& b, double& a);
extern LIBGTKMM2EXT_API Color rgba_to_color (double r, double g, double b, double a);

uint32_t LIBGTKMM2EXT_API contrasting_text_color (uint32_t c);

struct LIBGTKMM2EXT_API HSV;

class LIBGTKMM2EXT_API SVAModifier
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

struct LIBGTKMM2EXT_API HSV
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

	HSV darker (double factor = 1.3) const;
	HSV lighter (double factor = 0.7) const;

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

} /* namespace */

std::ostream& operator<<(std::ostream& o, const Gtkmm2ext::HSV& hsv);

#endif
