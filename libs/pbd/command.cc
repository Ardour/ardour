#include <pbd/command.h>
#include <pbd/xml++.h>


XMLNode &Command::get_state()
{
    XMLNode *node = new XMLNode ("Command");
    node->add_content("WARNING: Somebody forgot to subclass Command.");
    return *node;
}
