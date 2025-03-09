/*
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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

#include <cassert>
#include <ytkmm/widget.h>

#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "widgets/frame.h"

#include "ardour/region.h"
#include "ardour/region_fx_plugin.h"
#include "plugin_ui.h"
#include "region_fx_properties_box.h"
#include "timers.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;

RegionFxPropertiesBox::RegionFxPropertiesBox (std::shared_ptr<ARDOUR::Region> r)
	: _region (r)
	, _idle_redisplay_plugins_id (-1)
{
	_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_NEVER);
	_scroller.set_shadow_type(Gtk::SHADOW_NONE);
	_scroller.set_border_width(0);
	_scroller.add (_box);

	_box.set_spacing (4);

	pack_start (_scroller, true, true);
	show_all();

	/* remove shadow from scrollWindow's viewport */
	Gtk::Viewport* viewport = (Gtk::Viewport*) _scroller.get_child();
	viewport->set_shadow_type(Gtk::SHADOW_NONE);
	viewport->set_border_width(0);

	_region->RegionFxChanged.connect (_region_connection, invalidator (*this), std::bind (&RegionFxPropertiesBox::idle_redisplay_plugins, this), gui_context ());

	redisplay_plugins ();
}

RegionFxPropertiesBox::~RegionFxPropertiesBox ()
{
	drop_plugin_uis ();

	if (_idle_redisplay_plugins_id >= 0) {
		g_source_destroy (g_main_context_find_source_by_id (NULL, _idle_redisplay_plugins_id));
		_idle_redisplay_plugins_id = -1;
	}
}

void
RegionFxPropertiesBox::drop_plugin_uis ()
{
	std::list<Gtk::Widget*> children = _box.get_children ();
	for (auto const& child : children) {
		child->hide ();
		_box.remove (*child);
		delete child;
	}

	for (auto const& ui : _proc_uis) {
		ui->stop_updating (0);
		delete ui;
	}

	_processor_connections.drop_connections ();
	_proc_uis.clear ();
}

void
RegionFxPropertiesBox::add_fx_to_display (std::weak_ptr<RegionFxPlugin> wfx)
{
	std::shared_ptr<RegionFxPlugin> fx (wfx.lock ());
	if (!fx || !fx->plugin ()) {
		return;
	}

	GenericPluginUI* plugin_ui = new GenericPluginUI (fx, true, true);
	if (plugin_ui->empty ()) {
		delete plugin_ui;
		return;
	}
	_proc_uis.push_back (plugin_ui);

	ArdourWidgets::Frame* frame = new ArdourWidgets::Frame ();
	frame->set_label (fx->name ());
	frame->add (*plugin_ui);
	frame->set_padding (0);
	frame->set_edge_color (0x000000ff); // black (0);
	_box.pack_start (*frame, false, false);
	plugin_ui->show ();
}

int
RegionFxPropertiesBox::_idle_redisplay_processors (gpointer arg)
{
	static_cast<RegionFxPropertiesBox*>(arg)->redisplay_plugins ();
	return 0;
}

void
RegionFxPropertiesBox::idle_redisplay_plugins ()
{
	if (_idle_redisplay_plugins_id < 0) {
		_idle_redisplay_plugins_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, _idle_redisplay_processors, this, NULL);
	}
}

void
RegionFxPropertiesBox::redisplay_plugins ()
{
	drop_plugin_uis ();

	_region->foreach_plugin (sigc::mem_fun (*this, &RegionFxPropertiesBox::add_fx_to_display));

	if (_proc_uis.empty ()) {
		_scroller.hide ();
	} else {
		float ui_scale = std::max<float> (1.f, UIConfiguration::instance().get_ui_scale());
		int h = 100 * ui_scale;
		for (auto const& ui : _proc_uis) {
			h = std::max<int> (h, ui->get_preferred_height () + /* frame label */ 30 * ui_scale);
		}
		h = std::min<int> (h, 300 * ui_scale);
		_box.set_size_request (-1, h);
		_scroller.show_all ();
	}
	_idle_redisplay_plugins_id = -1;
}
