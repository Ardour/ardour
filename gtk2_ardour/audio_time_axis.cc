/*
    Copyright (C) 2000 Paul Davis 

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

    $Id$
*/

#include <cstdlib>
#include <cmath>
#include <cassert>

#include <algorithm>
#include <string>
#include <vector>

#include <sigc++/bind.h>

#include <pbd/error.h>
#include <pbd/stl_delete.h>
#include <pbd/whitespace.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/utils.h>

#include <ardour/audioplaylist.h>
#include <ardour/audio_diskstream.h>
#include <ardour/insert.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/location.h>
#include <ardour/panner.h>
#include <ardour/playlist.h>
#include <ardour/session.h>
#include <ardour/session_playlist.h>
#include <ardour/utils.h>

#include "ardour_ui.h"
#include "audio_time_axis.h"
#include "automation_gain_line.h"
#include "automation_pan_line.h"
#include "automation_time_axis.h"
#include "canvas_impl.h"
#include "crossfade_view.h"
#include "enums.h"
#include "gain_automation_time_axis.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "pan_automation_time_axis.h"
#include "playlist_selector.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "point_selection.h"
#include "prompter.h"
#include "public_editor.h"
#include "redirect_automation_line.h"
#include "redirect_automation_time_axis.h"
#include "audio_regionview.h"
#include "rgb_macros.h"
#include "selection.h"
#include "simplerect.h"
#include "audio_streamview.h"
#include "utils.h"

#include <ardour/audio_track.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Editing;


AudioTimeAxisView::AudioTimeAxisView (PublicEditor& ed, Session& sess, boost::shared_ptr<Route> rt, Canvas& canvas)
	: AxisView(sess), // FIXME: won't compile without this, why??
	RouteTimeAxisView(ed, sess, rt, canvas)
{
	subplugin_menu.set_name ("ArdourContextMenu");
	gain_track = 0;
	pan_track = 0;
	waveform_item = 0;
	pan_automation_item = 0;
	gain_automation_item = 0;

	_view = new AudioStreamView (*this);

	add_gain_automation_child ();
	add_pan_automation_child ();

	ignore_toggle = false;

	mute_button->set_active (false);
	solo_button->set_active (false);
	
	if (is_audio_track())
		controls_ebox.set_name ("AudioTimeAxisViewControlsBaseUnselected");
	else // bus
		controls_ebox.set_name ("AudioBusControlsBaseUnselected");

	/* map current state of the route */

	redirects_changed (0);
	reset_redirect_automation_curves ();

	ensure_xml_node ();

	set_state (*xml_node);
	
	_route->panner().Changed.connect (mem_fun(*this, &AudioTimeAxisView::update_pans));

	if (is_audio_track()) {

		controls_ebox.set_name ("AudioTrackControlsBaseUnselected");
		controls_base_selected_name = "AudioTrackControlsBaseSelected";
		controls_base_unselected_name = "AudioTrackControlsBaseUnselected";

		/* ask for notifications of any new RegionViews */
		_view->RegionViewAdded.connect (mem_fun(*this, &AudioTimeAxisView::region_view_added));
		_view->attach ();

	} else { /* bus */

		controls_ebox.set_name ("AudioBusControlsBaseUnselected");
		controls_base_selected_name = "AudioBusControlsBaseSelected";
		controls_base_unselected_name = "AudioBusControlsBaseUnselected";
	}
}

AudioTimeAxisView::~AudioTimeAxisView ()
{
	vector_delete (&redirect_automation_curves);

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		delete *i;
	}
}

AudioStreamView*
AudioTimeAxisView::audio_view()
{
	return dynamic_cast<AudioStreamView*>(_view);
}

guint32
AudioTimeAxisView::show_at (double y, int& nth, Gtk::VBox *parent)
{
	ensure_xml_node ();
	xml_node->add_property ("shown_editor", "yes");
		
	return TimeAxisView::show_at (y, nth, parent);
}

void
AudioTimeAxisView::hide ()
{
	ensure_xml_node ();
	xml_node->add_property ("shown_editor", "no");

	TimeAxisView::hide ();
}

