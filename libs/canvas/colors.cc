/*
    Copyright (C) 2014 Paul Davis

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

#include <algorithm>
#include <sstream>
#include <cmath>
#include <stdint.h>
#include <cfloat>

#include "pbd/failed_constructor.h"
#include "pbd/locale_guard.h"
#include "pbd/string_convert.h"

#include "canvas/colors.h"
#include "canvas/colorspace.h"

using namespace std;
using namespace ArdourCanvas;

using std::max;
using std::min;

ArdourCanvas::Color
ArdourCanvas::change_alpha (Color c, double a)
{
	return ((c & ~0xff) | (lrintf (a*255.0) & 0xff));
}

void
ArdourCanvas::color_to_hsv (Color color, double& h, double& s, double& v)
{
	double a;
	color_to_hsva (color, h, s, v, a);
}

void
ArdourCanvas::color_to_hsva (Color color, double& h, double& s, double& v, double& a)
{
	double r, g, b;
	double cmax;
	double cmin;
	double delta;

	color_to_rgba (color, r, g, b, a);

	if (r > g) {
		cmax = max (r, b);
	} else {
		cmax = max (g, b);
	}

	if (r < g) {
		cmin = min (r, b);
	} else {
		cmin = min (g, b);
	}

	v = cmax;

	delta = cmax - cmin;

	if (cmax == 0) {
		// r = g = b == 0 ... v is undefined, s = 0
		s = 0.0;
		h = 0.0;
		return;
	}

	if (delta != 0.0) {
		if (cmax == r) {
			h = fmod ((g - b)/delta, 6.0);
		} else if (cmax == g) {
			h = ((b - r)/delta) + 2;
		} else {
			h = ((r - g)/delta) + 4;
		}

		h *= 60.0;

		if (h < 0.0) {
			/* negative values are legal but confusing, because
			   they alias positive values.
			*/
			h = 360 + h;
		}
	}

	if (delta == 0 || cmax == 0) {
		s = 0;
	} else {
		s = delta / cmax;
	}
}

ArdourCanvas::Color
ArdourCanvas::hsva_to_color (double h, double s, double v, double a)
{
	s = min (1.0, max (0.0, s));
	v = min (1.0, max (0.0, v));

	if (s == 0) {
		return rgba_to_color (v, v, v, a);
	}

	h = fmod (h + 360.0, 360.0);

	double c = v * s;
        double x = c * (1.0 - fabs(fmod(h / 60.0, 2) - 1.0));
        double m = v - c;

        if (h >= 0.0 && h < 60.0) {
		return rgba_to_color (c + m, x + m, m, a);
        } else if (h >= 60.0 && h < 120.0) {
		return rgba_to_color (x + m, c + m, m, a);
        } else if (h >= 120.0 && h < 180.0) {
		return rgba_to_color (m, c + m, x + m, a);
        } else if (h >= 180.0 && h < 240.0) {
		return rgba_to_color (m, x + m, c + m, a);
        } else if (h >= 240.0 && h < 300.0) {
		return rgba_to_color (x + m, m, c + m, a);
        } else if (h >= 300.0 && h < 360.0) {
		return rgba_to_color (c + m, m, x + m, a);
        }
	return rgba_to_color (m, m, m, a);
}

void
ArdourCanvas::color_to_rgba (Color color, double& r, double& g, double& b, double& a)
{
	r = ((color >> 24) & 0xff) / 255.0;
	g = ((color >> 16) & 0xff) / 255.0;
	b = ((color >>  8) & 0xff) / 255.0;
	a = ((color >>  0) & 0xff) / 255.0;
}

ArdourCanvas::Color
ArdourCanvas::rgba_to_color (double r, double g, double b, double a)
{
	/* clamp to [0 .. 1] range */

	r = min (1.0, max (0.0, r));
	g = min (1.0, max (0.0, g));
	b = min (1.0, max (0.0, b));
	a = min (1.0, max (0.0, a));

	/* convert to [0..255] range */

	unsigned int rc, gc, bc, ac;
	rc = rint (r * 255.0);
	gc = rint (g * 255.0);
	bc = rint (b * 255.0);
	ac = rint (a * 255.0);

	/* build-an-integer */

	return (rc << 24) | (gc << 16) | (bc << 8) | ac;
}

