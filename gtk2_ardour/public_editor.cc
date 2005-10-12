#include "public_editor.h"
#include "editor.h"

PublicEditor* PublicEditor::_instance = 0;

PublicEditor::PublicEditor ()
  	: Window (Gtk::WINDOW_TOPLEVEL),
	  KeyboardTarget (*this, "editor")
{
}

PublicEditor::~PublicEditor()
{
}

