#ifndef __gtk_ardour_route_group_dialog_h__
#define __gtk_ardour_route_group_dialog_h__

#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/checkbutton.h>

class RouteGroupDialog : public Gtk::Dialog
{
public:
	RouteGroupDialog (ARDOUR::RouteGroup *);

	int do_run ();

private:	
	ARDOUR::RouteGroup* _group;
	Gtk::Entry _name;
	Gtk::CheckButton _active;
};


#endif