// Inverse of sRGB "gamma" function.
static inline double
inv_gam_sRGB (double c)
{
        if (c <= 0.04045) {
                return c/12.92;
        } else {
                return pow(((c+0.055)/(1.055)),2.4);
        }
}

// sRGB "gamma" function
static inline int
gam_sRGB(double v)
{
        if (v <= 0.0031308) {
                v *= 12.92;
        } else {
                v = 1.055 * pow (v, 1.0 / 2.4) - 0.055;
        }
        return int (v*255+.5);
}

static double
luminance (uint32_t c)
{
        // sRGB luminance(Y) values
        const double rY = 0.212655;
        const double gY = 0.715158;
        const double bY = 0.072187;

        double r, g, b, a;

        ArdourCanvas::color_to_rgba (c, r, g, b, a);

        return (gam_sRGB (rY*inv_gam_sRGB(r) + gY*inv_gam_sRGB(g) + bY*inv_gam_sRGB(b))) / 255.0;
}

uint32_t
ArdourCanvas::contrasting_text_color (uint32_t c)
{
	/* use a slightly off-white... XXX should really look this up */

        static const uint32_t white = ArdourCanvas::rgba_to_color (0.98, 0.98, 0.98, 1.0);
        static const uint32_t black = ArdourCanvas::rgba_to_color (0.0, 0.0, 0.0, 1.0);

	return (luminance (c) < 0.50) ? white : black;
}



HSV::HSV ()
	: h (0.0)
	, s (1.0)
	, v (1.0)
	, a (1.0)
{
}

HSV::HSV (double hh, double ss, double vv, double aa)
	: h (hh)
	, s (ss)
	, v (vv)
	, a (aa)
{
	if (h < 0.0) {
		/* normalize negative hue values into positive range */
		h = 360.0 + h;
	}
}

HSV::HSV (Color c)
{
	color_to_hsva (c, h, s, v, a);
}

HSV::HSV (const std::string& str)
{
	stringstream ss (str);
	ss >> h;
	ss >> s;
	ss >> v;
	ss >> a;
}

string
HSV::to_string () const
{
	PBD::LocaleGuard lg;
	stringstream ss;
	ss << PBD::to_string(h) << ' ';
	ss << PBD::to_string(s) << ' ';
	ss << PBD::to_string(v) << ' ';
	ss << PBD::to_string(a);
	return ss.str();
}

bool
HSV::is_gray () const
{
	return s == 0;
}

void
HSV::clamp ()
{
	h = fmod (h, 360.0);
	if (h < 0.0) {
		/* normalize negative hue values into positive range */
		h = 360.0 + h;
	}
	s = min (1.0, s);
	v = min (1.0, v);
	a = min (1.0, a);
}

HSV
HSV::operator+ (const HSV& operand) const
{
	HSV hsv;
	hsv.h = h + operand.h;
	hsv.s = s + operand.s;
	hsv.v = v + operand.v;
	hsv.a = a + operand.a;
	hsv.clamp ();
	return hsv;
}

HSV
HSV::operator- (const HSV& operand) const
{
	HSV hsv;
	hsv.h = h - operand.h;
	hsv.s = s - operand.s;
	hsv.v = s - operand.v;
	hsv.a = a - operand.a;
	hsv.clamp ();
	return hsv;
}

HSV&
HSV::operator=(Color c)
{
	color_to_hsva (c, h, s, v, a);
	clamp ();
	return *this;
}

HSV&
HSV::operator=(const std::string& str)
{
	uint32_t c;
	c = strtol (str.c_str(), 0, 16);
	color_to_hsva (c, h, s, v, a);
	clamp ();
	return *this;
}

bool
HSV::operator== (const HSV& other)
{
	return h == other.h &&
		s == other.s &&
		v == other.v &&
		a == other.a;
}

