#include <cmath>
#include <iostream>
#include <fstream>
#include <errno.h>

#include <gtkmm/stock.h>

#include "color_manager.h"
#include "rgb_macros.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace PBD;

/* the global color map */

ColorMap color_map;

/* lookup table of color IDs as strings */

#undef COLORID
#define COLORID(s) #s,
static const char *color_id_strs[] = {
	#include "colors.h"
};
#undef COLORID

/* global color change signals */

sigc::signal<void> ColorsChanged;
sigc::signal<void,ColorID,uint32_t> ColorChanged;

ColorManager::ColorManager()
	: ArdourDialog ("ColorManager")
{
	color_list = ListStore::create (columns);
	color_display.set_model (color_list);
	color_display.append_column (_("Object"), columns.name);
	color_display.append_column (_("Color"), columns.color);
	color_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));	
	color_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));	
	color_display.set_reorderable (false);
	color_display.get_selection()->set_mode (SELECTION_NONE);
	color_display.set_headers_visible (true);

	CellRenderer* color_cell = color_display.get_column_cell_renderer (1);
	TreeViewColumn* color_column = color_display.get_column (1);
	color_column->add_attribute (color_cell->property_cell_background_gdk(), columns.gdkcolor);
	
	scroller.add (color_display);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	get_vbox()->pack_start (scroller);

	color_display.signal_button_press_event().connect (mem_fun (*this, &ColorManager::button_press_event), false);

	color_dialog.get_colorsel()->set_has_opacity_control (true);
	color_dialog.get_colorsel()->set_has_palette (true);

	color_dialog.get_ok_button()->signal_clicked().connect (bind (mem_fun (color_dialog, &Gtk::Dialog::response), RESPONSE_ACCEPT));
	color_dialog.get_cancel_button()->signal_clicked().connect (bind (mem_fun (color_dialog, &Gtk::Dialog::response), RESPONSE_CANCEL));

	set_size_request (-1, 400);
}

ColorManager::~ColorManager()
{
}

int
ColorManager::load (string path)
{
	ifstream in (path.c_str());

	if (!in) {
		error << string_compose (_("cannot open color definition file %1: %2"), path, strerror(errno)) << endmsg;
		return -1;
	}

	while (in) {
		string name;
		double r, g, b, a;

		in >> name; if (!in) break;
		in >> r; if (!in) break;
		in >> g; if (!in) break;
		in >> b; if (!in) break;
		in >> a; if (!in) break;

		for (uint32_t i = 0; i < sizeof (color_id_strs)/sizeof(color_id_strs[0]); ++i) {
			if (name == color_id_strs[i]) {

				/* set color map */

				int ir,ig,ib,ia;
				int rgba;

				ir = (int) floor (r * 255.0);
				ig = (int) floor (g * 255.0);
				ib = (int) floor (b * 255.0);
				ia = (int) floor (a * 255.0);
				rgba = RGBA_TO_UINT (ir, ig, ib, ia);

				color_map[(ColorID)i] = rgba;

				/* set up list entry */

				Gdk::Color col;
				col.set_rgb_p (r,g,b);

				TreeModel::Row row = *(color_list->append());

				/* all the color names are prefixed by 'c' to avoid
				   naming collisions when used as enums. trim
				   this leading character from the displayed
				   value.
				*/

				row[columns.name] = name.substr (1);
				row[columns.color] = "";
				row[columns.id] = (ColorID) i;
				row[columns.gdkcolor] = col;
				row[columns.rgba] = rgba;

				break;
			}
		}
	}
	
	ColorsChanged(); /* emit signal */
	return 0;
}

int
ColorManager::save (string path)
{
	return 0;
}

bool
ColorManager::button_press_event (GdkEventButton* ev)
{
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;
	
	if (!color_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 0:
		/* allow normal processing to occur */
		return false;

	case 1: /* color */
		if ((iter = color_list->get_iter (path))) {

			ColorID edit_color_id = (*iter)[columns.id];
			int r,g, b, a;
			uint32_t rgba;
			Gdk::Color color;

			ResponseType result = (ResponseType) color_dialog.run();

			switch (result) {
			case RESPONSE_CANCEL:
				break;
			case RESPONSE_ACCEPT:
				color = color_dialog.get_colorsel()->get_current_color(); 
				a = color_dialog.get_colorsel()->get_current_alpha();
				r = (int) floor (color.get_red_p() * 255.0);
				g = (int) floor (color.get_green_p() * 255.0);
				b = (int) floor (color.get_blue_p() * 255.0);

				rgba = RGBA_TO_UINT(r,g,b,a);
				
				(*iter)[columns.rgba] = rgba;
				(*iter)[columns.gdkcolor] = color;

				color_map[edit_color_id] = rgba;

				ColorChanged (edit_color_id, rgba);
				break;

			default:
				break;

			}

			color_dialog.hide ();
		}
		return true;

	default:
		break;
	}

	return false;
}
