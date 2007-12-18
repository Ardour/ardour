#include <gtkmm.h>

class MyButton : public Gtk::Button
{
public:
    MyButton();
    virtual ~MyButton();
};

MyButton::MyButton()
: Gtk::Button("Ok", true)
{ }

MyButton::~MyButton()
{
    g_warning("MyButtom::~MyButton()");
}

class ExampleWindow : public Gtk::Window
{
public:
    ExampleWindow();
    virtual ~ExampleWindow();

protected:

    MyButton* m_button;
};

ExampleWindow::ExampleWindow()
{
    set_default_size(150, 150);

    m_button = manage(new MyButton);
    add(*m_button);
	
    show_all_children();
}

ExampleWindow::~ExampleWindow()
{
  g_warning("ExampleWindow::~ExampleWindow()");
}


int main(int argc, char* argv[])
{
    Gtk::Main kit(argc, argv);
    ExampleWindow window;
    kit.run(window);
    return 0;
}