HSV
HSV::shade (double factor) const
{
	HSV hsv (*this);

	/* algorithm derived from a google palette website
	   and analysis of their color palettes.

	   basic rule: to make a color darker, increase its saturation
	   until it reaches 88%, but then additionally reduce value/lightness
	   by a larger amount.

	   invert rule to make a color lighter.
	*/

	if (factor > 1.0) {
		if (s < 88) {
			hsv.v += (hsv.v * (factor * 10.0));
		}
		hsv.s *= factor;
	} else {
		if (s < 88) {
			hsv.v -= (hsv.v * (factor * 10.0));
		}
		hsv.s *= factor;
	}

	hsv.clamp();

	return hsv;
}

HSV
HSV::outline () const
{
	if (luminance (color()) < 0.50) {
		/* light color, darker outline: black with 15% opacity */
		return HSV (0.0, 0.0, 0.0, 0.15);
	} else {
		/* dark color, lighter outline: white with 15% opacity */
		return HSV (0.0, 0.0, 1.0, 0.15);
	}
}

HSV
HSV::mix (const HSV& other, double amount) const
{
	HSV hsv;

	hsv.h = h + (amount * (other.h - h));
	hsv.v = v + (amount * (other.s - s));
	hsv.s = s + (amount * (other.v - v));

	hsv.clamp();

	return hsv;
}

HSV
HSV::delta (const HSV& other) const
{
	HSV d;

	if (is_gray() && other.is_gray()) {
		d.h = 0.0;
		d.s = 0.0;
		d.v = v - other.v;
	} else {
		d.h = h - other.h;
		d.s = s - other.s;
		d.v = v - other.v;
	}
	d.a = a - other.a;
	/* do not clamp - we are returning a delta */
	return d;
}

double
HSV::distance (const HSV& other) const
{
	if (is_gray() && other.is_gray()) {
		/* human color perception of achromatics generates about 450
		   distinct colors. By contrast, CIE94 could give a maximal
		   perceptual distance of sqrt ((360^2) + 1 + 1) = 360. The 450
		   are not evenly spread (Webers Law), so lets use 360 as an
		   approximation of the number of distinct achromatics.

		   So, scale up the achromatic difference to give about
		   a maximal distance between v = 1.0 and v = 0.0 of 360.

		   A difference of about 0.0055 will generate a return value of
		   2, which is roughly the limit of human perceptual
		   discrimination for chromatics.
		*/
		return fabs (360.0 * (v - other.v));
	}

	if (is_gray() != other.is_gray()) {
		/* no comparison possible */
		return DBL_MAX;
	}

	/* Use CIE94 definition for now */

	double sL, sA, sB;
	double oL, oA, oB;
	double r, g, b, alpha;  // Careful, "a" is a field of this
	Color c;

	c = hsva_to_color (h, s, v, a);
	color_to_rgba (c, r, g, b, alpha);
	Rgb2Lab (&sL, &sA, &sB, r, g, b);

	c = hsva_to_color (other.h, other.s, other.v, other.a);
	color_to_rgba (c, r, g, b, alpha);
	Rgb2Lab (&oL, &oA, &oB, r, g, b);

	// Weighting factors depending on the application (1 = default)

	const double whtL = 1.0;
	const double whtC = 1.0;
	const double whtH = 1.0;

	const double xC1 = sqrt ((sA * sA) + (sB * oB));
	const double xC2 = sqrt ((oA * oA) + (oB * oB));
	double xDL = oL - sL;
	double xDC = xC2 - xC1;
	const double xDE = sqrt (((sL - oL) * (sL - oL))
				 + ((sA - oA) * (sA - oA))
				 + ((sB - oB) * (sB - oB)));

	double xDH;

	if (sqrt (xDE) > (sqrt (abs (xDL)) + sqrt (abs (xDC)))) {
		xDH = sqrt ((xDE * xDE) - (xDL * xDL) - (xDC * xDC));
	} else {
		xDH = 0;
	}

	const double xSC = 1 + (0.045 * xC1);
	const double xSH = 1 + (0.015 * xC1);

	xDL /= whtL;
	xDC /= whtC * xSC;
	xDH /= whtH * xSH;

	return sqrt ((xDL * xDL) + (xDC * xDC) + (xDH * xDH));
}

