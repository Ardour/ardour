#ifndef __pbd_gtkmm_choice_h__
#define __pbd_gtkmm_choice_h__

#include <gtkmm.h>
#include <vector>

namespace Gtkmm2ext {

class Choice : public Gtk::Window
{
  public:
	Choice (std::string prompt, std::vector<std::string> choices);
	virtual ~Choice ();

	/* This signal will be raised when a choice
	   is made or the choice window is deleted.
	   If the choice was to cancel, or the window
	   was deleted, then the argument will be -1.
	   Otherwise, it will be choice selected
	   of those presented, starting at zero.
	*/

	sigc::signal<void,int> choice_made;
	sigc::signal<void> chosen;

	int get_choice ();

  protected:
	void on_realize ();
	
  private:
	Gtk::VBox packer;
	Gtk::Label prompt_label;
	Gtk::HBox button_packer;
	std::vector<Gtk::Button*> buttons;
	int  which_choice;

	void _choice_made (int nbutton);
	gint closed (GdkEventAny *);
};

} /* namespace */

#endif  // __pbd_gtkmm_choice_h__