void
AudioTimeAxisView::set_state (const XMLNode& node)
{
	const XMLProperty *prop;
	
	TimeAxisView::set_state (node);
	
	if ((prop = node.property ("shown_editor")) != 0) {
		if (prop->value() == "no") {
			_marked_for_display = false;
		} else {
			_marked_for_display = true;
		}
	} else {
		_marked_for_display = true;
	}
	
	XMLNodeList nlist = node.children();
	XMLNodeConstIterator niter;
	XMLNode *child_node;
	
	
	show_gain_automation = false;
	show_pan_automation  = false;
	
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		child_node = *niter;

		if (child_node->name() == "gain") {
			XMLProperty *prop=child_node->property ("shown");
			
			if (prop != 0) {
				if (prop->value() == "yes") {
					show_gain_automation = true;
				}
			}
			continue;
		}
		
		if (child_node->name() == "pan") {
			XMLProperty *prop=child_node->property ("shown");
			
			if (prop != 0) {
				if (prop->value() == "yes") {
					show_pan_automation = true;
				}			
			}
			continue;
		}
	}
}

void
AudioTimeAxisView::reset_redirect_automation_curves ()
{
	for (vector<RedirectAutomationLine*>::iterator i = redirect_automation_curves.begin(); i != redirect_automation_curves.end(); ++i) {
		(*i)->reset();
	}
}

void
AudioTimeAxisView::build_display_menu ()
{
	using namespace Menu_Helpers;

	/* get the size menu ready */

	build_size_menu ();

	/* prepare it */

	TimeAxisView::build_display_menu ();

	/* now fill it with our stuff */

	MenuList& items = display_menu->items();
	display_menu->set_name ("ArdourContextMenu");
	
	items.push_back (MenuElem (_("Height"), *size_menu));
	items.push_back (MenuElem (_("Color"), mem_fun(*this, &AudioTimeAxisView::select_track_color)));


	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Hide all crossfades"), mem_fun(*this, &AudioTimeAxisView::hide_all_xfades)));
	items.push_back (MenuElem (_("Show all crossfades"), mem_fun(*this, &AudioTimeAxisView::show_all_xfades)));
	items.push_back (SeparatorElem());

	build_remote_control_menu ();
	items.push_back (MenuElem (_("Remote Control ID"), *remote_control_menu));

	automation_action_menu = manage (new Menu);
	MenuList& automation_items = automation_action_menu->items();
	automation_action_menu->set_name ("ArdourContextMenu");
	
	automation_items.push_back (MenuElem (_("Show all automation"),
					      mem_fun(*this, &AudioTimeAxisView::show_all_automation)));

	automation_items.push_back (MenuElem (_("Show existing automation"),
					      mem_fun(*this, &AudioTimeAxisView::show_existing_automation)));

	automation_items.push_back (MenuElem (_("Hide all automation"),
					      mem_fun(*this, &AudioTimeAxisView::hide_all_automation)));

	automation_items.push_back (SeparatorElem());

	automation_items.push_back (CheckMenuElem (_("Fader"), 
						   mem_fun(*this, &AudioTimeAxisView::toggle_gain_track)));
	gain_automation_item = static_cast<CheckMenuItem*> (&automation_items.back());
	gain_automation_item->set_active(show_gain_automation);

	automation_items.push_back (CheckMenuElem (_("Pan"),
						   mem_fun(*this, &AudioTimeAxisView::toggle_pan_track)));
	pan_automation_item = static_cast<CheckMenuItem*> (&automation_items.back());
	pan_automation_item->set_active(show_pan_automation);

	automation_items.push_back (MenuElem (_("Plugins"), subplugin_menu));

	items.push_back (MenuElem (_("Automation"), *automation_action_menu));

	Menu *waveform_menu = manage(new Menu);
	MenuList& waveform_items = waveform_menu->items();
	waveform_menu->set_name ("ArdourContextMenu");
	
	waveform_items.push_back (CheckMenuElem (_("Show waveforms"), mem_fun(*this, &AudioTimeAxisView::toggle_waveforms)));
	waveform_item = static_cast<CheckMenuItem *> (&waveform_items.back());
	ignore_toggle = true;
	waveform_item->set_active (editor.show_waveforms());
	ignore_toggle = false;

	RadioMenuItem::Group group;

	waveform_items.push_back (RadioMenuElem (group, _("Traditional"), bind (mem_fun(*this, &AudioTimeAxisView::set_waveform_shape), Traditional)));
	traditional_item = static_cast<RadioMenuItem *> (&waveform_items.back());

	waveform_items.push_back (RadioMenuElem (group, _("Rectified"), bind (mem_fun(*this, &AudioTimeAxisView::set_waveform_shape), Rectified)));
	rectified_item = static_cast<RadioMenuItem *> (&waveform_items.back());

	items.push_back (MenuElem (_("Waveform"), *waveform_menu));

	if (is_audio_track()) {

		Menu* alignment_menu = manage (new Menu);
		MenuList& alignment_items = alignment_menu->items();
		alignment_menu->set_name ("ArdourContextMenu");

		RadioMenuItem::Group align_group;
		
		alignment_items.push_back (RadioMenuElem (align_group, _("Align with existing material"), bind (mem_fun(*this, &AudioTimeAxisView::set_align_style), ExistingMaterial)));
		align_existing_item = dynamic_cast<RadioMenuItem*>(&alignment_items.back());
		if (get_diskstream()->alignment_style() == ExistingMaterial) {
			align_existing_item->set_active();
		}
		alignment_items.push_back (RadioMenuElem (align_group, _("Align with capture time"), bind (mem_fun(*this, &AudioTimeAxisView::set_align_style), CaptureTime)));
		align_capture_item = dynamic_cast<RadioMenuItem*>(&alignment_items.back());
		if (get_diskstream()->alignment_style() == CaptureTime) {
			align_capture_item->set_active();
		}
		
		items.push_back (MenuElem (_("Alignment"), *alignment_menu));

		get_diskstream()->AlignmentStyleChanged.connect (mem_fun(*this, &AudioTimeAxisView::align_style_changed));
	}

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Active"), mem_fun(*this, &RouteUI::toggle_route_active)));
	route_active_menu_item = dynamic_cast<CheckMenuItem *> (&items.back());
	route_active_menu_item->set_active (_route->active());

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), mem_fun(*this, &RouteUI::remove_this_route)));

}