HSV
HSV::opposite () const
{
	HSV hsv (*this);
	hsv.h = fmod (h + 180.0, 360.0);
	return hsv;
}

HSV
HSV::bw_text () const
{
	return HSV (contrasting_text_color (color()));
}

HSV
HSV::text () const
{
	return opposite ();
}

HSV
HSV::selected () const
{
	/* XXX hack */
	return HSV (Color (0xff0000));
}


void
HSV::print (std::ostream& o) const
{
	if (!is_gray()) {
		o << '(' << h << ',' << s << ',' << v << ',' << a << ')';
	} else {
		o << "gray(" << v << ')';
	}
}


std::ostream& operator<<(std::ostream& o, const ArdourCanvas::HSV& hsv) { hsv.print (o); return o; }

HSV
HSV::mod (SVAModifier const & svam)
{
	return svam (*this);
}

SVAModifier::SVAModifier (string const &str)
	: type (Add)
	, _s (0)
	, _v (0)
	, _a (0)
{
	from_string (str);
}

void
SVAModifier::from_string (string const & str)
{
	char op;
	stringstream ss (str);
	string mod;

	ss >> op;

	switch (op) {
	case '*':
		type = Multiply;
		/* no-op values for multiply */
		_s = 1.0;
		_v = 1.0;
		_a = 1.0;
		break;
	case '+':
		type = Add;
		/* no-op values for add */
		_s = 0.0;
		_v = 0.0;
		_a = 0.0;
		break;
	case '=':
		type = Assign;
		/* this will avoid assignment in operator() (see below) */
		_s = -1.0;
		_v = -1.0;
		_a = -1.0;
		break;
	default:
		throw failed_constructor ();
	}

	string::size_type pos;

	while (ss) {
		ss >> mod;
		if ((pos = mod.find ("alpha:")) != string::npos) {
			_a = PBD::string_to<double>(mod.substr (pos+6));
		} else if ((pos = mod.find ("saturate:")) != string::npos) {
			_s = PBD::string_to<double>(mod.substr (pos+9));
		} else if ((pos = mod.find ("darkness:")) != string::npos) {
			_v = PBD::string_to<double>(mod.substr (pos+9));
		} else {
			throw failed_constructor ();
		}
	}
}

string
SVAModifier::to_string () const
{
	PBD::LocaleGuard lg;
	stringstream ss;

	switch (type) {
	case Add:
		ss << '+';
		break;
	case Multiply:
		ss << '*';
		break;
	case Assign:
		ss << '=';
		break;
	}

	if (_s >= 0.0) {
		ss << " saturate:" << PBD::to_string(_s);
	}

	if (_v >= 0.0) {
		ss << " darker:" << PBD::to_string(_v);
	}

	if (_a >= 0.0) {
		ss << " alpha:" << PBD::to_string(_a);
	}

	return ss.str();
}

HSV
SVAModifier::operator () (HSV& hsv)  const
{
	HSV r (hsv);

	switch (type) {
	case Add:
		r.s += _s;
		r.v += _v;
		r.a += _a;
		break;
	case Multiply:
		r.s *= _s;
		r.v *= _v;
		r.a *= _a;
		break;
	case Assign:
		if (_s >= 0.0) {
			r.s = _s;
		}
		if (_v >= 0.) {
			r.v = _v;
		}
		if (_a >= 0.0) {
			r.a = _a;
		}
		break;
	}

	return r;
}

ArdourCanvas::Color
ArdourCanvas::color_at_alpha (ArdourCanvas::Color c, double a)
{
	double r, g, b, unused;
	color_to_rgba (c, r, g, b, unused);
	return rgba_to_color( r,g,b, a );
}
