/*
    Copyright (C) 1999-2014 Paul Davis

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

#include <unistd.h>
#include <cstdlib>
#include <cstdio> /* for snprintf, grrr */

#include <glibmm/miscutils.h>

#include "pbd/failed_constructor.h"
#include "pbd/xml++.h"
#include "pbd/file_utils.h"
#include "pbd/error.h"

#include "gtkmm2ext/rgb_macros.h"

#include "ardour/filesystem_paths.h"

#include "ardour_ui.h"
#include "global_signals.h"
#include "ui_config.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourCanvas;

static const char* ui_config_file_name = "ui_config";
static const char* default_ui_config_file_name = "default_ui_config";
UIConfiguration* UIConfiguration::_instance = 0;

static const double hue_cnt = 18.0;

UIConfiguration::UIConfiguration ()
	:
#undef  UI_CONFIG_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,name,val) var (name,val),
#define CANVAS_STRING_VARIABLE(var,name) var (name),
#define CANVAS_FONT_VARIABLE(var,name) var (name),
#include "ui_config_vars.h"
#include "canvas_vars.h"
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_STRING_VARIABLE
#undef  CANVAS_FONT_VARIABLE

	/* initialize all the base colors using default
	   colors for now. these will be reset when/if
	   we load the UI config file.
	*/

#undef CANVAS_BASE_COLOR
#define CANVAS_BASE_COLOR(var,name,val) var (name,quantized (val)),
#include "base_colors.h"
#undef CANVAS_BASE_COLOR

#undef CANVAS_COLOR
#define CANVAS_COLOR(var,name,base,modifier) var (base,modifier),
#include "colors.h"
#undef CANVAS_COLOR

	_dirty (false)
{
	_instance = this;

	/* pack all base colors into the configurable color map so that
	   derived colors can use them.
	*/
	  
#undef CANVAS_BASE_COLOR
#define CANVAS_BASE_COLOR(var,name,color) configurable_colors.insert (make_pair (name,&var));
#include "base_colors.h"
#undef CANVAS_BASE_COLOR

#undef CANVAS_COLOR
#define CANVAS_COLOR(var,name,base,modifier) relative_colors.insert (make_pair (name,var));
#include "colors.h"
#undef CANVAS_COLOR

	load_state();

	// regenerate_relative_definitions ();

	color_compute ();

	ARDOUR_UI_UTILS::ColorsChanged.connect (boost::bind (&UIConfiguration::color_theme_changed, this));
}

UIConfiguration::~UIConfiguration ()
{
}

Color
UIConfiguration::quantized (Color c)
{
	HSV hsv (c);
	hsv.h = hue_cnt * (round (hsv.h/hue_cnt));
	return hsv.color ();
}

void
UIConfiguration::print_relative_def (string camelcase, string name, Color c)
{
	HSV variable (c);
	HSV closest;
	double shortest_distance = DBL_MAX;
	string closest_name;

	map<string,ColorVariable<Color>*>::iterator f;
	std::map<std::string,HSV> palette;

	for (f = configurable_colors.begin(); f != configurable_colors.end(); ++f) {
		palette.insert (make_pair (f->first, HSV (f->second->get())));
	}

	for (map<string,HSV>::iterator f = palette.begin(); f != palette.end(); ++f) {
		
		double d;
		HSV fixed (f->second);
		
		if (fixed.is_gray() || variable.is_gray()) {
			/* at least one is achromatic; HSV::distance() will do
			 * the right thing
			 */
			d = fixed.distance (variable);
		} else {
			/* chromatic: compare ONLY hue because our task is
			   to pick the HUE closest and then compute
			   a modifier. We want to keep the number of 
			   hues low, and by computing perceptual distance 
			   we end up finding colors that are to each
			   other without necessarily be close in hue.
			*/
			d = fabs (variable.h - fixed.h);
		}

		if (d < shortest_distance) {
			closest = fixed;
			closest_name = f->first;
			shortest_distance = d;
		}
	}
	
	/* we now know the closest color of the fixed colors to 
	   this variable color. Compute the HSV diff and
	   use it to redefine the variable color in terms of the
	   fixed one.
	*/
	
	HSV delta = variable.delta (closest);

	/* quantize hue delta so we don't end up with many subtle hues caused
	 * by original color choices
	 */

	delta.h = hue_cnt * (round (delta.h/hue_cnt));

	cerr << "CANVAS_COLOR(" << camelcase << ",\"" << name << "\", \"" << closest_name <<  "\", HSV(" 
	     << delta.h << ',' << delta.s << ',' << delta.v << ',' << variable.a << ")) /*" 
	     << shortest_distance << " */" << endl;
}

