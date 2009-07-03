#include "editor_component.h"

class EditorRouteGroups : public EditorComponent
{
public:
	EditorRouteGroups (Editor *);

	void connect_to_session (ARDOUR::Session *);

	Gtk::Widget& widget () {
		return *_display_packer;
	}

	Gtk::Menu* menu (ARDOUR::RouteGroup *);

	void clear ();
	
private:
	
        struct Columns : public Gtk::TreeModel::ColumnRecord {

                Columns () {
			add (is_visible);
			add (gain);
			add (record);
			add (mute);
			add (solo);
			add (select);
			add (edits);
			add (text);
			add (routegroup);
                }

	        Gtk::TreeModelColumn<bool> is_visible;
		Gtk::TreeModelColumn<bool> gain;
		Gtk::TreeModelColumn<bool> record;
		Gtk::TreeModelColumn<bool> mute;
		Gtk::TreeModelColumn<bool> solo;
		Gtk::TreeModelColumn<bool> select;
		Gtk::TreeModelColumn<bool> edits;
	        Gtk::TreeModelColumn<std::string> text;
	        Gtk::TreeModelColumn<ARDOUR::RouteGroup*> routegroup;
	};

	Columns _columns;

	void activate_all ();
	void disable_all ();
	void subgroup (ARDOUR::RouteGroup*);
	void unsubgroup (ARDOUR::RouteGroup*);

	void row_change (const Gtk::TreeModel::Path&,const Gtk::TreeModel::iterator&);
	void name_edit (const Glib::ustring&, const Glib::ustring&);
	void new_route_group ();
	void new_from_selection ();
	void new_from_rec_enabled ();
	void new_from_soloed ();
	void edit (ARDOUR::RouteGroup *);
	void button_clicked ();
	gint button_press_event (GdkEventButton* ev);
	void add (ARDOUR::RouteGroup* group);
	void remove_route_group ();
	void groups_changed ();
	void flags_changed (void*, ARDOUR::RouteGroup*);
	void set_activation (ARDOUR::RouteGroup *, bool);
	void remove_selected ();

	Gtk::Menu* _menu;
	Glib::RefPtr<Gtk::ListStore> _model;
	Glib::RefPtr<Gtk::TreeSelection> _selection;
	Gtk::TreeView _display;
	Gtk::ScrolledWindow _scroller;
	Gtk::VBox* _display_packer;
	bool _in_row_change;
};


