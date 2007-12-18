#include <gtkmm.h>
#include <gtk/gtkdialog.h>
#include <iostream>
#include <list>


class MyDialog : public Gtk::Dialog {
public:
  MyDialog()
   {
     add_button("Ok", 0);
   }
};

class MyWindow : public Gtk::Window
{
public:
  MyWindow();

  void on_button_clicked();

protected:
  Gtk::HBox m_Box;
  Gtk::Button m_Button;
};

MyWindow::MyWindow()
: m_Button("Show Dialog")
{
  set_size_request(200, 200);

  m_Button.signal_clicked().connect( sigc::mem_fun(*this, &MyWindow::on_button_clicked) );
  m_Box.pack_start(m_Button);
  add(m_Box);
}

void MyWindow::on_button_clicked()
{
  {
    MyDialog d;
    d.run();
    std::cout << "After d.run()" << std::endl;
  }

   std::cout << "before list_toplevel" << std::endl;
  std::list<Gtk::Window*> toplevelwindows = list_toplevels();
   std::cout << "after list_toplevel" << std::endl;

  std::cout << "toplevelwindows.size = " << toplevelwindows.size() << std::endl;
}

int main(int argc, char* argv[])
{
  Gtk::Main kit(argc, argv);

  MyWindow win;
  win.show_all();
  kit.run(win);
  return 0;
}