void 
UIConfiguration::regenerate_relative_definitions ()
{
	/* this takes the color definitions from around ardour 3.5.3600,
	   quantizes their hues, then prints out macros to be used
	   when defining these colors relative to the current
	   base palette. It doesn't need to be called unless
	   we change the base palette defaults.
	*/
	
	map<string,HSV> c;
        c.insert (make_pair ("active crossfade", HSV (0x20b2af2e)));
        c.insert (make_pair ("arrange base", HSV (0x595959ff)));
        c.insert (make_pair ("audio bus base", HSV (0x73829968)));
        c.insert (make_pair ("audio master bus base", HSV (0x00000000)));
        c.insert (make_pair ("audio track base", HSV (0x9daac468)));
        c.insert (make_pair ("automation line", HSV (0x44bc59ff)));
        c.insert (make_pair ("automation track fill", HSV (0xa0a0ce68)));
        c.insert (make_pair ("automation track outline", HSV (0x282828ff)));
        c.insert (make_pair ("cd marker bar", HSV (0x9496a3cc)));
        c.insert (make_pair ("crossfade editor base", HSV (0x282d49ff)));
        c.insert (make_pair ("crossfade editor line", HSV (0x000000ff)));
        c.insert (make_pair ("crossfade editor line shading", HSV (0x00a0d154)));
        c.insert (make_pair ("crossfade editor point fill", HSV (0x00ff00ff)));
        c.insert (make_pair ("crossfade editor point outline", HSV (0x0000ffff)));
        c.insert (make_pair ("crossfade editor wave", HSV (0xffffff28)));
        c.insert (make_pair ("selected crossfade editor wave fill", HSV (0x00000000)));
        c.insert (make_pair ("crossfade line", HSV (0x000000ff)));
        c.insert (make_pair ("edit point", HSV (0x0000ffff)));
        c.insert (make_pair ("entered automation line", HSV (0xdd6363ff)));
        c.insert (make_pair ("control point fill", HSV (0xffffff66)));
        c.insert (make_pair ("control point outline", HSV (0xff0000ee)));
        c.insert (make_pair ("control point selected", HSV (0x55ccccff)));
        c.insert (make_pair ("entered gain line", HSV (0xdd6363ff)));
        c.insert (make_pair ("entered marker", HSV (0xdd6363ff)));
        c.insert (make_pair ("frame handle", HSV (0x7c00ff96)));
        c.insert (make_pair ("gain line", HSV (0x00bc20ff)));
        c.insert (make_pair ("gain line inactive", HSV (0x9fbca4c5)));
        c.insert (make_pair ("ghost track base", HSV (0x603e7cc6)));
        c.insert (make_pair ("ghost track midi outline", HSV (0x00000000)));
        c.insert (make_pair ("ghost track wave", HSV (0x202020d9)));
        c.insert (make_pair ("ghost track wave fill", HSV (0x20202060)));
        c.insert (make_pair ("ghost track wave clip", HSV (0x202020d9)));
        c.insert (make_pair ("ghost track zero line", HSV (0xe500e566)));
        c.insert (make_pair ("image track", HSV (0xddddd8ff)));
        c.insert (make_pair ("inactive crossfade", HSV (0xe8ed3d77)));
        c.insert (make_pair ("inactive fade handle", HSV (0xbbbbbbaa)));
        c.insert (make_pair ("inactive group tab", HSV (0x434343ff)));
        c.insert (make_pair ("location cd marker", HSV (0x1ee8c4ff)));
        c.insert (make_pair ("location loop", HSV (0x35964fff)));
        c.insert (make_pair ("location marker", HSV (0xc4f411ff)));
        c.insert (make_pair ("location punch", HSV (0x7c3a3aff)));
        c.insert (make_pair ("location range", HSV (0x497a59ff)));
        c.insert (make_pair ("marker bar", HSV (0x99a1adcc)));
        c.insert (make_pair ("marker bar separator", HSV (0x555555ff)));
        c.insert (make_pair ("marker drag line", HSV (0x004f00f9)));
        c.insert (make_pair ("marker label", HSV (0x000000ff)));
        c.insert (make_pair ("marker track", HSV (0xddddd8ff)));
        c.insert (make_pair ("measure line bar", HSV (0xffffff9c)));
        c.insert (make_pair ("measure line beat", HSV (0xa29e9e76)));
        c.insert (make_pair ("meter bar", HSV (0x626470cc)));
        c.insert (make_pair ("meter fill: 0", HSV (0x008800ff)));
        c.insert (make_pair ("meter fill: 1", HSV (0x00aa00ff)));
        c.insert (make_pair ("meter fill: 2", HSV (0x00ff00ff)));
        c.insert (make_pair ("meter fill: 3", HSV (0x00ff00ff)));
        c.insert (make_pair ("meter fill: 4", HSV (0xfff000ff)));
        c.insert (make_pair ("meter fill: 5", HSV (0xfff000ff)));
        c.insert (make_pair ("meter fill: 6", HSV (0xff8000ff)));
        c.insert (make_pair ("meter fill: 7", HSV (0xff8000ff)));
        c.insert (make_pair ("meter fill: 8", HSV (0xff0000ff)));
        c.insert (make_pair ("meter fill: 9", HSV (0xff0000ff)));
        c.insert (make_pair ("meter background: bottom", HSV (0x333333ff)));
        c.insert (make_pair ("meter background: top", HSV (0x444444ff)));
        c.insert (make_pair ("midi meter fill: 0", HSV (0xeffaa1ff)));
        c.insert (make_pair ("midi meter fill: 1", HSV (0xf2c97dff)));
        c.insert (make_pair ("midi meter fill: 2", HSV (0xf2c97dff)));
        c.insert (make_pair ("midi meter fill: 3", HSV (0xf48f52ff)));
        c.insert (make_pair ("midi meter fill: 4", HSV (0xf48f52ff)));
        c.insert (make_pair ("midi meter fill: 5", HSV (0xf83913ff)));
        c.insert (make_pair ("midi meter fill: 6", HSV (0xf83913ff)));
        c.insert (make_pair ("midi meter fill: 7", HSV (0x8fc78eff)));
        c.insert (make_pair ("midi meter fill: 8", HSV (0x8fc78eff)));
        c.insert (make_pair ("midi meter fill: 9", HSV (0x00f45600)));
        c.insert (make_pair ("meterbridge peakindicator: fill", HSV (0x444444ff)));
        c.insert (make_pair ("meterbridge peakindicator: fill active", HSV (0xff0000ff)));
        c.insert (make_pair ("meterbridge label: fill", HSV (0x444444ff)));
        c.insert (make_pair ("meterbridge label: fill active", HSV (0x333333ff)));
        c.insert (make_pair ("meterbridge label: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("meter marker", HSV (0xf2425bff)));
        c.insert (make_pair ("midi bus base", HSV (0x00000000)));
        c.insert (make_pair ("midi frame base", HSV (0x393d3766)));
        c.insert (make_pair ("midi note inactive channel", HSV (0x00000000)));
        c.insert (make_pair ("midi note color min", HSV (0x3f542aff)));
        c.insert (make_pair ("midi note color mid", HSV (0x7ea854ff)));
        c.insert (make_pair ("midi note color max", HSV (0xbfff80ff)));
        c.insert (make_pair ("selected midi note color min", HSV (0x1e1e33ff)));
        c.insert (make_pair ("selected midi note color mid", HSV (0x51518aff)));
        c.insert (make_pair ("selected midi note color max", HSV (0x8383deff)));
        c.insert (make_pair ("midi note selected", HSV (0xb2b2ffff)));
        c.insert (make_pair ("midi note velocity text", HSV (0xf4f214bc)));
        c.insert (make_pair ("midi patch change fill", HSV (0x50555aa0)));
        c.insert (make_pair ("midi patch change outline", HSV (0xc0c5caff)));
        c.insert (make_pair ("midi patch change inactive channel fill", HSV (0x50555ac0)));
        c.insert (make_pair ("midi patch change inactive channel outline", HSV (0x20252ac0)));
        c.insert (make_pair ("midi sysex fill", HSV (0xf1e139a0)));
        c.insert (make_pair ("midi sysex outline", HSV (0xa7a7d4ff)));
        c.insert (make_pair ("midi select rect fill", HSV (0x8888ff88)));
        c.insert (make_pair ("midi select rect outline", HSV (0x5555ffff)));
        c.insert (make_pair ("midi track base", HSV (0xb3cca35f)));
        c.insert (make_pair ("name highlight fill", HSV (0x0000ffff)));
        c.insert (make_pair ("name highlight outline", HSV (0x7c00ff96)));
        c.insert (make_pair ("piano roll black outline", HSV (0xf4f4f476)));
        c.insert (make_pair ("piano roll black", HSV (0x6c6e6a6b)));
        c.insert (make_pair ("piano roll white", HSV (0x979b9565)));
        c.insert (make_pair ("play head", HSV (0xff0000ff)));
        c.insert (make_pair ("processor automation line", HSV (0x7aa3f9ff)));
        c.insert (make_pair ("punch line", HSV (0xa80000ff)));
        c.insert (make_pair ("range drag bar rect", HSV (0x969696c6)));
        c.insert (make_pair ("range drag rect", HSV (0x82c696c6)));
        c.insert (make_pair ("range marker bar", HSV (0x7d7f8ccc)));
        c.insert (make_pair ("recording rect", HSV (0xcc2828ff)));
        c.insert (make_pair ("recorded waveform fill", HSV (0xffffffd9)));
        c.insert (make_pair ("recorded waveform outline", HSV (0x0f0f1fff)));
        c.insert (make_pair ("rubber band rect", HSV (0xc6c6c659)));
        c.insert (make_pair ("ruler base", HSV (0x2c2121ff)));
        c.insert (make_pair ("ruler text", HSV (0xe5e5e5ff)));
        c.insert (make_pair ("selected crossfade editor line", HSV (0x00dbdbff)));
        c.insert (make_pair ("selected crossfade editor wave", HSV (0xf9ea14a0)));
        c.insert (make_pair ("selected region base", HSV (0x585c61ff)));
        c.insert (make_pair ("selected waveform fill", HSV (0xffa500d9)));
        c.insert (make_pair ("selected waveform outline", HSV (0x0f0f0fcc)));
        c.insert (make_pair ("selection rect", HSV (0xe8f4d377)));
        c.insert (make_pair ("selection", HSV (0x636363b2)));
        c.insert (make_pair ("shuttle", HSV (0x6bb620ff)));
        c.insert (make_pair ("silence", HSV (0x9efffd7a)));
        c.insert (make_pair ("silence text", HSV (0x0e066cff)));
        c.insert (make_pair ("mono panner outline", HSV (0x33445eff)));
        c.insert (make_pair ("mono panner fill", HSV (0x7a9bccc9)));
        c.insert (make_pair ("mono panner text", HSV (0x000000ff)));
        c.insert (make_pair ("mono panner bg", HSV (0x2e2929ff)));
        c.insert (make_pair ("mono panner position fill", HSV (0x7a89b3ff)));
        c.insert (make_pair ("mono panner position outline", HSV (0x33445eff)));
        c.insert (make_pair ("stereo panner outline", HSV (0x33445eff)));
        c.insert (make_pair ("stereo panner fill", HSV (0x7a9accc9)));
        c.insert (make_pair ("stereo panner text", HSV (0x000000ff)));
        c.insert (make_pair ("stereo panner bg", HSV (0x2e2929ff)));
        c.insert (make_pair ("stereo panner rule", HSV (0x455c7fff)));
        c.insert (make_pair ("stereo panner mono outline", HSV (0xa05600ff)));
        c.insert (make_pair ("stereo panner mono fill", HSV (0xe99668ca)));
        c.insert (make_pair ("stereo panner mono text", HSV (0x000000ff)));
        c.insert (make_pair ("stereo panner mono bg", HSV (0x2e2929ff)));
        c.insert (make_pair ("stereo panner inverted outline", HSV (0xbf0a00ff)));
        c.insert (make_pair ("stereo panner inverted fill", HSV (0xe4a19cc9)));
        c.insert (make_pair ("stereo panner inverted text", HSV (0x000000ff)));
        c.insert (make_pair ("stereo panner inverted bg", HSV (0x2e2929ff)));
        c.insert (make_pair ("tempo bar", HSV (0x70727fcc)));
        c.insert (make_pair ("tempo marker", HSV (0xf2425bff)));
        c.insert (make_pair ("time axis frame", HSV (0x000000ff)));
        c.insert (make_pair ("selected time axis frame", HSV (0xee0000ff)));
        c.insert (make_pair ("time stretch fill", HSV (0xe2b5b596)));
        c.insert (make_pair ("time stretch outline", HSV (0x63636396)));
        c.insert (make_pair ("tracknumber label: fill", HSV (0x444444ff)));
        c.insert (make_pair ("tracknumber label: fill active", HSV (0x333333ff)));
        c.insert (make_pair ("tracknumber label: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("transport drag rect", HSV (0x969696c6)));
        c.insert (make_pair ("transport loop rect", HSV (0x1e7728f9)));
        c.insert (make_pair ("transport marker bar", HSV (0x8c8e98cc)));
        c.insert (make_pair ("transport punch rect", HSV (0x6d2828e5)));
        c.insert (make_pair ("trim handle locked", HSV (0xea0f0f28)));
        c.insert (make_pair ("trim handle", HSV (0x1900ff44)));
        c.insert (make_pair ("verbose canvas cursor", HSV (0xfffd2ebc)));
        c.insert (make_pair ("vestigial frame", HSV (0x0000000f)));
        c.insert (make_pair ("video timeline bar", HSV (0x303030ff)));
        c.insert (make_pair ("region base", HSV (0x838890ff)));
        c.insert (make_pair ("region area covered by another region", HSV (0x505050b0)));
        c.insert (make_pair ("waveform outline", HSV (0x000000ff)));
        c.insert (make_pair ("clipped waveform", HSV (0xff0000e5)));
        c.insert (make_pair ("waveform fill", HSV (0xffffffd9)));
        c.insert (make_pair ("zero line", HSV (0x7f7f7fe0)));
        c.insert (make_pair ("zoom rect", HSV (0xc6d1b26d)));
        c.insert (make_pair ("monitor knob", HSV (0x555050ff)));
        c.insert (make_pair ("monitor knob: arc start", HSV (0x5d90b0ff)));
        c.insert (make_pair ("monitor knob: arc end", HSV (0x154c6eff)));
        c.insert (make_pair ("button border", HSV (0x000000f0)));
        c.insert (make_pair ("border color", HSV (0x00000000)));
        c.insert (make_pair ("processor prefader: fill", HSV (0x873c3cff)));
        c.insert (make_pair ("processor prefader: fill active", HSV (0x603535ff)));
        c.insert (make_pair ("processor prefader: led", HSV (0x26550eff)));
        c.insert (make_pair ("processor prefader: led active", HSV (0x78cb4eff)));
        c.insert (make_pair ("processor prefader: text", HSV (0xaaaaa3ff)));
        c.insert (make_pair ("processor prefader: text active", HSV (0xeeeeecff)));
        c.insert (make_pair ("processor fader: fill", HSV (0x5d90b0ff)));
        c.insert (make_pair ("processor fader: fill active", HSV (0x256d8fff)));
        c.insert (make_pair ("processor fader: led", HSV (0x26550eff)));
        c.insert (make_pair ("processor fader: led active", HSV (0x78cb4eff)));
        c.insert (make_pair ("processor fader: text", HSV (0xaaaaa3ff)));
        c.insert (make_pair ("processor fader: text active", HSV (0xeeeeecff)));
        c.insert (make_pair ("processor postfader: fill", HSV (0x455a3cff)));
        c.insert (make_pair ("processor postfader: fill active", HSV (0x254528ff)));
        c.insert (make_pair ("processor postfader: led", HSV (0x26550eff)));
        c.insert (make_pair ("processor postfader: led active", HSV (0x78cb4eff)));
        c.insert (make_pair ("processor postfader: text", HSV (0xaaaaa3ff)));
        c.insert (make_pair ("processor postfader: text active", HSV (0xeeeeecff)));
        c.insert (make_pair ("processor control button: fill", HSV (0x222222ff)));
        c.insert (make_pair ("processor control button: fill active", HSV (0x333333ff)));
        c.insert (make_pair ("processor control button: led", HSV (0x101010ff)));
        c.insert (make_pair ("processor control button: led active", HSV (0x5d90b0ff)));
        c.insert (make_pair ("processor control button: text", HSV (0xffffffff)));
        c.insert (make_pair ("processor control button: text active", HSV (0xffffffff)));
        c.insert (make_pair ("midi device: fill", HSV (0x54555dff)));
        c.insert (make_pair ("midi device: fill active", HSV (0x45464cff)));
        c.insert (make_pair ("midi device: led", HSV (0x006600ff)));
        c.insert (make_pair ("midi device: led active", HSV (0x00ff00ff)));
        c.insert (make_pair ("midi device: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("midi device: text active", HSV (0xeeeeecff)));
        c.insert (make_pair ("monitor button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("monitor button: fill active", HSV (0xc56505ff)));
        c.insert (make_pair ("monitor button: led", HSV (0x660000ff)));
        c.insert (make_pair ("monitor button: led active", HSV (0xff0000ff)));
        c.insert (make_pair ("monitor button: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("monitor button: text active", HSV (0x1a1a1aff)));
        c.insert (make_pair ("solo isolate: fill", HSV (0x616268ff)));
        c.insert (make_pair ("solo isolate: fill active", HSV (0x564d48ff)));
        c.insert (make_pair ("solo isolate: led", HSV (0x660000ff)));
        c.insert (make_pair ("solo isolate: led active", HSV (0xff0000ff)));
        c.insert (make_pair ("solo isolate: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("solo isolate: text active", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("solo safe: fill", HSV (0x616268ff)));
        c.insert (make_pair ("solo safe: fill active", HSV (0x564d48ff)));
        c.insert (make_pair ("solo safe: led", HSV (0x660000ff)));
        c.insert (make_pair ("solo safe: led active", HSV (0xff0000ff)));
        c.insert (make_pair ("solo safe: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("solo safe: text active", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("meterbridge peaklabel", HSV (0xff1111ff)));
        c.insert (make_pair ("meter color BBC", HSV (0xffa500ff)));
        c.insert (make_pair ("monitor section cut: fill", HSV (0x5f5a58ff)));
        c.insert (make_pair ("monitor section cut: fill active", HSV (0xffa500ff)));
        c.insert (make_pair ("monitor section cut: led", HSV (0x473812ff)));
        c.insert (make_pair ("monitor section cut: led active", HSV (0x78cb4eff)));
        c.insert (make_pair ("monitor section cut: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("monitor section cut: text active", HSV (0x000000ff)));
        c.insert (make_pair ("monitor section dim: fill", HSV (0x5f5a58ff)));
        c.insert (make_pair ("monitor section dim: fill active", HSV (0xe58505ff)));
        c.insert (make_pair ("monitor section dim: led", HSV (0x00000000)));
        c.insert (make_pair ("monitor section dim: led active", HSV (0x78cb4eff)));
        c.insert (make_pair ("monitor section dim: text", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("monitor section dim: text active", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("monitor section solo: fill", HSV (0x5f5a58ff)));
        c.insert (make_pair ("monitor section solo: fill active", HSV (0x4dbb00ff)));
        c.insert (make_pair ("monitor section solo: led", HSV (0x473812ff)));
        c.insert (make_pair ("monitor section solo: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("monitor section solo: text", HSV (0x00000000)));
        c.insert (make_pair ("monitor section solo: text active", HSV (0x00000000)));
        c.insert (make_pair ("monitor section invert: fill", HSV (0x5f5a58ff)));
        c.insert (make_pair ("monitor section invert: fill active", HSV (0x4242d0ff)));
        c.insert (make_pair ("monitor section invert: led", HSV (0x473812ff)));
        c.insert (make_pair ("monitor section invert: led active", HSV (0x78cb4eff)));
        c.insert (make_pair ("monitor section invert: text", HSV (0x00000000)));
        c.insert (make_pair ("monitor section invert: text active", HSV (0x00000000)));
        c.insert (make_pair ("monitor section mono: fill", HSV (0x5f5a58ff)));
        c.insert (make_pair ("monitor section mono: fill active", HSV (0x3232c0ff)));
        c.insert (make_pair ("monitor section mono: led", HSV (0x473812ff)));
        c.insert (make_pair ("monitor section mono: led active", HSV (0x78cb4eff)));
        c.insert (make_pair ("monitor section mono: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("monitor section mono: text active", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("monitor section solo model: fill", HSV (0x5d5856ff)));
        c.insert (make_pair ("monitor section solo model: fill active", HSV (0x564d48ff)));
        c.insert (make_pair ("monitor section solo model: led", HSV (0x4f3300ff)));
        c.insert (make_pair ("monitor section solo model: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("monitor section solo model: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("monitor section solo model: text active", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("monitor solo override: fill", HSV (0x5d5856ff)));
        c.insert (make_pair ("monitor solo override: fill active", HSV (0x564d48ff)));
        c.insert (make_pair ("monitor solo override: led", HSV (0x4f3300ff)));
        c.insert (make_pair ("monitor solo override: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("monitor solo override: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("monitor solo override: text active", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("monitor solo exclusive: fill", HSV (0x5d5856ff)));
        c.insert (make_pair ("monitor solo exclusive: fill active", HSV (0x564c47ff)));
        c.insert (make_pair ("monitor solo exclusive: led", HSV (0x4f3300ff)));
        c.insert (make_pair ("monitor solo exclusive: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("monitor solo exclusive: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("monitor solo exclusive: text active", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("rude solo: fill", HSV (0x684d4dff)));
        c.insert (make_pair ("rude solo: fill active", HSV (0xe21b1bff)));
        c.insert (make_pair ("rude solo: led", HSV (0x00000000)));
        c.insert (make_pair ("rude solo: led active", HSV (0x00000000)));
        c.insert (make_pair ("rude solo: text", HSV (0x969696ff)));
        c.insert (make_pair ("rude solo: text active", HSV (0xe5e5e5ff)));
        c.insert (make_pair ("rude isolate: fill", HSV (0x21414fff)));
        c.insert (make_pair ("rude isolate: fill active", HSV (0xb6e5fdff)));
        c.insert (make_pair ("rude isolate: led", HSV (0x00000000)));
        c.insert (make_pair ("rude isolate: led active", HSV (0x000000ff)));
        c.insert (make_pair ("rude isolate: text", HSV (0x979797ff)));
        c.insert (make_pair ("rude isolate: text active", HSV (0x000000ff)));
        c.insert (make_pair ("rude audition: fill", HSV (0x684d4dff)));
        c.insert (make_pair ("rude audition: fill active", HSV (0xe21b1bff)));
        c.insert (make_pair ("rude audition: led", HSV (0x00000000)));
        c.insert (make_pair ("rude audition: led active", HSV (0x00000000)));
        c.insert (make_pair ("rude audition: text", HSV (0x979797ff)));
        c.insert (make_pair ("rude audition: text active", HSV (0xffffffff)));
        c.insert (make_pair ("feedback alert: fill", HSV (0x684d4dff)));
        c.insert (make_pair ("feedback alert: fill active", HSV (0xe21b1bff)));
        c.insert (make_pair ("feedback alert: led", HSV (0x00000000)));
        c.insert (make_pair ("feedback alert: led active", HSV (0x00000000)));
        c.insert (make_pair ("feedback alert: text", HSV (0x969696ff)));
        c.insert (make_pair ("feedback alert: text active", HSV (0xe5e5e5ff)));
        c.insert (make_pair ("mute button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("mute button: fill active", HSV (0xbbbb00ff)));
        c.insert (make_pair ("mute button: led", HSV (0x00000000)));
        c.insert (make_pair ("mute button: led active", HSV (0x00000000)));
        c.insert (make_pair ("mute button: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("mute button: text active", HSV (0x191919ff)));
        c.insert (make_pair ("solo button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("solo button: fill active", HSV (0x4dbb00ff)));
        c.insert (make_pair ("solo button: led", HSV (0x00000000)));
        c.insert (make_pair ("solo button: led active", HSV (0x00000000)));
        c.insert (make_pair ("solo button: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("solo button: text active", HSV (0x191919ff)));
        c.insert (make_pair ("invert button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("invert button: fill active", HSV (0x4242d0ff)));
        c.insert (make_pair ("invert button: led", HSV (0x473812ff)));
        c.insert (make_pair ("invert button: led active", HSV (0x78cb4eff)));
        c.insert (make_pair ("invert button: text", HSV (0xd7d7e8ff)));
        c.insert (make_pair ("invert button: text active", HSV (0xbfbfbfff)));
        c.insert (make_pair ("record enable button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("record enable button: fill active", HSV (0xb50e0eff)));
        c.insert (make_pair ("record enable button: led", HSV (0x7b3541ff)));
        c.insert (make_pair ("record enable button: led active", HSV (0xffa3b3ff)));
        c.insert (make_pair ("record enable button: text", HSV (0xa5a5a5ff)));
        c.insert (make_pair ("record enable button: text active", HSV (0x000000ff)));
        c.insert (make_pair ("generic button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("generic button: fill active", HSV (0xfd0000ff)));
        c.insert (make_pair ("generic button: led", HSV (0x22224fff)));
        c.insert (make_pair ("generic button: led active", HSV (0x2222ffff)));
        c.insert (make_pair ("generic button: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("generic button: text active", HSV (0x191919ff)));
        c.insert (make_pair ("send alert button: fill", HSV (0x4e5647ff)));
        c.insert (make_pair ("send alert button: fill active", HSV (0x85e524ff)));
        c.insert (make_pair ("send alert button: led", HSV (0x00000000)));
        c.insert (make_pair ("send alert button: led active", HSV (0x00000000)));
        c.insert (make_pair ("send alert button: text", HSV (0xccccccff)));
        c.insert (make_pair ("send alert button: text active", HSV (0x000000ff)));
        c.insert (make_pair ("transport button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("transport button: fill active", HSV (0x00a300ff)));
        c.insert (make_pair ("transport button: led", HSV (0x00000000)));
        c.insert (make_pair ("transport button: led active", HSV (0x00000000)));
        c.insert (make_pair ("transport button: text", HSV (0x00000000)));
        c.insert (make_pair ("transport button: text active", HSV (0x00000000)));
        c.insert (make_pair ("transport recenable button: fill", HSV (0x5f3f3fff)));
        c.insert (make_pair ("transport recenable button: fill active", HSV (0xb50e0eff)));
        c.insert (make_pair ("transport recenable button: led", HSV (0x00000000)));
        c.insert (make_pair ("transport recenable button: led active", HSV (0x00000000)));
        c.insert (make_pair ("transport recenable button: text", HSV (0x00000000)));
        c.insert (make_pair ("transport recenable button: text active", HSV (0x00000000)));
        c.insert (make_pair ("transport option button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("transport option button: fill active", HSV (0x4a4b51ff)));
        c.insert (make_pair ("transport option button: led", HSV (0x4f3300ff)));
        c.insert (make_pair ("transport option button: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("transport option button: text", HSV (0xd7d7e8ff)));
        c.insert (make_pair ("transport option button: text active", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("transport active option button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("transport active option button: fill active", HSV (0x00a300ff)));
        c.insert (make_pair ("transport active option button: led", HSV (0x4f3300ff)));
        c.insert (make_pair ("transport active option button: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("transport active option button: text", HSV (0xd7d7e8ff)));
        c.insert (make_pair ("transport active option button: text active", HSV (0x000000ff)));
        c.insert (make_pair ("plugin bypass button: fill", HSV (0x5d5856ff)));
        c.insert (make_pair ("plugin bypass button: fill active", HSV (0x564d48ff)));
        c.insert (make_pair ("plugin bypass button: led", HSV (0x660000ff)));
        c.insert (make_pair ("plugin bypass button: led active", HSV (0xff0000ff)));
        c.insert (make_pair ("plugin bypass button: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("plugin bypass button: text active", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("punch button: fill", HSV (0x603f3fff)));
        c.insert (make_pair ("punch button: fill active", HSV (0xf03020ff)));
        c.insert (make_pair ("punch button: led", HSV (0x00000000)));
        c.insert (make_pair ("punch button: led active", HSV (0x00000000)));
        c.insert (make_pair ("punch button: text", HSV (0xa5a5a5ff)));
        c.insert (make_pair ("punch button: text active", HSV (0xd8d8d8ff)));
        c.insert (make_pair ("mouse mode button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("mouse mode button: fill active", HSV (0x00b200ff)));
        c.insert (make_pair ("mouse mode button: led", HSV (0x4f3300ff)));
        c.insert (make_pair ("mouse mode button: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("mouse mode button: text", HSV (0xd7d7e8ff)));
        c.insert (make_pair ("mouse mode button: text active", HSV (0x000000ff)));
        c.insert (make_pair ("nudge button: fill", HSV (0x684744ff)));
        c.insert (make_pair ("nudge button: fill active", HSV (0x404045ff)));
        c.insert (make_pair ("nudge button: led", HSV (0x4f3300ff)));
        c.insert (make_pair ("nudge button: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("nudge button: text", HSV (0xc7c7d8ff)));
        c.insert (make_pair ("nudge button: text active", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("zoom menu: fill", HSV (0x99997950)));
        c.insert (make_pair ("zoom menu: fill active", HSV (0x404045ff)));
        c.insert (make_pair ("zoom menu: led", HSV (0x4f3300ff)));
        c.insert (make_pair ("zoom menu: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("zoom menu: text", HSV (0xd7d7e8ff)));
        c.insert (make_pair ("zoom menu: text active", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("zoom button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("zoom button: fill active", HSV (0x00a300ff)));
        c.insert (make_pair ("zoom button: led", HSV (0x4f3300ff)));
        c.insert (make_pair ("zoom button: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("zoom button: text", HSV (0xd7d7e8ff)));
        c.insert (make_pair ("zoom button: text active", HSV (0x000000ff)));
        c.insert (make_pair ("route button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("route button: fill active", HSV (0x121212ff)));
        c.insert (make_pair ("route button: led", HSV (0x4f3300ff)));
        c.insert (make_pair ("route button: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("route button: text", HSV (0xd7d7e8ff)));
        c.insert (make_pair ("route button: text active", HSV (0x191919ff)));
        c.insert (make_pair ("mixer strip button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("mixer strip button: fill active", HSV (0xffa500ff)));
        c.insert (make_pair ("mixer strip button: led", HSV (0x4f3300ff)));
        c.insert (make_pair ("mixer strip button: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("mixer strip button: text", HSV (0xd7d7e8ff)));
        c.insert (make_pair ("mixer strip button: text active", HSV (0x000000ff)));
        c.insert (make_pair ("mixer strip name button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("mixer strip name button: fill active", HSV (0x121212ff)));
        c.insert (make_pair ("mixer strip name button: led", HSV (0x4f3300ff)));
        c.insert (make_pair ("mixer strip name button: led active", HSV (0xffa500ff)));
        c.insert (make_pair ("mixer strip name button: text", HSV (0xd7d7e8ff)));
        c.insert (make_pair ("mixer strip name button: text active", HSV (0xc8c8d9ff)));
        c.insert (make_pair ("midi input button: fill", HSV (0x656867ff)));
        c.insert (make_pair ("midi input button: fill active", HSV (0x00a300ff)));
        c.insert (make_pair ("midi input button: led", HSV (0x00000000)));
        c.insert (make_pair ("midi input button: led active", HSV (0x00000000)));
        c.insert (make_pair ("midi input button: text", HSV (0x00000000)));
        c.insert (make_pair ("midi input button: text active", HSV (0x00000000)));
        c.insert (make_pair ("transport clock: background", HSV (0x262626ff)));
        c.insert (make_pair ("transport clock: text", HSV (0x8df823ff)));
        c.insert (make_pair ("transport clock: edited text", HSV (0xffa500ff)));
        c.insert (make_pair ("transport clock: cursor", HSV (0xffa500ff)));
        c.insert (make_pair ("secondary clock: background", HSV (0x262626ff)));
        c.insert (make_pair ("secondary clock: text", HSV (0x8df823ff)));
        c.insert (make_pair ("secondary clock: edited text", HSV (0xffa500ff)));
        c.insert (make_pair ("secondary clock: cursor", HSV (0xffa500ff)));
        c.insert (make_pair ("transport delta clock: background", HSV (0x000000ff)));
        c.insert (make_pair ("transport delta clock: edited text", HSV (0xff0000ff)));
        c.insert (make_pair ("transport delta clock: cursor", HSV (0xf11000ff)));
        c.insert (make_pair ("transport delta clock: text", HSV (0x8ce1f8ff)));
        c.insert (make_pair ("secondary delta clock: edited text", HSV (0xff0000ff)));
        c.insert (make_pair ("secondary delta clock: cursor", HSV (0xf11000ff)));
        c.insert (make_pair ("secondary delta clock: background", HSV (0x000000ff)));
        c.insert (make_pair ("secondary delta clock: text", HSV (0x8ce1f8ff)));
        c.insert (make_pair ("big clock: background", HSV (0x020202ff)));
        c.insert (make_pair ("big clock: text", HSV (0xf0f0f0ff)));
        c.insert (make_pair ("big clock: edited text", HSV (0xffa500ff)));
        c.insert (make_pair ("big clock: cursor", HSV (0xffa500ff)));
        c.insert (make_pair ("big clock active: background", HSV (0x020202ff)));
        c.insert (make_pair ("big clock active: text", HSV (0xf11000ff)));
        c.insert (make_pair ("big clock active: edited text", HSV (0xffa500ff)));
        c.insert (make_pair ("big clock active: cursor", HSV (0xffa500ff)));
        c.insert (make_pair ("punch clock: background", HSV (0x000000ff)));
        c.insert (make_pair ("punch clock: text", HSV (0x6bb620ff)));
        c.insert (make_pair ("punch clock: edited text", HSV (0xff0000ff)));
        c.insert (make_pair ("punch clock: cursor", HSV (0xf11000ff)));
        c.insert (make_pair ("selection clock: background", HSV (0x000000ff)));
        c.insert (make_pair ("selection clock: text", HSV (0x6bb620ff)));
        c.insert (make_pair ("selection clock: edited text", HSV (0xff0000ff)));
        c.insert (make_pair ("selection clock: cursor", HSV (0xf11000ff)));
        c.insert (make_pair ("nudge clock: background", HSV (0x262626ff)));
        c.insert (make_pair ("nudge clock: text", HSV (0x6bb620ff)));
        c.insert (make_pair ("nudge clock: edited text", HSV (0xffa500ff)));
        c.insert (make_pair ("nudge clock: cursor", HSV (0xffa500ff)));
        c.insert (make_pair ("clock: background", HSV (0x000000ff)));
        c.insert (make_pair ("clock: text", HSV (0x6bb620ff)));
        c.insert (make_pair ("clock: edited text", HSV (0xffa500ff)));
        c.insert (make_pair ("clock: cursor", HSV (0xffa500ff)));
        c.insert (make_pair ("lock button: fill", HSV (0x616268ff)));
        c.insert (make_pair ("lock button: fill active", HSV (0x404045ff)));
        c.insert (make_pair ("lock button: led", HSV (0x00000000)));
        c.insert (make_pair ("lock button: led active", HSV (0x00000000)));
        c.insert (make_pair ("lock button: text", HSV (0x000024ff)));
        c.insert (make_pair ("lock button: text active", HSV (0xc8c8d9ff)));

	for (map<string,HSV>::iterator fp = c.begin(); fp != c.end(); ++fp) {
		const double hue_cnt = 18.0;

		fp->second.h = hue_cnt * (round (fp->second.h/hue_cnt));
	}

#undef CANVAS_COLOR
#define CANVAS_COLOR(var,name,base,modifier) print_relative_def (#var,name,c[name]);
#include "colors.h"
#undef CANVAS_COLOR	
}

void
UIConfiguration::color_theme_changed ()
{
	map<std::string,RelativeHSV>::iterator current_color;

	/* we need to reset the quantized hues before we start, because
	 * otherwise when we call RelativeHSV::get() in color_compute()
	 * we don't get an answer based on the new base colors, but instead
	 * based on any existing hue quantization.
	 */

	for (current_color = relative_colors.begin(); current_color != relative_colors.end(); ++current_color) {
		current_color->second.quantized_hue = -1;
	}

	color_compute ();
}

void
UIConfiguration::map_parameters (boost::function<void (std::string)>& functor)
{
#undef  UI_CONFIG_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,Name,value) functor (Name);
#include "ui_config_vars.h"
#undef  UI_CONFIG_VARIABLE
}

int
UIConfiguration::load_defaults ()
{
	int found = 0;
        std::string rcfile;

	if (find_file (ardour_config_search_path(), default_ui_config_file_name, rcfile) ) {
		XMLTree tree;
		found = 1;

		info << string_compose (_("Loading default ui configuration file %1"), rcfile) << endl;

		if (!tree.read (rcfile.c_str())) {
			error << string_compose(_("cannot read default ui configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root(), Stateful::loading_state_version)) {
			error << string_compose(_("default ui configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}

		_dirty = false;
	}

	return found;
}

int
UIConfiguration::load_state ()
{
	bool found = false;

	std::string rcfile;

	if ( find_file (ardour_config_search_path(), default_ui_config_file_name, rcfile)) {
		XMLTree tree;
		found = true;

		info << string_compose (_("Loading default ui configuration file %1"), rcfile) << endl;

		if (!tree.read (rcfile.c_str())) {
			error << string_compose(_("cannot read default ui configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root(), Stateful::loading_state_version)) {
			error << string_compose(_("default ui configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}
	}

	if (find_file (ardour_config_search_path(), ui_config_file_name, rcfile)) {
		XMLTree tree;
		found = true;

		info << string_compose (_("Loading user ui configuration file %1"), rcfile) << endmsg;

		if (!tree.read (rcfile)) {
			error << string_compose(_("cannot read ui configuration file \"%1\""), rcfile) << endmsg;
			return -1;
		}

		if (set_state (*tree.root(), Stateful::loading_state_version)) {
			error << string_compose(_("user ui configuration file \"%1\" not loaded successfully."), rcfile) << endmsg;
			return -1;
		}

		_dirty = false;
	}

	if (!found) {
		error << _("could not find any ui configuration file, canvas will look broken.") << endmsg;
	}

	return 0;
}

int
UIConfiguration::save_state()
{
	XMLTree tree;

	std::string rcfile(user_config_directory());
	rcfile = Glib::build_filename (rcfile, ui_config_file_name);

	// this test seems bogus?
	if (rcfile.length()) {
		tree.set_root (&get_state());
		if (!tree.write (rcfile.c_str())){
			error << string_compose (_("Config file %1 not saved"), rcfile) << endmsg;
			return -1;
		}
	}

	_dirty = false;

	return 0;
}

XMLNode&
UIConfiguration::get_state ()
{
	XMLNode* root;
	LocaleGuard lg (X_("POSIX"));

	root = new XMLNode("Ardour");

	root->add_child_nocopy (get_variables ("UI"));
	root->add_child_nocopy (get_variables ("Canvas"));

	if (_extra_xml) {
		root->add_child_copy (*_extra_xml);
	}

	return *root;
}

XMLNode&
UIConfiguration::get_variables (std::string which_node)
{
	XMLNode* node;
	LocaleGuard lg (X_("POSIX"));

	node = new XMLNode (which_node);

#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_STRING_VARIABLE
#undef  CANVAS_FONT_VARIABLE
#undef  CANVAS_BASE_COLOR
#define UI_CONFIG_VARIABLE(Type,var,Name,value) if (node->name() == "UI") { var.add_to_node (*node); }
#define CANVAS_STRING_VARIABLE(var,Name) if (node->name() == "Canvas") { var.add_to_node (*node); }
#define CANVAS_FONT_VARIABLE(var,Name) if (node->name() == "Canvas") { var.add_to_node (*node); }
#define CANVAS_BASE_COLOR(var,Name,val) if (node->name() == "Canvas") { var.add_to_node (*node); }
#include "ui_config_vars.h"
#include "canvas_vars.h"
#include "base_colors.h"
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_STRING_VARIABLE
#undef  CANVAS_FONT_VARIABLE
#undef  CANVAS_BASE_COLOR

	return *node;
}

int
UIConfiguration::set_state (const XMLNode& root, int /*version*/)
{
	if (root.name() != "Ardour") {
		return -1;
	}

	Stateful::save_extra_xml (root);

	XMLNodeList nlist = root.children();
	XMLNodeConstIterator niter;
	XMLNode *node;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		node = *niter;

		if (node->name() == "Canvas" ||  node->name() == "UI") {
			set_variables (*node);

		}
	}

	return 0;
}


void
UIConfiguration::set_variables (const XMLNode& node)
{
#undef  UI_CONFIG_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,name,val) \
         if (var.set_from_node (node)) { \
		 ParameterChanged (name); \
		 }
#define CANVAS_STRING_VARIABLE(var,name)	\
         if (var.set_from_node (node)) { \
		 ParameterChanged (name); \
		 }
#define CANVAS_FONT_VARIABLE(var,name)	\
         if (var.set_from_node (node)) { \
		 ParameterChanged (name); \
		 }
#include "ui_config_vars.h"
#include "canvas_vars.h"
#undef  UI_CONFIG_VARIABLE
#undef  CANVAS_STRING_VARIABLE
#undef  CANVAS_FONT_VARIABLE

	/* Reset base colors */

#undef  CANVAS_BASE_COLOR
#define CANVAS_BASE_COLOR(var,name,val) \
	var.set_from_node (node);
#include "base_colors.h"
#undef CANVAS_BASE_COLOR	

}

void
UIConfiguration::set_dirty ()
{
	_dirty = true;
}

bool
UIConfiguration::dirty () const
{
	return _dirty;
}

ArdourCanvas::Color
UIConfiguration::base_color_by_name (const std::string& name) const
{
	map<std::string,ColorVariable<Color>* >::const_iterator i = configurable_colors.find (name);

	if (i != configurable_colors.end()) {
		return i->second->get();
	}

#if 0 // yet unsed experimental style postfix
	/* Idea: use identical colors but different font/sizes
	 * for variants of the same 'widget'.
	 *
	 * example:
	 *  set_name("mute button");  // in route_ui.cc
	 *  set_name("mute button small"); // in mixer_strip.cc
	 *
	 * ardour3_widget_list.rc:
	 *  widget "*mute button" style:highest "small_button"
	 *  widget "*mute button small" style:highest "very_small_text"
	 *
	 * both use color-schema of defined in
	 *   BUTTON_VARS(MuteButton, "mute button")
	 *
	 * (in this particular example the widgets should be packed
	 * vertically shinking the mixer strip ones are currently not)
	 */
	const size_t name_len = name.size();
	const size_t name_sep = name.find(':');
	for (i = configurable_colors.begin(); i != configurable_colors.end(), name_sep != string::npos; ++i) {
		const size_t cmp_len = i->first.size();
		const size_t cmp_sep = i->first.find(':');
		if (cmp_len >= name_len || cmp_sep == string::npos) continue;
		if (name.substr(name_sep) != i->first.substr(cmp_sep)) continue;
		if (name.substr(0, cmp_sep) != i->first.substr(0, cmp_sep)) continue;
		return i->second->get();
	}
#endif

	cerr << string_compose (_("Color %1 not found"), name) << endl;
	return RGBA_TO_UINT (g_random_int()%256,g_random_int()%256,g_random_int()%256,0xff);
}

ArdourCanvas::Color
UIConfiguration::color (const std::string& name) const
{
	map<string,string>::const_iterator e = color_aliases.find (name);

	if (e != color_aliases.end ()) {
		map<string,HSV>::const_iterator ac = actual_colors.find (e->second);
		if (ac != actual_colors.end()) {
			return ac->second;
		}
	} 

	cerr << string_compose (_("Color %1 not found"), name) << endl;

	return rgba_to_color ((g_random_int()%256)/255.0,
			      (g_random_int()%256)/255.0,
			      (g_random_int()%256)/255.0,
			      0xff);
}

ArdourCanvas::HSV
UIConfiguration::RelativeHSV::get() const
{
	HSV base (UIConfiguration::instance()->base_color_by_name (base_color));
	
	/* this operation is a little wierd. because of the way we originally
	 * computed the alpha specification for the modifiers used here
	 * we need to reset base's alpha to zero before adding the modifier.
	 */

	base.a = 0.0;

	HSV self (base + modifier);
	
	if (quantized_hue >= 0.0) {
		self.h = quantized_hue;
	}
	
	return self;
}

void
UIConfiguration::color_compute ()
{
	using namespace ArdourCanvas;

	map<std::string,ColorVariable<Color>* >::iterator f;
	map<std::string,HSV*>::iterator v;

	/* now compute distances */

	cerr << "Attempt to reduce " << relative_colors.size() << endl;

	map<std::string,RelativeHSV>::iterator current_color;
	
	color_aliases.clear ();
	
	actual_colors.clear ();

	for (current_color = relative_colors.begin(); current_color != relative_colors.end(); ++current_color) {

		map<std::string,HSV>::iterator possible_match;
		std::string equivalent_name;
		bool matched;

		matched = false;

		for (possible_match = actual_colors.begin(); possible_match != actual_colors.end(); ++possible_match) {

			HSV a (current_color->second.get());
			HSV b (possible_match->second);
		
			/* This uses perceptual distance to find visually
			 * similar colors.
			 */

			if (a.distance (b) < 6.0) {
				matched = true;
				break;
			}
		}

		if (!matched) {

			/* color does not match any other, generate a generic
			 * name and store two aliases.
			 */

			string alias = string_compose ("color %1", actual_colors.size() + 1);
			//cerr << alias << " == " << current_color->second.base_color 
			// << " [ " << HSV (base_color_by_name (current_color->second.base_color)) << "] + " 
			// << current_color->second.modifier << endl;
			actual_colors.insert (make_pair (alias, current_color->second.get()));
			color_aliases.insert (make_pair (current_color->first, alias));

		} else {

			/* this color was within the JND CIE76 distance of
			 * another, so throw it away.
			 */
			
			color_aliases.insert (make_pair (current_color->first, possible_match->first));
		}
	}

	cerr << "Ended with " << actual_colors.size() << " colors" << endl;
}
