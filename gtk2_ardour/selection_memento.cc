#include "selection_memento.h"
#include "editing.h"
#include "public_editor.h"

#include "i18n.h"

SelectionMemento::SelectionMemento ()
{

}

SelectionMemento::~SelectionMemento ()
{

}

XMLNode&
SelectionMemento::get_state () {

	XMLNode* node = new XMLNode ("SelectionMemento");
	char buf[32];

	id().print (buf, sizeof (buf));
	node->add_property ("id", buf);
	snprintf (buf, sizeof (buf), "%" PRIi64, PublicEditor::instance().leftmost_sample());
	node->add_property ("left-frame", buf);
	snprintf (buf, sizeof (buf), "%f", PublicEditor::instance().get_y_origin());
	node->add_property ("y-origin", buf);
	node->add_property ("mouse-mode", enum2str(PublicEditor::instance().current_mouse_mode()));

	node->add_child_nocopy (PublicEditor::instance().get_selection().get_state());
	return *node;
}

int
SelectionMemento::set_state (const XMLNode& node, int /*version*/) {

	const XMLProperty* prop;

	if (node.name() != X_("SelectionMemento")) {
		return -1;
	}

	set_id (node);

	if ((prop = node.property ("mouse-mode"))) {
		Editing::MouseMode m = Editing::str2mousemode(prop->value());
		PublicEditor::instance().set_mouse_mode (m, true);
	} else {
		PublicEditor::instance().set_mouse_mode (Editing::MouseObject, true);
	}

	if ((prop = node.property ("left-frame")) != 0) {
		framepos_t pos;
		if (sscanf (prop->value().c_str(), "%" PRId64, &pos) == 1) {
			if (pos < 0) {
				pos = 0;
			}
			PublicEditor::instance().reset_x_origin (pos);
		}
	}

	if ((prop = node.property ("y-origin")) != 0) {
		PublicEditor::instance().reset_y_origin (atof (prop->value ().c_str()));
	}

	XMLNodeList children = node.children ();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		PublicEditor::instance().get_selection().set_state (**i, Stateful::current_state_version);
	}

	return 0;
}
