namespace ARDOUR {
	class Session;
	class RouteGroup;
}

class RouteGroupMenu : public Gtk::Menu
{
public:
	RouteGroupMenu (ARDOUR::Session &);

	void rebuild (ARDOUR::RouteGroup *);

	sigc::signal<void, ARDOUR::RouteGroup*> GroupSelected;
	
private:
	void add_item (ARDOUR::RouteGroup *, ARDOUR::RouteGroup *, Gtk::RadioMenuItem::Group*);
	void new_group ();
	void set_group (ARDOUR::RouteGroup *);
	
	ARDOUR::Session& _session;
};