void
AudioTimeAxisView::toggle_waveforms ()
{
	AudioStreamView* asv = audio_view();
	assert(asv);

	if (asv && waveform_item && !ignore_toggle) {
		asv->set_show_waveforms (waveform_item->get_active());
	}
}

void
AudioTimeAxisView::set_show_waveforms (bool yn)
{
	AudioStreamView* asv = audio_view();
	assert(asv);

	if (waveform_item) {
		waveform_item->set_active (yn);
	} else {
		asv->set_show_waveforms (yn);
	}
}

void
AudioTimeAxisView::set_show_waveforms_recording (bool yn)
{
	AudioStreamView* asv = audio_view();

	if (asv) {
		asv->set_show_waveforms_recording (yn);
	}
}

void
AudioTimeAxisView::set_waveform_shape (WaveformShape shape)
{
	AudioStreamView* asv = audio_view();

	if (asv) {
		asv->set_waveform_shape (shape);
	}

	map_frozen ();
}	

void
AudioTimeAxisView::set_selected_regionviews (RegionSelection& regions)
{
	AudioStreamView* asv = audio_view();

	if (asv) {
		asv->set_selected_regionviews (regions);
	}
}

void
AudioTimeAxisView::add_gain_automation_child ()
{
	XMLProperty* prop;
	AutomationLine* line;

	gain_track = new GainAutomationTimeAxisView (_session,
						     _route,
						     editor,
						     *this,
						     parent_canvas,
						     _("gain"),
						     _route->gain_automation_curve());
	
	line = new AutomationGainLine ("automation gain",
				       _session,
				       *gain_track,
				       *gain_track->canvas_display,
				       _route->gain_automation_curve());

	line->set_line_color (color_map[cAutomationLine]);
	

	gain_track->add_line (*line);

	add_child (gain_track);

	gain_track->Hiding.connect (mem_fun(*this, &AudioTimeAxisView::gain_hidden));

	bool hideit = true;
	
	XMLNode* node;

	if ((node = gain_track->get_state_node()) != 0) {
		if  ((prop = node->property ("shown")) != 0) {
			if (prop->value() == "yes") {
				hideit = false;
			}
		} 
	}

	if (hideit) {
		gain_track->hide ();
	}
}

