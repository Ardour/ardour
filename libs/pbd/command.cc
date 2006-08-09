#include <pbd/command.h>
#include <pbd/xml++.h>


XMLNode &Command::get_state()
{
    XMLNode *node = new XMLNode ("Command");
    // TODO
    return *node;
}
