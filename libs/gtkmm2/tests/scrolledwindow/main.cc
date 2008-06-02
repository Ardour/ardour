#include <gtkmm.h>
#include <gtk/gtk.h>
#include <stdio.h>

void on_viewport_destroyed(GtkWidget* widget, gpointer)
{
  GTK_VIEWPORT(widget);
  g_warning("viewport destroyed");
}

void on_gtklabel_destroyed(GtkWidget*, gpointer)
{
  g_warning("gtklabel destroyed");
}


class DerivedLabel : public Gtk::Label
{
public:
  DerivedLabel() {}
  virtual ~DerivedLabel() { printf("~DerivedLabel\n");}
};

class DerivedScrolledWindow : public Gtk::ScrolledWindow
{
public:
  DerivedScrolledWindow() {}
  virtual ~DerivedScrolledWindow() { printf("~DerivedScrolledWindow\n");}
};

class Instance
{
public:
  Instance()
  {
    //m_pLabel = new DerivedLabel();
    m_ScrolledWindow.add( m_pLabel );

    g_signal_connect (G_OBJECT(m_ScrolledWindow.get_child()->gobj()), "destroy", G_CALLBACK(on_viewport_destroyed), NULL);

   g_signal_connect (G_OBJECT(m_pLabel.gobj()), "destroy", G_CALLBACK(on_gtklabel_destroyed), NULL);
    
  }
 
  virtual ~Instance()
  {
    printf("~Instance() 1\n");
    //m_ScrolledWindow.remove(); //Removes m_pLabel;
    printf("~Instance() 2\n");
    //delete m_pLabel;
    printf("~Instance() 3\n");
  }

protected:
  DerivedLabel m_pLabel;
  DerivedScrolledWindow m_ScrolledWindow;
};

int main ( int argc, char *argv[] ) {
  
  Gtk::Main main( argc, argv );

  Instance instance;

}
