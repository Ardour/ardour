/*
 * Copyright (C) 2025 Robin Gareus <robin@gareus.org>
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

#include "pbd/compose.h"
#include "pbd/unwind.h"

#include "ardour/session.h"
#include "ardour/session_configuration.h"

#include "gtkmm2ext/utils.h"

#include "gui_thread.h"
#include "option_editor.h"
#include "route_comment_editor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;

RouteCommentEditor::RouteCommentEditor ()
	: ArdourWindow ("")
	, _bo (nullptr)
	, _ignore_change (false)
{
	const float scale = std::max (1.f, UIConfiguration::instance ().get_ui_scale ());
	set_default_size (400 * scale, 200 * scale);

	set_skip_taskbar_hint (true);

	_comment_area.set_name ("MixerTrackCommentArea");
	_comment_area.set_wrap_mode (WRAP_WORD);
	_comment_area.set_editable (true);

	add (_vbox);

	signal_hide ().connect (sigc::mem_fun (*this, &RouteCommentEditor::commit_change));
	_comment_area.get_buffer ()->signal_changed ().connect (sigc::mem_fun (*this, &RouteCommentEditor::commit_change));
}

RouteCommentEditor::~RouteCommentEditor ()
{
	reset ();
}

void
RouteCommentEditor::reset ()
{
	hide ();
	delete _bo;
	_bo = nullptr;
	if (_route && _route->comment_editor () == this) {
		_route->set_comment_editor (nullptr);
	}
	_route.reset ();
	_connections.drop_connections ();
}

void
RouteCommentEditor::toggle (std::shared_ptr<ARDOUR::Route> r)
{
	if (r && r->comment_editor ()) {
		ArdourWindow* self = r->comment_editor ();
		if (self->get_visible ()) {
			self->hide ();
		} else {
			self->present ();
		}
		return;
	}
	open (r);
}

void
RouteCommentEditor::open (std::shared_ptr<ARDOUR::Route> r)
{
	if (r && r->comment_editor ()) {
		r->comment_editor ()->present ();
		return;
	}
	if (_route == r) {
		present ();
		return;
	}
	if (!r) {
		assert (0);
		reset ();
		return;
	}
	_route = r;
	_route->set_comment_editor (this);
	_route->comment_changed.connect (_connections, invalidator (*this), std::bind (&RouteCommentEditor::comment_changed, this), gui_context ());
	_route->DropReferences.connect (_connections, invalidator (*this), std::bind (&RouteCommentEditor::reset, this), gui_context ());

	set_title (string_compose ("%1: %2", _route->name (), _("Comment Editor")));
	_comment_area.get_buffer ()->set_text (_route->comment ());

	Gtkmm2ext::container_clear (_vbox, false);
	_vbox.pack_start (_comment_area);

	delete _bo;
	if (_route->is_master ()) {
		ARDOUR::Session* session = &_route->session ();
		_bo                      = new BoolOption (
                    "show-master-bus-comment-on-load",
                    _ ("Show this comment on next session load"),
                    sigc::mem_fun (session->config, &SessionConfiguration::get_show_master_bus_comment_on_load),
                    sigc::mem_fun (session->config, &SessionConfiguration::set_show_master_bus_comment_on_load));

		_vbox.pack_start (_bo->tip_widget (), false, false, 4);
		_bo->tip_widget ().show_all ();
		_bo->parameter_changed ("show-master-bus-comment-on-load");
		session->config.ParameterChanged.connect (_connections, invalidator (*this), std::bind (&BoolOption::parameter_changed, _bo, _1), gui_context ());
	} else {
		_bo = nullptr;
	}

	_vbox.show_all ();
	set_position (Gtk::WIN_POS_CENTER_ON_PARENT);
	present ();
}

void
RouteCommentEditor::comment_changed ()
{
	if (_ignore_change) {
		return;
	}
	_comment_area.get_buffer ()->set_text (_route->comment ());
}

void
RouteCommentEditor::commit_change ()
{
	if (!_route) {
		return;
	}

	std::string const str = _comment_area.get_buffer ()->get_text ();
	if (str != _route->comment ()) {
		PBD::Unwinder<bool> uw (_ignore_change, true);
		_route->set_comment (str, this);
	}
}
