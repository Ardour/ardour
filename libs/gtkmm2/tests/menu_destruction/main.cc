#include <gtkmm.h>
#include <iostream>

void on_destroyed_ComboBoxderived(GtkWidget*, gpointer)
{
  std::cout << "on_destroyed_ComboBoxderived" << std::endl;
}

void on_destroyed_menuderived(GtkWidget*, gpointer)
{
  std::cout << "on_destroyed_menuderived" << std::endl;
}

//Previously, this derived from a Gtk::ComboBox, 
//and that might have been necessary to trigger the bug at the time,
//but ComboBox is now deprecated.
class ComboBoxTextDerived : public Gtk::ComboBoxText
{
public:
  ComboBoxTextDerived()
  {
    g_signal_connect (gobj(), "destroy",
				G_CALLBACK (on_destroyed_ComboBoxderived), NULL);
  }
  
  ~ComboBoxTextDerived()
  {
    //remove_menu();
    std::cout << "~ComboBoxTextDerived()" << std::endl;
  }
};

class MenuDerived : public Gtk::Menu
{
public:
  MenuDerived()
  {
  g_signal_connect (gobj(), "destroy",
				G_CALLBACK (on_destroyed_menuderived), NULL);
  }
  
  ~MenuDerived()
  {
    std::cout << "~MenuDerived() 1" << std::endl;
 
    std::cout << "~MenuDerived() 2" << std::endl;

  }
};

class test_window : public Gtk::Window
{
public:
  test_window();
  ~test_window()
  {
    std::cout << "~test_window()1" << std::endl;

   }
  
protected:

  //Gtk::MenuItem m_MenuItem;
  MenuDerived m_Menu;
  ComboBoxTextDerived m_ComboBox;
};

test_window::test_window()
//: m_MenuItem("One")
{
  //  m_Menu.append(m_MenuItem);
  //m_ComboBox.set_menu(m_Menu);
  add(m_ComboBox);
  //show_all();
}

int main(int argc, char *argv[])
{
  Gtk::Main main_runner(argc, argv);
  test_window foo;
  Gtk::Main::run(foo);

  return(0);
}
