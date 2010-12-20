#include <iostream>

#include <glibmm/regex.h>

#include "ardour/midi_patch_manager.h"
#include "ardour_ui.h"
#include "midi_region_view.h"
#include "canvas-program-change.h"
#include "editor.h"
#include "editor_drag.h"

using namespace Gnome::Canvas;
using namespace MIDI::Name;
using namespace std;

/** @param x x position in pixels.
 *  @param event_time PC time in beats, with respect to the start of the source.
 */
CanvasProgramChange::CanvasProgramChange(
		MidiRegionView& region,
		Group&          parent,
		const string&   text,
		double          height,
		double          x,
		double          y,
		string&         model_name,
		string&         custom_device_mode,
		double          event_time,
		uint8_t         channel,
		uint8_t         program)
	: CanvasFlag(
			region,
			parent,
			height,
			ARDOUR_UI::config()->canvasvar_MidiProgramChangeOutline.get(),
			ARDOUR_UI::config()->canvasvar_MidiProgramChangeFill.get(),
			x,
			y)
	 , _model_name(model_name)
	 , _custom_device_mode(custom_device_mode)
	 , _event_time(event_time)
	 , _channel(channel)
	 , _program(program)
	 , _popup_initialized(false)
{
	set_text(text);
}

CanvasProgramChange::~CanvasProgramChange()
{
}

void
CanvasProgramChange::initialize_popup_menus()
{
	boost::shared_ptr<ChannelNameSet> channel_name_set =
		MidiPatchManager::instance()
			.find_channel_name_set(_model_name, _custom_device_mode, _channel);

	if (!channel_name_set) {
		return;
	}

	const ChannelNameSet::PatchBanks& patch_banks = channel_name_set->patch_banks();

	// fill popup menu:
	Gtk::Menu::MenuList& patch_bank_menus = _popup.items();

	for (ChannelNameSet::PatchBanks::const_iterator bank = patch_banks.begin();
	     bank != patch_banks.end();
	     ++bank) {
		Glib::RefPtr<Glib::Regex> underscores = Glib::Regex::create("_");
		std::string replacement(" ");

		Gtk::Menu& patch_bank_menu = *manage(new Gtk::Menu());

		const PatchBank::PatchNameList& patches = (*bank)->patch_name_list();
		Gtk::Menu::MenuList& patch_menus = patch_bank_menu.items();

		for (PatchBank::PatchNameList::const_iterator patch = patches.begin();
		     patch != patches.end();
		     ++patch) {
			std::string name = underscores->replace((*patch)->name().c_str(), -1, 0, replacement);

			patch_menus.push_back(
				Gtk::Menu_Helpers::MenuElem(
					name,
					sigc::bind(
						sigc::mem_fun(*this, &CanvasProgramChange::on_patch_menu_selected),
						(*patch)->patch_primary_key())) );
		}


		std::string name = underscores->replace((*bank)->name().c_str(), -1, 0, replacement);

		patch_bank_menus.push_back(
			Gtk::Menu_Helpers::MenuElem(
				name,
				patch_bank_menu) );
	}
}

void
CanvasProgramChange::on_patch_menu_selected(const PatchPrimaryKey& key)
{
	cerr << " got patch program number " << key.program_number << endl;
	_region.program_selected(*this, key);
}

bool
CanvasProgramChange::on_event(GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		switch (ev->button.button) {
		case 1:
		{
			/* XXX: icky dcast */
			Editor* e = dynamic_cast<Editor*> (&_region.get_time_axis_view().editor());
			if (e->current_mouse_mode() == Editing::MouseObject && e->internal_editing()) {
				e->drags()->set (new ProgramChangeDrag (e, this, &_region), ev);
				return true;
			}
		}
		case 3:
			// lazy init
			if (!_popup_initialized) {
				initialize_popup_menus();
				_popup_initialized = true;
			}
			_popup.popup(ev->button.button, ev->button.time);
			return true;
		}
		break;

	case GDK_KEY_PRESS:
		switch (ev->key.keyval) {
		case GDK_Up:
		case GDK_KP_Up:
		case GDK_uparrow:
			_region.previous_program(*this);
			break;
		case GDK_Down:
		case GDK_KP_Down:
		case GDK_downarrow:
			_region.next_program(*this);
			break;
		default:
			break;
		}
		break;

	case GDK_SCROLL:
		if (ev->scroll.direction == GDK_SCROLL_UP) {
			_region.previous_program(*this);
			return true;
		} else if (ev->scroll.direction == GDK_SCROLL_DOWN) {
			_region.next_program(*this);
			return true;
		}
		break;

	default:
		break;
	}

	return false;
}