void
AudioTimeAxisView::add_pan_automation_child ()
{
	XMLProperty* prop;

	pan_track = new PanAutomationTimeAxisView (_session, _route, editor, *this, parent_canvas, _("pan"));

	update_pans ();
	
	add_child (pan_track);

	pan_track->Hiding.connect (mem_fun(*this, &AudioTimeAxisView::pan_hidden));

	ensure_xml_node ();
	bool hideit = true;
	
	XMLNode* node;

	if ((node = pan_track->get_state_node()) != 0) {
		if ((prop = node->property ("shown")) != 0) {
			if (prop->value() == "yes") {
				hideit = false;
			}
		} 
	}

	if (hideit) {
		pan_track->hide ();
	}
}

void
AudioTimeAxisView::update_pans ()
{
	Panner::iterator p;
	
	pan_track->clear_lines ();
	
	/* we don't draw lines for "greater than stereo" panning.
	 */

	if (_route->n_outputs() > 2) {
		return;
	}

	for (p = _route->panner().begin(); p != _route->panner().end(); ++p) {

		AutomationLine* line;

		line = new AutomationPanLine ("automation pan", _session, *pan_track,
					      *pan_track->canvas_display, 
					      (*p)->automation());

		if (p == _route->panner().begin()) {
			/* first line is a nice orange */
			line->set_line_color (color_map[cLeftPanAutomationLine]);
		} else {
			/* second line is a nice blue */
			line->set_line_color (color_map[cRightPanAutomationLine]);
		}

		pan_track->add_line (*line);
	}
}
		
void
AudioTimeAxisView::toggle_gain_track ()
{

	bool showit = gain_automation_item->get_active();

	if (showit != gain_track->marked_for_display()) {
		if (showit) {
			gain_track->set_marked_for_display (true);
			gain_track->canvas_display->show();
			gain_track->get_state_node()->add_property ("shown", X_("yes"));
		} else {
			gain_track->set_marked_for_display (false);
			gain_track->hide ();
			gain_track->get_state_node()->add_property ("shown", X_("no"));
		}

		/* now trigger a redisplay */
		
		if (!no_redraw) {
			 _route->gui_changed (X_("track_height"), (void *) 0); /* EMIT_SIGNAL */
		}
	}
}

