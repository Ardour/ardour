#include <gtkmm.h>


GQuark quark_test = 0;

void initialize_quark()
{
  if(!quark_test)
  {
    //g_warning("initializing quark.");
    quark_test = g_quark_from_static_string("quarktestmurrayc");
  }
}

void on_object_qdata_destroyed(gpointer data)
{
  //This doesn't seem to be called:
  g_warning("on_object_qdata_destroyed():  c instance=%p", (void*)data);
}

int main(int argc, char**argv)
{
  Gtk::Main app(&argc, &argv);

  Gtk::Dialog* pDialog = new Gtk::Dialog();
  Gtk::VBox* pBox = pDialog->get_vbox();

  //Set a quark and a callback:
  initialize_quark();
  int a = 0; // (This doesn't work unless we have a non-null value for the 3rd parameter.)
  g_object_set_qdata_full((GObject*)pBox->gobj(), quark_test, &a, &on_object_qdata_destroyed); 

  g_warning("vbox refcount=%d", G_OBJECT(pBox->gobj())->ref_count);

  delete pDialog;

  g_warning("after delete");
}
