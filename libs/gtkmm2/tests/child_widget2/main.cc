#include <gtkmm.h>

class MyWindow : public Gtk::Window
{
public:
  MyWindow();
private:
  Gtk::Button b;
  Gtk::VBox vbox;
};

MyWindow::MyWindow() : 
  b("hello"),
  vbox()
{
  add(vbox);
  vbox.pack_start(b);
  show_all_children();
}

int main (int argc, char *argv[])
{
  Gtk::Main kit(argc, argv);
  
  MyWindow window;
  kit.run(window);

  return 0;
}
