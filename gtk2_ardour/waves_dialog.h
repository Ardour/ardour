/*
	Copyright (C) 2002 Paul Davis

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

#ifndef __waves_dialog_h__
#define __waves_dialog_h__

#include <gtkmm.h>

#include "ardour/session_handle.h"

namespace WM {
	class ProxyTemporary;
}

class WavesButton;
class XMLNode;

/*
 * This virtual parent class is so that each dialog box uses the
 * same mechanism to declare its closing. It shares a common
 * method of connecting and disconnecting from a Session with
 * all other objects that have a handle on a Session.
 */
class WavesDialog : public Gtk::Dialog, public ARDOUR::SessionHandlePtr
{
  public:

	WavesDialog (std::string layout_script, bool modal = false, bool use_separator = false);
	~WavesDialog();

	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);
	bool on_delete_event (GdkEventAny*);
	bool on_key_press_event (GdkEventKey*);
	void on_unmap ();
	void on_show ();

  protected:

	  void set_layout_size (int width, int height);
	  bool read_layout (std::string file_name);

	  Gtk::Layout& get_layout (char* id);
	  Gtk::Label& get_label (char* id);
	  Gtk::ComboBoxText& get_combo_box_text (char* id);
	  WavesButton& get_waves_button (char* id);

  private:

	WM::ProxyTemporary* proxy;
	bool _splash_pushed;
	
	std::map<std::string, Gtk::Widget*> _children;
	
	Gtk::Widget* get_widget(char *id);

	static sigc::signal<void> CloseAllDialogs;

	// Layout
	Gtk::Layout parent;

	Gtk::Widget* create_widget (const XMLNode& definition);
	Gtk::Widget* add_widget (Gtk::Layout& parent, const XMLNode &definition);
	Gtk::Widget* add_widget (Gtk::Widget& parent, const XMLNode &definition);

	Gtk::Label& add_label (const std::string& label, int x, int y, int width = -1, int height = -1);
	Gtk::ComboBoxText& add_combo_box_text (int x, int y, int width = -1, int height = -1);
	WavesButton& add_button (const std::string& label, const std::string name, int x, int y, int width = -1, int height = -1);

	double xml_property (const XMLNode& node, const char* prop_name, double default_value);
	int xml_property (const XMLNode& node, const char* prop_name, int default_value);
	bool xml_property (const XMLNode& node, const char* prop_name, bool default_value);
	std::string xml_property (const XMLNode& node, const char* prop_name, const std::string default_value);
	std::string xml_property (const XMLNode& node, const char* prop_name, const char* default_value) { return xml_property (node, prop_name, std::string(default_value)); };
};

#endif // __waves_dialog_h__

