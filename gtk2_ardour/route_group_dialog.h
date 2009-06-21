#ifndef __gtk_ardour_route_group_dialog_h__
#define __gtk_ardour_route_group_dialog_h__

#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/checkbutton.h>

class RouteGroupDialog : public Gtk::Dialog
{
public:
	RouteGroupDialog (ARDOUR::RouteGroup *, Gtk::StockID const &);

	int do_run ();

private:	
	ARDOUR::RouteGroup* _group;
	Gtk::Entry _name;
	Gtk::CheckButton _active;
	Gtk::CheckButton _gain;
	Gtk::CheckButton _mute;
	Gtk::CheckButton _solo;
	Gtk::CheckButton _rec_enable;
	Gtk::CheckButton _select;
	Gtk::CheckButton _edit;
};


#endif
