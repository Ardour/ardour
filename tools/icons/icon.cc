/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <unistd.h>

#include <cairo/cairo.h>

#include "pbd/xml++.h"
#include "gtkmm2ext/colors.h"
#include "widgets/ardour_icon.h"

using namespace ArdourWidgets;

static int  wh   = 64;
static int  sq   = 1;
static bool grid = false;

static uint32_t bg_color = 0x3d3d3dff; // gtk_background
static uint32_t fg_color = 0xeeeeecff; // gtk_foreground

static XMLNode*
find_named_node (const XMLNode& node, std::string name)
{
	XMLNodeList nlist = node.children ();
	for (XMLNodeConstIterator niter = nlist.begin (); niter != nlist.end (); ++niter) {
		XMLNode* child = *niter;
		if (child->name () == name) {
			return child;
		}
	}
	return 0;
}

static std::string
find_color_alias (const XMLNode& node, std::string colorname)
{
	XMLNodeList nlist = node.children ();
	for (XMLNodeConstIterator niter = nlist.begin (); niter != nlist.end (); ++niter) {
		XMLNode* child = *niter;
		if (child->name () != "ColorAlias") {
			continue;
		}
		XMLProperty const* name  = child->property ("name");
		XMLProperty const* alias = child->property ("alias");
		if (!name || !alias) {
			continue;
		}
		if (name->value () == colorname) {
			return alias->value ();
		}
	}
	return "";
}

static uint32_t
lookup_aliased_color (const XMLNode& node, std::string aliasname)
{
	XMLNodeList nlist = node.children ();
	for (XMLNodeConstIterator niter = nlist.begin (); niter != nlist.end (); ++niter) {
		XMLNode* child = *niter;
		if (child->name () != "Color") {
			continue;
		}
		XMLProperty const* name  = child->property ("name");
		XMLProperty const* color = child->property ("value");
		if (!name || !color) {
			continue;
		}
		if (name->value () == aliasname) {
			return strtoul (color->value ().c_str (), 0, 16);
		}
	}
	return 0;
}

static bool
load_colors (const char* path)
{
	XMLTree tree;
	if (!tree.read (path)) {
		return false;
	}
	XMLNode* colors  = find_named_node (*tree.root (), "Colors");
	XMLNode* aliases = find_named_node (*tree.root (), "ColorAliases");

	if (!colors || !aliases) {
		return false;
	}

	bg_color = lookup_aliased_color (*colors, find_color_alias (*aliases, "gtk_background"));
	fg_color = lookup_aliased_color (*colors, find_color_alias (*aliases, "gtk_foreground"));

	printf ("Theme colors bg:0x%x fg:0x%x\n", bg_color, fg_color);
	return true;
}

static void
draw_icon (cairo_t* cr, int pos, const enum ArdourIcon::Icon icon, const Gtkmm2ext::ActiveState state)
{
	int col = pos % sq;
	int row = pos / sq;
	cairo_save (cr);
	cairo_translate (cr, col * wh, row * wh);
	if (grid) {
		cairo_rectangle (cr, .5, .5, wh - 1, wh - 1);
		cairo_set_line_width (cr, 1);
		cairo_set_source_rgba (cr, 0, 0, 0, 1.0);
		cairo_stroke (cr);
		cairo_move_to (cr, wh * .5, 0);
		cairo_line_to (cr, wh * .5, wh);
		cairo_move_to (cr, 0, wh * .5);
		cairo_line_to (cr, wh, wh * .5);
		cairo_stroke (cr);
	}
	ArdourIcon::render (cr, icon, wh, wh, state, fg_color);
	cairo_restore (cr);
}

int
main (int argc, char** argv)
{
	const char* fn = "/tmp/ardour_icons.png";

	int c = 0;
	while (EOF != (c = getopt (argc, argv, "go:s:t:"))) {
		switch (c) {
			case 'g':
				grid = true;
				break;
			case 't':
				if (!load_colors (optarg)) {
					std::cerr << "Error: failed to load color theme.\n";
					::exit (EXIT_FAILURE);
				}
				break;
			case 'o':
				fn = optarg;
				break;
			case 's':
				wh = atoi (optarg);
				break;
			default:
				std::cerr << "Error: unrecognized option.\n";
				::exit (EXIT_FAILURE);
				break;
		}
	}

	if (optind < argc) {
		std::cerr << "Error: Extra commandline argument.\n";
		::exit (EXIT_FAILURE);
	}

	if (wh <= 0 || wh > 256) {
		wh = 64;
	}

	sq = ceil (sqrt (ArdourIcon::NoIcon + 3));

	cairo_surface_t* cs = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, wh * sq, wh * sq);
	cairo_t*         cr = cairo_create (cs);

	Gtkmm2ext::set_source_rgba (cr, bg_color);
	cairo_paint (cr);

	int pos = 0;

	draw_icon (cr, pos++, ArdourIcon::RecButton, Gtkmm2ext::Off);
	draw_icon (cr, pos++, ArdourIcon::RecTapeMode, Gtkmm2ext::Off);
	draw_icon (cr, pos++, ArdourIcon::RecButton, Gtkmm2ext::ImplicitActive);
	draw_icon (cr, pos++, ArdourIcon::RecTapeMode, Gtkmm2ext::ImplicitActive);

	for (int i = 0; i < ArdourIcon::NoIcon; ++i) {
		draw_icon (cr, pos++, ArdourIcon::Icon (i), Gtkmm2ext::ExplicitActive);
	}

	if (CAIRO_STATUS_SUCCESS != cairo_surface_write_to_png (cs, fn)) {
		std::cerr << "Error: Failed to write to '" << fn << "'.\n";
		::exit (EXIT_FAILURE);
	}
	cairo_destroy (cr);
	cairo_surface_destroy (cs);
	return 0;
}