void
AudioTimeAxisView::gain_hidden ()
{
	gain_track->get_state_node()->add_property (X_("shown"), X_("no"));

	if (gain_automation_item && !_hidden) {
		gain_automation_item->set_active (false);
	}

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::toggle_pan_track ()
{
	bool showit = pan_automation_item->get_active();

	if (showit != pan_track->marked_for_display()) {
		if (showit) {
			pan_track->set_marked_for_display (true);
			pan_track->canvas_display->show();
			pan_track->get_state_node()->add_property ("shown", X_("yes"));
		} else {
			pan_track->set_marked_for_display (false);
			pan_track->hide ();
			pan_track->get_state_node()->add_property ("shown", X_("no"));
		}

		/* now trigger a redisplay */
		
		if (!no_redraw) {
			 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
		}
	}
}

void
AudioTimeAxisView::pan_hidden ()
{
	pan_track->get_state_node()->add_property ("shown", "no");

	if (pan_automation_item && !_hidden) {
		pan_automation_item->set_active (false);
	}

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

AudioTimeAxisView::RedirectAutomationInfo::~RedirectAutomationInfo ()
{
	for (vector<RedirectAutomationNode*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		delete *i;
	}
}


AudioTimeAxisView::RedirectAutomationNode::~RedirectAutomationNode ()
{
	parent.remove_ran (this);

	if (view) {
		delete view;
	}
}

void
AudioTimeAxisView::remove_ran (RedirectAutomationNode* ran)
{
	if (ran->view) {
		remove_child (ran->view);
	}
}

AudioTimeAxisView::RedirectAutomationNode*
AudioTimeAxisView::find_redirect_automation_node (boost::shared_ptr<Redirect> redirect, uint32_t what)
{
	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {

		if ((*i)->redirect == redirect) {

			for (vector<RedirectAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
				if ((*ii)->what == what) {
					return *ii;
				}
			}
		}
	}

	return 0;
}

// FIXME: duplicated in midi_time_axis.cc
static string 
legalize_for_xml_node (string str)
{
	string::size_type pos;
	string legal_chars = "abcdefghijklmnopqrtsuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_+=:";
	string legal;

	legal = str;
	pos = 0;

	while ((pos = legal.find_first_not_of (legal_chars, pos)) != string::npos) {
		legal.replace (pos, 1, "_");
		pos += 1;
	}

	return legal;
}


void
AudioTimeAxisView::add_redirect_automation_curve (boost::shared_ptr<Redirect> redirect, uint32_t what)
{
	RedirectAutomationLine* ral;
	string name;
	RedirectAutomationNode* ran;

	if ((ran = find_redirect_automation_node (redirect, what)) == 0) {
		fatal << _("programming error: ")
		      << string_compose (X_("redirect automation curve for %1:%2 not registered with audio track!"),
				  redirect->name(), what)
		      << endmsg;
		/*NOTREACHED*/
		return;
	}

	if (ran->view) {
		return;
	}

	name = redirect->describe_parameter (what);

	/* create a string that is a legal XML node name that can be used to refer to this redirect+port combination */

	char state_name[256];
	snprintf (state_name, sizeof (state_name), "Redirect-%s-%" PRIu32, legalize_for_xml_node (redirect->name()).c_str(), what);

	ran->view = new RedirectAutomationTimeAxisView (_session, _route, editor, *this, parent_canvas, name, what, *redirect, state_name);

	ral = new RedirectAutomationLine (name, 
					  *redirect, what, _session, *ran->view,
					  *ran->view->canvas_display, redirect->automation_list (what));
	
	ral->set_line_color (color_map[cRedirectAutomationLine]);
	ral->queue_reset ();

	ran->view->add_line (*ral);

	ran->view->Hiding.connect (bind (mem_fun(*this, &AudioTimeAxisView::redirect_automation_track_hidden), ran, redirect));

	if (!ran->view->marked_for_display()) {
		ran->view->hide ();
	} else {
		ran->menu_item->set_active (true);
	}

	add_child (ran->view);

	audio_view()->foreach_regionview (bind (mem_fun(*this, &AudioTimeAxisView::add_ghost_to_redirect), ran->view));

	redirect->mark_automation_visible (what, true);
}

void
AudioTimeAxisView::redirect_automation_track_hidden (AudioTimeAxisView::RedirectAutomationNode* ran, boost::shared_ptr<Redirect> r)
{
	if (!_hidden) {
		ran->menu_item->set_active (false);
	}

	r->mark_automation_visible (ran->what, false);

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::add_existing_redirect_automation_curves (boost::shared_ptr<Redirect> redirect)
{
	set<uint32_t> s;
	RedirectAutomationLine *ral;

	redirect->what_has_visible_automation (s);

	for (set<uint32_t>::iterator i = s.begin(); i != s.end(); ++i) {
		
		if ((ral = find_redirect_automation_curve (redirect, *i)) != 0) {
			ral->queue_reset ();
		} else {
			add_redirect_automation_curve (redirect, (*i));
		}
	}
}

void
AudioTimeAxisView::add_redirect_to_subplugin_menu (boost::shared_ptr<Redirect> r)
{
	using namespace Menu_Helpers;
	RedirectAutomationInfo *rai;
	list<RedirectAutomationInfo*>::iterator x;
	
	const std::set<uint32_t>& automatable = r->what_can_be_automated ();
	std::set<uint32_t> has_visible_automation;

	r->what_has_visible_automation(has_visible_automation);

	if (automatable.empty()) {
		return;
	}

	for (x = redirect_automation.begin(); x != redirect_automation.end(); ++x) {
		if ((*x)->redirect == r) {
			break;
		}
	}

	if (x == redirect_automation.end()) {

		rai = new RedirectAutomationInfo (r);
		redirect_automation.push_back (rai);

	} else {

		rai = *x;

	}

	/* any older menu was deleted at the top of redirects_changed()
	   when we cleared the subplugin menu.
	*/

	rai->menu = manage (new Menu);
	MenuList& items = rai->menu->items();
	rai->menu->set_name ("ArdourContextMenu");

	items.clear ();

	for (std::set<uint32_t>::const_iterator i = automatable.begin(); i != automatable.end(); ++i) {

		RedirectAutomationNode* ran;
		CheckMenuItem* mitem;
		
		string name = r->describe_parameter (*i);
		
		items.push_back (CheckMenuElem (name));
		mitem = dynamic_cast<CheckMenuItem*> (&items.back());

		if (has_visible_automation.find((*i)) != has_visible_automation.end()) {
			mitem->set_active(true);
		}

		if ((ran = find_redirect_automation_node (r, *i)) == 0) {

			/* new item */
			
			ran = new RedirectAutomationNode (*i, mitem, *this);
			
			rai->lines.push_back (ran);

		} else {

			ran->menu_item = mitem;

		}

		mitem->signal_toggled().connect (bind (mem_fun(*this, &AudioTimeAxisView::redirect_menu_item_toggled), rai, ran));
	}

	/* add the menu for this redirect, because the subplugin
	   menu is always cleared at the top of redirects_changed().
	   this is the result of some poor design in gtkmm and/or
	   GTK+.
	*/

	subplugin_menu.items().push_back (MenuElem (r->name(), *rai->menu));
	rai->valid = true;
}

void
AudioTimeAxisView::redirect_menu_item_toggled (AudioTimeAxisView::RedirectAutomationInfo* rai,
					       AudioTimeAxisView::RedirectAutomationNode* ran)
{
	bool showit = ran->menu_item->get_active();
	bool redraw = false;

	if (ran->view == 0 && showit) {
		add_redirect_automation_curve (rai->redirect, ran->what);
		redraw = true;
	}

	if (showit != ran->view->marked_for_display()) {

		if (showit) {
			ran->view->set_marked_for_display (true);
			ran->view->canvas_display->show();
		} else {
			rai->redirect->mark_automation_visible (ran->what, true);
			ran->view->set_marked_for_display (false);
			ran->view->hide ();
		}

		redraw = true;

	}

	if (redraw && !no_redraw) {

		/* now trigger a redisplay */
		
		 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */

	}
}

void
AudioTimeAxisView::redirects_changed (void *src)
{
	using namespace Menu_Helpers;

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		(*i)->valid = false;
	}

	subplugin_menu.items().clear ();

	_route->foreach_redirect (this, &AudioTimeAxisView::add_redirect_to_subplugin_menu);
	_route->foreach_redirect (this, &AudioTimeAxisView::add_existing_redirect_automation_curves);

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ) {

		list<RedirectAutomationInfo*>::iterator tmp;

		tmp = i;
		++tmp;

		if (!(*i)->valid) {

			delete *i;
			redirect_automation.erase (i);

		} 

		i = tmp;
	}

	/* change in visibility was possible */

	_route->gui_changed ("track_height", this);
}

RedirectAutomationLine *
AudioTimeAxisView::find_redirect_automation_curve (boost::shared_ptr<Redirect> redirect, uint32_t what)
{
	RedirectAutomationNode* ran;

	if ((ran = find_redirect_automation_node (redirect, what)) != 0) {
		if (ran->view) {
			return dynamic_cast<RedirectAutomationLine*> (ran->view->lines.front());
		} 
	}

	return 0;
}

void
AudioTimeAxisView::show_all_automation ()
{
	no_redraw = true;

	pan_automation_item->set_active (true);
	gain_automation_item->set_active (true);
	
	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		for (vector<RedirectAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			if ((*ii)->view == 0) {
				add_redirect_automation_curve ((*i)->redirect, (*ii)->what);
			} 

			(*ii)->menu_item->set_active (true);
		}
	}

	no_redraw = false;

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::show_existing_automation ()
{
	no_redraw = true;

	pan_automation_item->set_active (true);
	gain_automation_item->set_active (true);

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		for (vector<RedirectAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			if ((*ii)->view != 0) {
				(*ii)->menu_item->set_active (true);
			}
		}
	}

	no_redraw = false;

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::hide_all_automation ()
{
	no_redraw = true;

	pan_automation_item->set_active (false);
	gain_automation_item->set_active (false);

	for (list<RedirectAutomationInfo*>::iterator i = redirect_automation.begin(); i != redirect_automation.end(); ++i) {
		for (vector<RedirectAutomationNode*>::iterator ii = (*i)->lines.begin(); ii != (*i)->lines.end(); ++ii) {
			(*ii)->menu_item->set_active (false);
		}
	}

	no_redraw = false;
	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::region_view_added (RegionView* rv)
{
	assert(dynamic_cast<AudioRegionView*>(rv));

	for (vector<TimeAxisView*>::iterator i = children.begin(); i != children.end(); ++i) {
		AutomationTimeAxisView* atv;

		if ((atv = dynamic_cast<AutomationTimeAxisView*> (*i)) != 0) {
			rv->add_ghost (*atv);
		}
	}
}

void
AudioTimeAxisView::add_ghost_to_redirect (RegionView* rv, AutomationTimeAxisView* atv)
{
	rv->add_ghost (*atv);
}

void
AudioTimeAxisView::show_all_xfades ()
{
	AudioStreamView* asv = audio_view();

	if (asv) {
		asv->show_all_xfades ();
	}
}

void
AudioTimeAxisView::hide_all_xfades ()
{
	AudioStreamView* asv = audio_view();
	
	if (asv) {
		asv->hide_all_xfades ();
	}
}

void
AudioTimeAxisView::hide_dependent_views (TimeAxisViewItem& tavi)
{
	AudioStreamView* asv = audio_view();
	AudioRegionView* rv;

	if (asv && (rv = dynamic_cast<AudioRegionView*>(&tavi)) != 0) {
		asv->hide_xfades_involving (*rv);
	}
}

void
AudioTimeAxisView::reveal_dependent_views (TimeAxisViewItem& tavi)
{
	AudioStreamView* asv = audio_view();
	AudioRegionView* rv;

	if (asv && (rv = dynamic_cast<AudioRegionView*>(&tavi)) != 0) {
		asv->reveal_xfades_involving (*rv);
	}
}

void
AudioTimeAxisView::route_active_changed ()
{
	RouteUI::route_active_changed ();

	if (is_audio_track()) {
		if (_route->active()) {
			controls_ebox.set_name ("AudioTrackControlsBaseUnselected");
			controls_base_selected_name = "AudioTrackControlsBaseSelected";
			controls_base_unselected_name = "AudioTrackControlsBaseUnselected";
		} else {
			controls_ebox.set_name ("AudioTrackControlsBaseInactiveUnselected");
			controls_base_selected_name = "AudioTrackControlsBaseInactiveSelected";
			controls_base_unselected_name = "AudioTrackControlsBaseInactiveUnselected";
		}
	} else {
		if (_route->active()) {
			controls_ebox.set_name ("BusControlsBaseUnselected");
			controls_base_selected_name = "BusControlsBaseSelected";
			controls_base_unselected_name = "BusControlsBaseUnselected";
		} else {
			controls_ebox.set_name ("BusControlsBaseInactiveUnselected");
			controls_base_selected_name = "BusControlsBaseInactiveSelected";
			controls_base_unselected_name = "BusControlsBaseInactiveUnselected";
		}
	}
}

XMLNode* 
AudioTimeAxisView::get_child_xml_node (const string & childname)
{
	return RouteUI::get_child_xml_node (childname);
}

